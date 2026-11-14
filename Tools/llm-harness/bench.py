"""Benchmark scripted interrogations against llama-server, using the meter engine in engine.py.

For each server config, plays every script in scripts/<case>.json and records: JSON validity, tactic-label
accuracy against the script's expected labels, bluff/gossip detection, which secrets came out, whether the
spoken reply matched, timings and video memory. Transcripts and per-turn meters go to results/.

Usage (from the repo root):
  python -I Tools/llm-harness/bench.py                        # default configs (e4b-gpu, e2b-gpu)
  python -I Tools/llm-harness/bench.py e4b-gpu e2b-cpu4       # selected configs
  python -I Tools/llm-harness/bench.py e4b-gpu --only vera_trust
"""

import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
LLAMA_TAG = "b12368"
VULKAN = ROOT / "ThirdParty" / "llama" / f"{LLAMA_TAG}-vulkan" / "llama-server.exe"
CPU = ROOT / "ThirdParty" / "llama" / f"{LLAMA_TAG}-cpu" / "llama-server.exe"
E4B = ROOT / "Models" / "gemma-4-E4B-it-Q4_K_M.gguf"
E2B = ROOT / "Models" / "gemma-4-E2B-it-Q4_K_M.gguf"
OUT = Path(__file__).resolve().parent / "results"
SCRIPTS = Path(__file__).resolve().parent / "scripts" / "spade_club.json"
PORT = 39281
MAX_TOKENS = 160

# Gemma 4 sometimes opens a "<|channel>thought" block on its own even with enable_thinking=false, burning the
# whole token budget and returning empty content. Replaying one turn 15x: --reasoning off alone 11/15 valid,
# + --reasoning-format none 13/15, + --reasoning-budget 0 15/15. Keep both flags.
COMMON = ["-c", "16384", "--parallel", "4", "--jinja", "--no-webui", "-fa", "on",
          "-ctk", "q8_0", "-ctv", "q8_0", "--reasoning", "off", "--reasoning-budget", "0"]

CONFIGS = {
    "e4b-gpu":  (VULKAN, E4B, ["-ngl", "99"]),
    "e2b-gpu":  (VULKAN, E2B, ["-ngl", "99"]),
    "e4b-cpu4": (CPU, E4B, ["-ngl", "0", "-t", "4"]),
    "e2b-cpu4": (CPU, E2B, ["-ngl", "0", "-t", "4"]),
}
DEFAULT = ["e4b-gpu", "e2b-gpu"]


# ---------- server ----------

def vram_used_mib(gpu=0):
    try:
        out = subprocess.run(["nvidia-smi", f"--id={gpu}", "--query-gpu=memory.used",
                              "--format=csv,noheader,nounits"], capture_output=True, text=True, timeout=10)
        return int(out.stdout.strip())
    except Exception:
        return None


def post(path, body, timeout=300):
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())


def wait_health(proc, timeout=240):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if proc.poll() is not None:
            return None
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{PORT}/health", timeout=2) as r:
                if r.status == 200:
                    return time.time() - t0
        except (urllib.error.URLError, ConnectionError, TimeoutError):
            pass
        time.sleep(0.5)
    return None


def chat(msgs, sch, slot, max_tokens=MAX_TOKENS):
    body = {
        "messages": msgs, "id_slot": slot, "cache_prompt": True,
        "temperature": 0.8, "max_tokens": max_tokens,
        "chat_template_kwargs": {"enable_thinking": False},
        "response_format": {"type": "json_schema", "json_schema": {"name": "reply", "schema": sch}},
    }
    t0 = time.time()
    data = post("/v1/chat/completions", body)
    return data, time.time() - t0


def parse(content, sch):
    try:
        obj = json.loads(content)
    except json.JSONDecodeError:
        return None, "not JSON"
    for k in sch["required"]:
        if k not in obj:
            return None, f"missing {k}"
    for k, p in sch["properties"].items():
        if "enum" in p and obj[k] not in p["enum"]:
            return None, f"bad {k}={obj[k]}"
    return obj, None


# ---------- play ----------

def play_script(case, script):
    cs = engine.CaseState(case)
    cs.inventory |= set(script.get("inventory_add", []))
    sid = script["suspect"]
    slot = case["suspect_defs"][sid]["slot"]
    # Pre-load this suspect's static prefix (the loading screen will do this for every suspect).
    pre, pre_s = chat([{"role": "system", "content": engine.static_system(case, sid)},
                       {"role": "user", "content": "[DETECTIVE] ..."}], engine.schema(cs, sid), slot, max_tokens=1)
    turns = []
    for step in script["turns"]:
        st = cs.suspects[sid]
        if st.closed:
            turns.append({"say": step.get("say"), "closed": True})
            continue
        presented = step.get("present")
        user_msg = engine.evidence_message(case, presented) if presented else engine.detective_message(step["say"])
        allowed = dict(engine.available_secrets(cs, sid))
        sch = engine.schema(cs, sid)
        msgs = engine.build_messages(cs, sid, user_msg)
        data, wall = chat(msgs, sch, slot)
        content = data["choices"][0]["message"].get("content") or ""
        obj, err = parse(content, sch)
        # Same as the game: an invalid reply changes nothing and never enters the history.
        ev = (engine.resolve_turn(cs, sid, user_msg, obj, content, allowed, presented=presented) if obj else
              {"deltas": {}, "revealed": None, "granted": None, "bluff": False, "gossip": None, "triggers": []})
        tm = data.get("timings", {})
        turn = {
            "say": step.get("say"), "present": presented, "expect": step.get("expect"),
            "got_t": obj and obj["t"], "c": obj and obj["c"], "g": obj and obj["g"],
            "emotion": obj and obj["e"], "reply": obj and obj["r"], "f": obj and obj["f"], "error": err,
            "raw": content if err else None,
            "finish": data["choices"][0].get("finish_reason"),
            "allowed": list(allowed), "event": ev, "meters": {k: round(v) for k, v in st.meters.items()},
            "prompt_n": tm.get("prompt_n"), "predicted_n": tm.get("predicted_n"),
            "tok_s": tm.get("predicted_per_second"), "wall_s": round(wall, 2),
        }
        if step.get("bluff"):
            turn["bluff_ok"] = ev["bluff"]
        if step.get("gossip"):
            turn["gossip_ok"] = ev["gossip"] == step["gossip"]
        turns.append(turn)
    result = {"turns": turns, "prefill_s": round(pre_s, 2),
              "prefill_tokens": pre.get("timings", {}).get("prompt_n"),
              "revealed": dict(cs.suspects[sid].revealed), "closed": cs.suspects[sid].closed,
              "inventory": sorted(cs.inventory), "cross_log": cs.log}
    if script.get("accuse_at_end"):
        result["accusation"] = engine.accuse(cs, script["accuse_at_end"])
    return result


def summarize(name, script, res):
    played = [t for t in res["turns"] if not t.get("closed")]
    valid = [t for t in played if not t["error"]]
    labelled = [t for t in valid if t["expect"]]
    correct = [t for t in labelled if t["got_t"] == t["expect"]]
    exp = set(script.get("expect_reveals", []))
    got = set(res["revealed"])
    checks = [t[k] for t in played for k in ("bluff_ok", "gossip_ok") if k in t]
    row = {
        "config": name, "script": script["name"],
        "valid": f"{len(valid)}/{len(played)}",
        "tactics": f"{len(correct)}/{len(labelled)}",
        "bluff_gossip": f"{sum(checks)}/{len(checks)}" if checks else "-",
        "reveals": f"{len(exp & got)}/{len(exp)}" + (f" +{sorted(got - exp)}" if got - exp else ""),
        "closed": ("ok" if res["closed"] == script.get("expect_closed", False) else "WRONG"),
        "avg_s": round(sum(t["wall_s"] for t in played) / max(1, len(played)), 2),
        "max_s": max((t["wall_s"] for t in played), default=0),
        "avg_read": round(sum(t["prompt_n"] or 0 for t in played[1:]) / max(1, len(played) - 1)),
        "avg_gen": round(sum(t["predicted_n"] or 0 for t in played) / max(1, len(played))),
    }
    if "accusation" in res:
        row["accusation"] = "WIN" if res["accusation"][0] else "no"
    return row


def run_config(name, case, scripts):
    exe, model, extra = CONFIGS[name]
    for p in (exe, model):
        if not p.exists():
            print(f"[{name}] missing {p}")
            return []
    OUT.mkdir(exist_ok=True)
    log = open(OUT / f"{name}.server.log", "w", encoding="utf-8")
    env = dict(os.environ, GGML_VK_VISIBLE_DEVICES="0")
    vram0 = vram_used_mib()
    args = [str(exe), "-m", str(model), "--host", "127.0.0.1", "--port", str(PORT)] + COMMON + extra
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=env, cwd=exe.parent)
    rows, full = [], {"config": name, "args": [a.replace(str(ROOT) + os.sep, "") for a in args[1:]], "scripts": {}}
    try:
        if wait_health(proc) is None:
            print(f"[{name}] server failed to start; see {log.name}")
            return []
        vram = vram_used_mib()
        full["vram_mib"] = (vram - vram0) if vram is not None and vram0 is not None else None
        for script in scripts:
            res = play_script(case, script)
            full["scripts"][script["name"]] = res
            row = summarize(name, script, res)
            rows.append(row)
            print(f"[{name}] {script['name']}: " + "  ".join(f"{k}={v}" for k, v in row.items() if k not in ("config", "script")))
        (OUT / f"spade-{name}.json").write_text(json.dumps(full, indent=2) + "\n", encoding="utf-8")
        write_transcript(name, case, scripts, full)
        return rows
    finally:
        proc.terminate()
        try:
            proc.wait(15)
        except subprocess.TimeoutExpired:
            proc.kill()
        log.close()


def write_transcript(name, case, scripts, full):
    lines = [f"# Spade Club transcripts: {name}\n"]
    for script in scripts:
        res = full["scripts"][script["name"]]
        lines.append(f"## {script['name']} ({case['suspect_defs'][script['suspect']]['name']})\n\n_{script['goal']}_\n")
        for t in res["turns"]:
            if t.get("closed"):
                lines.append(f"- **Detective:** {t['say']}\n  - _(interview closed: suspect has walked out)_")
                continue
            who = f"*[presents {t['present']}]*" if t["present"] else t["say"]
            mark = "" if not t["expect"] or t["got_t"] == t["expect"] else f" (expected {t['expect']})"
            ev = t["event"]
            extra = []
            if ev["revealed"]:
                extra.append(f"REVEALED {ev['revealed']}")
            if ev["granted"]:
                extra.append(f"new evidence: {ev['granted']}")
            if ev["bluff"]:
                extra.append("bluff")
            if ev["gossip"]:
                extra.append(f"repeats {ev['gossip']}")
            extra += [f"rule: {x}" for x in ev["triggers"]]
            lines.append(f"- **Detective:** {who}\n  - **Suspect** ({t['emotion']}): {t['reply'] or t['error']}\n"
                         f"  - t={t['got_t']}{mark}, c={t['c']}, g={t['g']}, f={t['f']} | "
                         f"meters {t['meters']} | {t['wall_s']}s" + (" | " + "; ".join(extra) if extra else ""))
        lines.append(f"\nRevealed: {list(res['revealed']) or 'nothing'}"
                     + (f"\n\nAccusation: {res['accusation'][1]}" if "accusation" in res else "") + "\n")
    (OUT / f"spade-{name}-transcript.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    argv = sys.argv[1:]
    only = None
    if "--only" in argv:
        i = argv.index("--only")
        only = set(argv[i + 1].split(","))
        argv = argv[:i] + argv[i + 2:]
    spec = json.loads(SCRIPTS.read_text(encoding="utf-8"))
    case = engine.load_case(ROOT / spec["case"])
    scripts = [s for s in spec["scripts"] if not only or s["name"] in only]
    for n in argv or DEFAULT:
        rows = run_config(n, case, scripts)
        if not rows:
            continue
        cols = list(dict.fromkeys(k for r in rows for k in r))
        table = ["| " + " | ".join(cols) + " |", "|" + "---|" * len(cols)]
        table += ["| " + " | ".join(str(r.get(c, "")) for c in cols) + " |" for r in rows]
        print("\n" + "\n".join(table))
        (OUT / f"spade-{n}-summary.md").write_text("\n".join(table) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

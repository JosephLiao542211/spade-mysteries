"""One comparison table across every results/spade-<config>.json (bench.py output), totals over all scripts.

Run: python -I Tools/llm-harness/summarize_results.py [config ...]
"""

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = json.loads((HERE / "scripts" / "spade_club.json").read_text(encoding="utf-8"))["scripts"]
SIZES_GB = {"e4b": 4.98, "e2b": 3.11, "qwen3-0.6b": 0.40, "qwen3.5-0.8b": 0.53, "gemma3-1b": 0.81,
            "llama3.2-1b": 0.81, "qwen3-1.7b": 1.11}


def size_of(config):
    for k, v in SIZES_GB.items():
        if config.startswith(k):
            return v
    return None


def row(path):
    full = json.loads(path.read_text(encoding="utf-8"))
    config = full["config"]
    valid = played = labelled = right = model_right = exp_n = got_n = extra = closed_ok = 0
    wins, walls = [], []
    for sc in SCRIPTS:
        res = full["scripts"].get(sc["name"])
        if not res:
            continue
        turns = [t for t in res["turns"] if not t.get("closed")]
        played += len(turns)
        valid += sum(1 for t in turns if not t["error"])
        for t in turns:
            if t["error"] or not t["expect"]:
                continue
            labelled += 1
            right += t["got_t"] == t["expect"]
            model_right += t.get("model_t", t["got_t"]) == t["expect"]
        exp = set(sc.get("expect_reveals", []))
        got = set(res["revealed"])
        exp_n += len(exp)
        got_n += len(exp & got)
        extra += len(got - exp)
        closed_ok += res["closed"] == sc.get("expect_closed", False)
        if "accusation" in res:
            wins.append(res["accusation"][0])
        walls += [t["wall_s"] for t in turns]
    walls.sort()
    return {
        "config": config,
        "download_gb": size_of(config),
        "valid_json": f"{valid}/{played}",
        "tactics": f"{right}/{labelled} ({right / max(1, labelled):.0%})",
        "tactics_model_only": f"{model_right}/{labelled}",
        "reveals": f"{got_n}/{exp_n}" + (f" +{extra} unexpected" if extra else ""),
        "walkout_right": f"{closed_ok}/{len(SCRIPTS)}",
        "accusation": "WIN" if wins and all(wins) else "no" if wins else "-",
        "median_s": walls[len(walls) // 2] if walls else None,
    }


def main():
    want = sys.argv[1:]
    paths = sorted((HERE / "results").glob("spade-*.json"))
    rows = [row(p) for p in paths if not want or p.stem[len("spade-"):] in want]
    if not rows:
        print("no results")
        return
    cols = list(rows[0])
    print("| " + " | ".join(cols) + " |")
    print("|" + "---|" * len(cols))
    for r in rows:
        print("| " + " | ".join(str(r[c]) for c in cols) + " |")


if __name__ == "__main__":
    main()

"""Interrogation engine: fear / personality meters, secret keys, rules, statements as evidence, bluffs, gossip, endings.

The game owns all of this; the model only classifies the detective's line (t, c, g) and plays the scene.
Mirrors Source/Spade_mystery/Llm/LlmEngine.cpp (and LlmCaseData.cpp for loading): change both together.
Data format: Content/Llm/Cases/<Case>/case.json + one JSON per suspect (see docs/cases/spade-club.md).
"""

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

NONE = "none"


def load_case(case_dir):
    case_dir = Path(case_dir)
    case = json.loads((case_dir / "case.json").read_text(encoding="utf-8"))
    case["suspect_defs"] = {}
    for fname in case["suspects"]:
        s = json.loads((case_dir / fname).read_text(encoding="utf-8"))
        case["suspect_defs"][s["id"]] = s
    case["evidence_by_id"] = {e["id"]: e for e in case["evidence"]}
    for d in case["suspect_defs"].values():
        for sec in d["secrets"]:
            g = sec.get("grants_evidence", [])
            sec["grants_evidence"] = [g] if isinstance(g, str) else list(g)
    return case


@dataclass
class SuspectState:
    sid: str
    meters: dict
    revealed: dict = field(default_factory=dict)      # secret id -> text actually unlocked (route version)
    history: list = field(default_factory=list)       # (user message, assistant raw JSON), byte-identical replay
    evidence_seen: set = field(default_factory=set)
    shutdown_left: int = 0
    shutdown_note: str = ""
    after_shutdown_set: dict = field(default_factory=dict)
    fired: set = field(default_factory=set)            # trigger rules currently fired (re-arm when condition false)
    closed: bool = False
    caught_bluff: bool = False                         # mention in the next state note, then clear


class CaseState:
    def __init__(self, case):
        self.case = case
        self.inventory = {e["id"] for e in case["evidence"] if e.get("found_at_start")}
        self.suspects = {sid: SuspectState(sid, {m: float(v["start"]) for m, v in d["meters"].items()})
                         for sid, d in case["suspect_defs"].items()}
        self.log = []  # cross-suspect events, for debugging

    def sdef(self, sid):
        return self.case["suspect_defs"][sid]


# ---------- conditions and meters ----------

def cond_ok(cond, st):
    for k, v in cond.items():
        if k.endswith("_gte"):
            if st.meters.get(k[:-4], 0) < v:
                return False
        elif k.endswith("_lt"):
            if st.meters.get(k[:-3], 0) >= v:
                return False
        elif k == "evidence_seen":
            if not set(v) <= st.evidence_seen:
                return False
        elif k == "revealed":
            if not set(v) <= set(st.revealed):
                return False
        else:
            raise ValueError(f"unknown condition key {k}")
    return True


def add_meter(cs, sid, meter, delta):
    st, d = cs.suspects[sid], cs.sdef(sid)
    if meter not in st.meters:
        return
    if delta > 0:
        for dm in d.get("dampen", []):  # e.g. frightened people don't confide: loyalty gains halved while fear is high
            if dm["meter"] == meter and cond_ok(dm["when"], st):
                delta *= dm["factor"]
    st.meters[meter] = max(0.0, min(100.0, st.meters[meter] + delta))


def apply_effects(cs, sid, effects):
    for m, v in (effects or {}).items():
        add_meter(cs, sid, m, v)


def words_for(bands, value):
    text = bands[0][1]
    for lo, w in bands:
        if value >= lo:
            text = w
    return text


def while_rules(cs, sid):
    st = cs.suspects[sid]
    return [r for r in cs.sdef(sid).get("rules", []) if r["mode"] == "while" and cond_ok(r["when"], st)]


def secrets_blocked(cs, sid):
    st = cs.suspects[sid]
    return st.shutdown_left > 0 or st.closed or any(r.get("block_secrets") for r in while_rules(cs, sid))


def available_secret_details(cs, sid):
    """Secrets whose key is satisfied right now: [(id, text, topic)]. Only these go into the prompt and schema."""
    if secrets_blocked(cs, sid):
        return []
    st, out = cs.suspects[sid], []
    for sec in cs.sdef(sid)["secrets"]:
        if sec["id"] in st.revealed:
            continue
        for route in sec["routes"]:
            if cond_ok(route["when"], st):
                out.append((sec["id"], route.get("text", sec["text"]), sec["topic"]))
                break
    return out


def available_secrets(cs, sid):
    return [(i, t) for i, t, _ in available_secret_details(cs, sid)]


# ---------- prompt ----------

def static_system(case, sid):
    s = case["suspect_defs"][sid]
    rules = "\n".join(f"- {r}" for r in case["rules"])
    claims = "\n".join(f"- {c['text']}" for c in s["claims"])
    tactics = "; ".join(f"{k} ({v})" for k, v in case["tactics"].items())
    evidence = "; ".join(f"{e['id']} ({e['name'].lower()})" for e in case["evidence"])
    others = "; ".join(f"{oid} ({o['name']})" for oid, o in case["suspect_defs"].items() if oid != sid)
    return (f"SETTING: {case['setting']}\n\n"
            f"WHO YOU ARE: {s['character']}\n\n"
            f"WHAT YOU HAVE TOLD THE POLICE:\n{claims}\n\n"
            f"RULES:\n{rules}\n\n"
            "OUTPUT: Answer only with JSON.\n"
            f"t = the detective's tactic in their LAST line, one of: {tactics}.\n"
            f"c = the statement or evidence the detective claims to HAVE, repeats or shows you in their LAST line, "
            f"one of: {evidence}; or none. Only asking about something is not claiming to have it: then c is none. "
            "These labels are only for classifying; never bring them up yourself.\n"
            f"g = whose words the detective is repeating to you in their LAST line, one of: {others}; or none.\n"
            "f = the id of a secret from SECRETS YOU WILL ADMIT whose topic the detective's line touches, "
            "otherwise none; if f is not none, r must say that secret. e = your emotion. r = your spoken reply.")


def state_note(cs, sid):
    case, st, d = cs.case, cs.suspects[sid], cs.sdef(sid)
    lines = ["[STATE]"]
    for m, v in st.meters.items():
        bands = d["meters"][m].get("words") or case["meter_words"].get(m)
        if bands:
            lines.append(words_for(bands, v))
    if st.caught_bluff:
        lines.append(d.get("bluff_note", "The detective just claimed to have something they don't have."))
    if st.shutdown_left > 0:
        lines.append(st.shutdown_note)
    for r in while_rules(cs, sid):
        lines.append(r["note"])
    avail = available_secret_details(cs, sid)
    if avail:
        lines.append("SECRETS YOU WILL ADMIT: you have decided to give these up. If the detective's line touches "
                     "the topic, set f to the id and say it in r, in your own words, keeping the facts exact:")
        lines += [f"({i}) If asked about {topic}: {t}" for i, t, topic in avail]
    else:
        lines.append("SECRETS YOU WILL ADMIT: none. Keep to what you have told the police.")
    if st.revealed:
        lines.append("You have already admitted: " + " ".join(st.revealed.values()))
    return "\n".join(lines)


def schema(cs, sid):
    case = cs.case
    others = [oid for oid in case["suspect_defs"] if oid != sid]
    return {
        "type": "object",
        "properties": {
            # Order matters: classification first, then the decision to admit (f), then the reply that carries it.
            # With f after r, E4B never admitted anything; the clue card covers any wording drift in r.
            "t": {"enum": list(case["tactics"])},
            "c": {"enum": [e["id"] for e in case["evidence"]] + [NONE]},
            "g": {"enum": others + [NONE]},
            "f": {"enum": [NONE] + [i for i, _ in available_secrets(cs, sid)]},
            "e": {"enum": case["emotions"]},
            "r": {"type": "string", "maxLength": 300},
        },
        "required": ["t", "c", "g", "f", "e", "r"],
    }


def detective_message(line):
    return f"[DETECTIVE] {line}"


def evidence_message(case, eid):
    e = case["evidence_by_id"][eid]
    if e.get("source"):
        return f"[EVIDENCE] The detective tells you what another witness said: {e['description']}."
    return f"[EVIDENCE] The detective places this on the table: {e['description']}."


def apply_gossip(cs, sid, source, ev):
    """The detective repeated `source`'s words to `sid` (typed, or by presenting their statement). Once per turn."""
    if source == sid or source not in cs.suspects or ev["gossip"] == source:
        return
    ev["gossip"] = source
    apply_effects(cs, source, cs.sdef(source).get("gossip_cost"))
    heard = cs.sdef(sid).get("gossip_heard", {})
    apply_effects(cs, sid, heard.get("self"))
    apply_effects(cs, source, heard.get("source"))
    cs.log.append(f"{sid} heard {source}'s words")


def see_evidence(cs, sid, eid, ev):
    """First time this suspect hears or sees an item: its effects, plus gossip if it is someone's statement."""
    st = cs.suspects[sid]
    if eid in st.evidence_seen:
        return
    st.evidence_seen.add(eid)
    apply_effects(cs, sid, cs.sdef(sid).get("evidence_effects", {}).get(eid))
    src = cs.case["evidence_by_id"][eid].get("source")
    if src:
        apply_gossip(cs, sid, src, ev)


def build_messages(cs, sid, last_user):
    st = cs.suspects[sid]
    msgs = [{"role": "system", "content": static_system(cs.case, sid)}]
    for user, assistant in st.history:
        msgs.append({"role": "user", "content": user})
        msgs.append({"role": "assistant", "content": assistant})
    msgs.append({"role": "user", "content": state_note(cs, sid) + "\n\n" + last_user})
    return msgs


# ---------- turn resolution ----------

def names_suspect(line, sdef):
    """True if the line uses any part of the suspect's name ("Tommy", "Mullen")."""
    words = set(re.findall(r"[a-z]+", line.lower()))
    return any(p.lower() in words for p in re.findall(r"[A-Za-z]{3,}", sdef["name"]))


def clue_text(case, sid, secret_id, fallback=""):
    """What the player reads when a secret comes out, in the third person. The secret's own text is written to
    the suspect ("You pulled the main fuse..."), so use its `clue` field, or the statements it grants."""
    sec = next((s for s in case["suspect_defs"][sid]["secrets"] if s["id"] == secret_id), None)
    if not sec:
        return fallback
    if sec.get("clue"):
        return sec["clue"]
    lines = [case["evidence_by_id"][g]["description"] for g in sec["grants_evidence"] if g in case["evidence_by_id"]]
    return ". ".join(lines) + "." if lines else fallback


def resolve_turn(cs, sid, user_msg, parsed, raw, allowed, presented=None):
    """Apply the outcome of one exchange. parsed = model JSON (or None). presented = evidence id if this turn
    was an evidence presentation event. Returns a dict describing what happened."""
    st, d, case = cs.suspects[sid], cs.sdef(sid), cs.case
    ev = {"deltas": {}, "revealed": None, "granted": [], "bluff": False, "gossip": None, "triggers": []}
    before = dict(st.meters)
    was_shutdown = st.shutdown_left > 0
    st.caught_bluff = False

    if presented:
        see_evidence(cs, sid, presented, ev)
    elif parsed:
        apply_effects(cs, sid, d["tactic_effects"].get(parsed["t"]))
        c, g = parsed.get("c", NONE), parsed.get("g", NONE)
        src = case["evidence_by_id"].get(c, {}).get("source")
        if g != NONE and src and src != g:
            # "Vera says she saw you..." came back as g=vera_lane, c=<Mickey's statement about Vera>: inconsistent,
            # and treating it as a claim made it a false bluff. Trust g and drop c.
            c = NONE
        if c != NONE and c not in cs.inventory and src and (
                src == sid or not names_suspect(user_msg, case["suspect_defs"][src])):
            # Bluffing a statement means naming the witness ("Tommy told me..."). "Did you go up the back stairs?"
            # came back as c=<a statement about the stairs>: a false bluff that steadied Vera and punished the
            # player. Nobody can be bluffed with their own statement either.
            c = NONE
        if c != NONE:
            if c in cs.inventory:
                see_evidence(cs, sid, c, ev)
            else:
                ev["bluff"] = True
                st.caught_bluff = "bluff_note" in d  # only suspects who see through bluffs notice
                apply_effects(cs, sid, d.get("bluff_effects"))
        g = parsed.get("g", NONE)
        if g != NONE:
            apply_gossip(cs, sid, g, ev)

    if parsed:
        f = parsed.get("f", NONE)
        if f != NONE and f in allowed:
            st.revealed[f] = allowed[f]
            ev["revealed"] = f
            sec = next(s for s in d["secrets"] if s["id"] == f)
            for g in sec["grants_evidence"]:
                if g not in cs.inventory:
                    cs.inventory.add(g)
                    ev["granted"].append(g)

    if was_shutdown:
        st.shutdown_left -= 1
        if st.shutdown_left == 0:
            for m, v in st.after_shutdown_set.items():
                st.meters[m] = float(v)

    for i, r in enumerate(d.get("rules", [])):
        if r["mode"] != "trigger":
            continue
        if not cond_ok(r["when"], st):
            st.fired.discard(i)
            continue
        if i in st.fired:
            continue
        st.fired.add(i)
        ev["triggers"].append(r.get("announce") or r["note"])  # announce = the same event told to the player
        if r.get("shutdown_turns"):
            st.shutdown_left = r["shutdown_turns"]
            st.shutdown_note = r["note"]
            st.after_shutdown_set = r.get("after_shutdown_set", {})
        if r.get("end_interview"):
            st.closed = True
        for oid in r.get("close_others", []):  # e.g. he walks out and takes his wife with him
            cs.suspects[oid].closed = True
        for oid in cs.suspects:
            if oid != sid:
                apply_effects(cs, oid, r.get("others"))

    st.history.append((user_msg, raw))
    ev["deltas"] = {m: round(st.meters[m] - before[m], 1) for m in st.meters if st.meters[m] != before[m]}
    return ev


# ---------- accusation ----------

def ending(acc, eid, **subs):
    e = acc["endings"][eid]
    text = e["text"]
    for k, v in subs.items():
        text = text.replace("{" + k + "}", v)
    return e["title"], text


def accuse(cs, accused):
    """Returns {win, ending, title, text}. Proof = every entry in `proofs` backed by a witness who revealed the
    secret, is still in the club, and whose meters still meet `when`; otherwise that witness takes it back."""
    acc = cs.case["accusation"]
    name = cs.sdef(accused)["name"]
    missing, notes = [], []
    if accused != acc["culprit"]:
        eid = acc["on_wrong"].get(accused, acc["on_wrong"]["default"])
    elif any(sec in cs.suspects[accused].revealed for sec in acc.get("confession_secrets", [])):
        eid = acc["on_confession"]
    else:
        for proof in acc["proofs"]:
            ok = False
            for w in proof["witnesses"]:
                ws, wname = cs.suspects[w["suspect"]], cs.sdef(w["suspect"])["name"]
                if w["secret"] not in ws.revealed:
                    continue
                if ws.closed:
                    notes.append(f"{wname} has left the club and won't testify.")
                elif not cond_ok(w.get("when", {}), ws):
                    notes.append(w.get("recant", f"{wname} takes back their statement."))
                else:
                    ok = True
            if not ok:
                missing.append(proof["label"])
        eid = acc["on_no_case"] if missing else acc["on_proof"]
    title, text = ending(acc, eid, name=name, missing=", ".join(missing))
    for n in dict.fromkeys(notes):
        text += " " + n
    for ep in acc.get("epilogues", []):
        if cond_ok(ep["when"], cs.suspects[ep["suspect"]]) and (not ep.get("endings") or eid in ep["endings"]):
            text += " " + ep["text"]
    return {"win": eid in (acc["on_confession"], acc["on_proof"]), "ending": eid, "title": title, "text": text}

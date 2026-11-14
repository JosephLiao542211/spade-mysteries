"""Model-free tests of the interrogation engine on the Spade Club case. Run: python -I Tools/llm-harness/test_engine.py"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CASE = engine.load_case(ROOT / "Content" / "Llm" / "Cases" / "SpadeClub")


def turn(cs, sid, t, c="none", g="none", f="none", present=None):
    allowed = dict(engine.available_secrets(cs, sid))
    parsed = None if present else {"t": t, "c": c, "g": g, "e": "calm", "r": "...", "f": f}
    return engine.resolve_turn(cs, sid, "x", parsed, "{}", allowed, presented=present)


def check(name, cond):
    print(("PASS " if cond else "FAIL ") + name)
    return cond


def main():
    ok = True

    cs = engine.CaseState(CASE)
    v = cs.suspects["vera_lake"]
    ok &= check("vera starts with no secrets", engine.available_secrets(cs, "vera_lake") == [])
    for t in ["flattery", "empathy", "reassure"]:
        turn(cs, "vera_lake", t)
    ok &= check("vera trust route opens augie_scared", [i for i, _ in engine.available_secrets(cs, "vera_lake")] == ["augie_scared"])
    turn(cs, "vera_lake", "threaten")
    turn(cs, "vera_lake", "threaten")
    ok &= check("vera lies while fear >= 40", engine.secrets_blocked(cs, "vera_lake") and "lie" in engine.state_note(cs, "vera_lake"))
    ok &= check("threats cost vera trust", v.meters["trust"] < 40)

    cs = engine.CaseState(CASE)
    for t in ["reassure", "reassure", "empathy", "reassure", "empathy", "empathy"]:
        turn(cs, "vera_lake", t)
    ids = [i for i, _ in engine.available_secrets(cs, "vera_lake")]
    ok &= check("vera full trust opens saw_reilly", "saw_reilly" in ids)
    turn(cs, "vera_lake", "question", f="saw_reilly")
    ok &= check("saw_reilly grants reilly_coat", "reilly_coat" in cs.inventory)
    turn(cs, "deacon_moss", "question", g="vera_lake")
    ok &= check("repeating vera's words costs her 35 trust", cs.suspects["vera_lake"].meters["trust"] < 60)

    cs = engine.CaseState(CASE)
    m = cs.suspects["deacon_moss"]
    turn(cs, "deacon_moss", "logic", c="ledger_carbon")  # not held -> bluff
    ok &= check("moss catches the bluff", m.caught_bluff and m.meters["trust"] < 10 and "bluffing" in engine.state_note(cs, "deacon_moss"))
    turn(cs, "deacon_moss", "challenge_pride")
    ids = dict(engine.available_secrets(cs, "deacon_moss"))
    ok &= check("moss pride route gives full reilly_absent", "two full hands" in ids.get("reilly_absent", ""))
    cs = engine.CaseState(CASE)
    turn(cs, "deacon_moss", None, present="marked_deck")
    ids = dict(engine.available_secrets(cs, "deacon_moss"))
    ok &= check("moss fear route gives the vague version", "can't recall" in ids.get("reilly_absent", ""))
    turn(cs, "deacon_moss", None, present="marked_deck")
    ok &= check("evidence effects apply once per suspect", cs.suspects["deacon_moss"].meters["fear"] == 80)

    cs = engine.CaseState(CASE)
    lou = cs.suspects["lou_barrett"]
    turn(cs, "lou_barrett", "threaten")
    turn(cs, "lou_barrett", "threaten")
    ok &= check("lou fear route opens r_payments", "r_payments" in dict(engine.available_secrets(cs, "lou_barrett")))
    turn(cs, "lou_barrett", "threaten")
    ok &= check("lou panics at fear 70", lou.shutdown_left == 3 and engine.available_secrets(cs, "lou_barrett") == [])
    for _ in range(3):
        turn(cs, "lou_barrett", "empathy")
    ok &= check("lou recovers to fear 50 after 3 turns", lou.shutdown_left == 0 and lou.meters["fear"] == 50)

    cs = engine.CaseState(CASE)
    r = cs.suspects["dan_reilly"]
    for _ in range(3):
        turn(cs, "dan_reilly", "threaten")
    ok &= check("reilly walks out at anger 70", r.closed)
    ok &= check("reilly leans on the witnesses", cs.suspects["vera_lake"].meters["trust"] == 5)

    cs = engine.CaseState(CASE)
    ok &= check("accusing moss loses", engine.accuse(cs, "deacon_moss")[0] is False)
    won, why = engine.accuse(cs, "dan_reilly")
    ok &= check("accusing reilly with nothing is not enough", not won and "motive" in why)
    cs.inventory |= {"ledger_carbon", "reilly_coat"}
    cs.suspects["deacon_moss"].revealed["reilly_absent"] = "..."
    cs.suspects["deacon_moss"].meters["trust"] = 20
    won, why = engine.accuse(cs, "dan_reilly")
    ok &= check("witness with low trust recants", not won and "takes back" in why)
    cs.suspects["deacon_moss"].meters["trust"] = 45
    ok &= check("full case wins", engine.accuse(cs, "dan_reilly")[0])
    cs = engine.CaseState(CASE)
    cs.suspects["dan_reilly"].revealed["safe_open"] = "..."
    ok &= check("confession alone wins", engine.accuse(cs, "dan_reilly")[0])

    print("\nALL PASS" if ok else "\nSOME FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

"""Model-free tests of the interrogation engine on the Black Spade case. Run: python -I Tools/llm-harness/test_engine.py"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import engine  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
CASE = engine.load_case(ROOT / "Content" / "Llm" / "Cases" / "SpadeClub")


def turn(cs, sid, t, c="none", g="none", f="none", present=None, say="x"):
    allowed = dict(engine.available_secrets(cs, sid))
    parsed = None if present else {"t": t, "c": c, "g": g, "e": "calm", "r": "...", "f": f}
    return engine.resolve_turn(cs, sid, engine.detective_message(say), parsed, "{}", allowed, presented=present)


def avail(cs, sid):
    return [i for i, _ in engine.available_secrets(cs, sid)]


def reveal(cs, sid, secret):
    assert secret in avail(cs, sid), (sid, secret, avail(cs, sid), cs.suspects[sid].meters)
    return turn(cs, sid, "question", f=secret)


def check(name, cond):
    print(("PASS " if cond else "FAIL ") + name)
    return cond


def main():
    ok = True

    # --- Vera: composure is the lock; flattery and bluffs make it stronger, statements break it.
    cs = engine.CaseState(CASE)
    v = cs.suspects["vera_lane"]
    ok &= check("vera starts with no secrets", avail(cs, "vera_lane") == [])
    turn(cs, "vera_lane", "flattery")
    ok &= check("flattering vera raises her composure", v.meters["composure"] == 90)
    ev = turn(cs, "vera_lane", "question", c="stmt_vera_paid", say="Did you pay someone to cut the lights?")
    ok &= check("a claim that names no witness is not a bluff", not ev["bluff"] and v.meters["composure"] == 90)
    ev = turn(cs, "vera_lane", "question", c="stmt_vera_went_up", say="Miss Lane, did you go up the back stairs?")
    ok &= check("nobody is bluffed with their own statement", not ev["bluff"] and v.meters["composure"] == 90)
    ev = turn(cs, "vera_lane", "logic", c="stmt_vera_paid", say="Tommy Mullen told me you paid him to kill the lights.")  # not held
    ok &= check("vera catches a bluffed statement and steadies", ev["bluff"] and v.caught_bluff and v.meters["composure"] == 95)
    ok &= check("clue text is third person, from the granted statement",
                engine.clue_text(CASE, "tommy_mullen", "who_paid").startswith("Tommy Mullen says Vera Lane paid him"))
    ok &= check("clue text for a secret with no statement uses its clue field",
                engine.clue_text(CASE, "vera_lane", "confession").startswith("Vera Lane confesses"))
    cs = engine.CaseState(CASE)
    v = cs.suspects["vera_lane"]
    cs.inventory |= {"stmt_singing_stopped", "stmt_vera_paid", "stmt_vera_bar"}
    turn(cs, "vera_lane", None, present="stmt_singing_stopped")
    ok &= check("one statement is not enough", avail(cs, "vera_lane") == [])
    ev = turn(cs, "vera_lane", None, present="stmt_vera_paid")
    ok &= check("presenting tommy's statement counts as repeating tommy", ev["gossip"] == "tommy_mullen")
    ok &= check("two statements crack her: went_up", avail(cs, "vera_lane") == ["went_up"] and "slipping" in engine.state_note(cs, "vera_lane"))
    ev = reveal(cs, "vera_lane", "went_up")
    ok &= check("went_up puts her statement in the notebook", ev["granted"] == ["stmt_vera_went_up"])
    ok &= check("no confession before she breaks", avail(cs, "vera_lane") == [])
    turn(cs, "vera_lane", None, present="stmt_vera_bar")
    ok &= check("third statement: confession", avail(cs, "vera_lane") == ["confession"])

    # --- Mickey: loyalty opens him, fear dampens loyalty and shuts him down.
    cs = engine.CaseState(CASE)
    m = cs.suspects["mickey_doyle"]
    turn(cs, "mickey_doyle", "appeal")
    ok &= check("one appeal opens tommy_fuse", avail(cs, "mickey_doyle") == ["tommy_fuse"])
    turn(cs, "mickey_doyle", "threaten")
    before = m.meters["loyalty"]
    turn(cs, "mickey_doyle", "appeal")
    ok &= check("fear halves loyalty gains", m.meters["loyalty"] - before == 5)
    turn(cs, "mickey_doyle", "threaten")
    ok &= check("mickey panics at fear 85", m.shutdown_left == 3 and avail(cs, "mickey_doyle") == [])
    for _ in range(3):
        turn(cs, "mickey_doyle", "small_talk")
    ok &= check("mickey recovers to fear 60", m.shutdown_left == 0 and m.meters["fear"] == 60)
    cs = engine.CaseState(CASE)
    for _ in range(4):
        turn(cs, "mickey_doyle", "appeal")
    reveal(cs, "mickey_doyle", "gun_missing")
    ok &= check("mickey won't name vera on loyalty alone", "vera_behind_bar" not in avail(cs, "mickey_doyle"))
    cs.inventory.add("stmt_singing_stopped")
    turn(cs, "mickey_doyle", None, present="stmt_singing_stopped")
    ok &= check("a statement against vera lets him name her", "vera_behind_bar" in avail(cs, "mickey_doyle"))

    # --- Graves: greed for the motive, fear only through the cloakroom.
    cs = engine.CaseState(CASE)
    turn(cs, "harlan_graves", "threaten")
    turn(cs, "harlan_graves", "threaten")
    ok &= check("threats barely move graves", cs.suspects["harlan_graves"].meters["fear"] == 11 and avail(cs, "harlan_graves") == [])
    turn(cs, "harlan_graves", "deal")
    turn(cs, "harlan_graves", "flattery")
    ok &= check("deal + flattery: contract_sold", avail(cs, "harlan_graves") == ["contract_sold"])
    cs.inventory.add("stmt_cloakroom")
    turn(cs, "harlan_graves", None, present="stmt_cloakroom")
    ok &= check("logan's statement scares graves into the cloakroom", "cloakroom" in avail(cs, "harlan_graves"))
    ok &= check("and logan gets nervous for being quoted to graves", cs.suspects["logan_wu"].meters["fear"] == 35)

    # --- Tommy: bragging gets the fuse, breaking his bravado gets the name.
    cs = engine.CaseState(CASE)
    turn(cs, "tommy_mullen", "challenge_pride")
    turn(cs, "tommy_mullen", "challenge_pride")
    ok &= check("bragging route: pulled_fuse", avail(cs, "tommy_mullen") == ["pulled_fuse"])
    reveal(cs, "tommy_mullen", "pulled_fuse")
    ok &= check("but no name while he's cocky", avail(cs, "tommy_mullen") == [])
    cs = engine.CaseState(CASE)
    cs.inventory.add("stmt_tommy_fuse")
    turn(cs, "tommy_mullen", None, present="stmt_tommy_fuse")
    reveal(cs, "tommy_mullen", "pulled_fuse")
    turn(cs, "tommy_mullen", "logic")
    ok &= check("mickey's statement + logic: who_paid", avail(cs, "tommy_mullen") == ["who_paid"])

    # --- Allison and Logan.
    cs = engine.CaseState(CASE)
    turn(cs, "allison_wu", "deal")
    ok &= check("ambition opens ledger_theft", avail(cs, "allison_wu") == ["ledger_theft"])
    for _ in range(3):
        turn(cs, "allison_wu", "threaten")
    ok &= check("allison denies everything at fear 75", avail(cs, "allison_wu") == [])
    cs = engine.CaseState(CASE)
    for t in ["deal", "flattery", "reassure"]:
        turn(cs, "allison_wu", t)
    ok &= check("ambition and calm: singing_stopped", "singing_stopped" in avail(cs, "allison_wu"))

    cs = engine.CaseState(CASE)
    lo = cs.suspects["logan_wu"]
    turn(cs, "logan_wu", "logic", g="vera_lane")
    ok &= check("quoting vera to logan scares him", lo.meters["fear"] == 45)
    cs2 = engine.CaseState(CASE)
    ev = turn(cs2, "logan_wu", "logic", c="stmt_vera_bar", g="vera_lane")
    ok &= check("c from another suspect than g is dropped, not a bluff", not ev["bluff"] and ev["gossip"] == "vera_lane")
    ev = reveal(cs, "logan_wu", "went_looking")
    ok &= check("went_looking grants two statements", sorted(ev["granted"]) == ["stmt_cloakroom", "stmt_gardenia"])
    cs = engine.CaseState(CASE)
    lo = cs.suspects["logan_wu"]
    cs.inventory.add("stmt_ledger")
    turn(cs, "logan_wu", None, present="stmt_ledger")
    ok &= check("the ledger statement plus gossip: temper 65", lo.meters["temper"] == 65)
    ok &= check("telling logan about the cloakroom scares allison", cs.suspects["allison_wu"].meters["fear"] == 60)
    ok &= check("and blocks his secrets", engine.secrets_blocked(cs, "logan_wu"))
    ev = turn(cs, "logan_wu", "challenge_pride")
    ok &= check("logan walks out at temper 80 and takes allison", lo.closed and cs.suspects["allison_wu"].closed)
    ok &= check("the walk-out is announced to the player in the third person", ev["triggers"] and ev["triggers"][0].startswith("Logan Wu stands"))

    # --- Accusation.
    cs = engine.CaseState(CASE)
    r = engine.accuse(cs, "mickey_doyle")
    ok &= check("accusing mickey: the wrong man", not r["win"] and r["ending"] == "wrong_man" and "Mickey Doyle goes down" in r["text"])
    ok &= check("accusing graves: bought and paid", engine.accuse(cs, "harlan_graves")["ending"] == "bought_and_paid")
    r = engine.accuse(cs, "vera_lane")
    ok &= check("vera with nothing: no case, all three missing", r["ending"] == "no_case" and "a motive, how she got the gun, proof she left the stage" in r["text"])

    def solved_state():
        cs = engine.CaseState(CASE)
        cs.suspects["harlan_graves"].revealed["contract_sold"] = "..."
        cs.suspects["mickey_doyle"].revealed["vera_behind_bar"] = "..."
        cs.suspects["mickey_doyle"].meters.update(fear=40, loyalty=85)
        cs.suspects["allison_wu"].revealed["singing_stopped"] = "..."
        return cs
    cs = solved_state()
    r = engine.accuse(cs, "vera_lane")
    ok &= check("motive + means + opportunity: case closed", r["win"] and r["ending"] == "case_closed")
    cs.suspects["mickey_doyle"].meters["fear"] = 90
    r = engine.accuse(cs, "vera_lane")
    ok &= check("a terrified witness recants", not r["win"] and "mistaken" in r["text"] and "how she got the gun" in r["text"])
    cs = solved_state()
    cs.suspects["allison_wu"].closed = True
    r = engine.accuse(cs, "vera_lane")
    ok &= check("a witness who left can't testify", not r["win"] and "has left the club" in r["text"])
    cs.suspects["logan_wu"].meters["temper"] = 85
    ok &= check("epilogue: logan took allison home", "never sang at the Black Spade again" in r["text"] or
                "never sang" in engine.accuse(cs, "vera_lane")["text"])
    cs = engine.CaseState(CASE)
    cs.suspects["vera_lane"].revealed["confession"] = "..."
    cs.suspects["tommy_mullen"].meters["fear"] = 75
    r = engine.accuse(cs, "vera_lane")
    ok &= check("confession alone: last song, tommy charged", r["ending"] == "last_song" and "accessory" in r["text"])

    print("\nALL PASS" if ok else "\nSOME FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

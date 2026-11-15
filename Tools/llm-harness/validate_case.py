"""Check a case folder for broken references and for cases that can't be solved.

Run: python -I Tools/llm-harness/validate_case.py [Content/Llm/Cases/SpadeClub]

Errors: unknown ids (evidence, tactics, meters, secrets, suspects), duplicate slots, secrets that can never
unlock, and an accusation that can never succeed. Warnings: things that are legal but probably mistakes.
Reachability assumes any meter value is reachable through play; only evidence and "revealed" chains can block.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import engine  # noqa: E402

COND_KEYS = ("evidence_seen", "revealed")


def _meter_of(key):
    for suf in ("_gte", "_lt"):
        if key.endswith(suf):
            return key[: -len(suf)]
    return None


def validate(case):
    errors, warnings = [], []
    tactics = set(case["tactics"])
    evidence = set(case["evidence_by_id"])
    suspects = case["suspect_defs"]
    secret_owner = {}

    slots = {}
    for sid, s in suspects.items():
        if s.get("slot") in slots:
            errors.append(f"{sid}: slot {s['slot']} already used by {slots[s['slot']]}")
        slots[s.get("slot")] = sid
        for sec in s["secrets"]:
            if sec["id"] in secret_owner:
                errors.append(f"secret id '{sec['id']}' used by both {secret_owner[sec['id']]} and {sid}")
            secret_owner[sec["id"]] = sid

    for sid, s in suspects.items():
        meters = set(s["meters"])
        own_secrets = {sec["id"] for sec in s["secrets"]}

        def check_cond(where, cond):
            for k, v in cond.items():
                m = _meter_of(k)
                if m is not None:
                    if m not in meters:
                        errors.append(f"{where}: unknown meter '{m}' (has {sorted(meters)})")
                    if not 0 <= v <= 100:
                        errors.append(f"{where}: {k}={v} is outside 0..100")
                elif k == "evidence_seen":
                    for e in v:
                        if e not in evidence:
                            errors.append(f"{where}: unknown evidence '{e}'")
                elif k == "revealed":
                    for x in v:
                        if x not in own_secrets:
                            errors.append(f"{where}: '{x}' is not one of {sid}'s secrets")
                else:
                    errors.append(f"{where}: unknown condition '{k}'")

        for t in s.get("tactic_effects", {}):
            if t not in tactics:
                errors.append(f"{sid}.tactic_effects: unknown tactic '{t}'")
        missing = tactics - set(s.get("tactic_effects", {}))
        if missing:
            warnings.append(f"{sid}: no tactic_effects for {sorted(missing)} (fine if intended: those lines move no meter)")
        for e in s.get("evidence_effects", {}):
            if e not in evidence:
                errors.append(f"{sid}.evidence_effects: unknown evidence '{e}'")
        for label, eff in [("tactic_effects", x) for x in s.get("tactic_effects", {}).values()] + \
                          [("evidence_effects", x) for x in s.get("evidence_effects", {}).values()] + \
                          [("bluff_effects", s.get("bluff_effects") or {})]:
            for m in eff:
                if m not in meters:
                    errors.append(f"{sid}.{label}: unknown meter '{m}'")
        for i, r in enumerate(s.get("rules", [])):
            check_cond(f"{sid}.rules[{i}].when", r["when"])
        for sec in s["secrets"]:
            if not sec.get("routes"):
                errors.append(f"{sid}.{sec['id']}: no routes, so it can never unlock")
            for j, route in enumerate(sec.get("routes", [])):
                check_cond(f"{sid}.{sec['id']}.routes[{j}]", route["when"])
            g = sec.get("grants_evidence")
            if g and g not in evidence:
                errors.append(f"{sid}.{sec['id']}.grants_evidence: unknown evidence '{g}'")
            if not sec.get("topic"):
                warnings.append(f"{sid}.{sec['id']}: no topic; models admit far less without one (plan §14)")

    # Reachability fixpoint over evidence + secrets.
    have = {e["id"] for e in case["evidence"] if e.get("found_at_start")}
    unlocked, order = set(), []
    changed = True
    while changed:
        changed = False
        for sid, s in suspects.items():
            for sec in s["secrets"]:
                if sec["id"] in unlocked:
                    continue
                for route in sec.get("routes", []):
                    w = route["when"]
                    if set(w.get("evidence_seen", [])) <= have and set(w.get("revealed", [])) <= unlocked:
                        unlocked.add(sec["id"])
                        order.append(sec["id"])
                        if sec.get("grants_evidence"):
                            have.add(sec["grants_evidence"])
                        changed = True
                        break
    for sid, s in suspects.items():
        for sec in s["secrets"]:
            if sec["id"] not in unlocked:
                errors.append(f"{sid}.{sec['id']}: can never unlock (needs evidence or secrets the player can't get)")
    for e in evidence - have:
        warnings.append(f"evidence '{e}' can never be found (not found_at_start and no secret grants it)")

    acc = case["accusation"]
    if acc["culprit"] not in suspects:
        errors.append(f"accusation.culprit: unknown suspect '{acc['culprit']}'")
    for key in ("motive_evidence", "physical_evidence"):
        for e in acc.get(key, []):
            if e not in evidence:
                errors.append(f"accusation.{key}: unknown evidence '{e}'")
    for w in acc.get("testimony", []):
        if w["suspect"] not in suspects:
            errors.append(f"accusation.testimony: unknown suspect '{w['suspect']}'")
        elif secret_owner.get(w["secret"]) != w["suspect"]:
            errors.append(f"accusation.testimony: '{w['secret']}' is not {w['suspect']}'s secret")
    for x in acc.get("confession_secrets", []):
        if secret_owner.get(x) != acc["culprit"]:
            errors.append(f"accusation.confession_secrets: '{x}' is not the culprit's secret")
    by_confession = any(x in unlocked for x in acc.get("confession_secrets", []))
    by_proof = (any(e in have for e in acc.get("motive_evidence", []))
                and any(e in have for e in acc.get("physical_evidence", []))
                and any(w["secret"] in unlocked for w in acc.get("testimony", [])))
    if not (by_confession or by_proof):
        errors.append("the case can't be won: no reachable confession, and motive + physical evidence + testimony "
                      "can't all be obtained")
    if not by_proof:
        warnings.append("the case can only be won by confession")
    return {"ok": not errors, "errors": errors, "warnings": warnings, "unlock_order": order}


def main():
    root = HERE.parents[1]
    folders = sys.argv[1:] or [str(root / "Content" / "Llm" / "Cases" / "SpadeClub")]
    bad = False
    for f in folders:
        rep = validate(engine.load_case(f))
        print(("OK   " if rep["ok"] else "FAIL ") + f)
        for e in rep["errors"]:
            print("  error:   " + e)
        for w in rep["warnings"]:
            print("  warning: " + w)
        if rep["ok"]:
            print("  unlock order: " + " -> ".join(rep["unlock_order"]))
        bad |= not rep["ok"]
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()

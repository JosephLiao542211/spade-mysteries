"""Score tactic_rules.py against the hand-labelled lines in scripts/*.json (no model needed).

Reports coverage (how often the rules answer) and precision (how often that answer matches the label).
Run: python -I Tools/llm-harness/score_tactic_rules.py [-v]
"""

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import engine  # noqa: E402
import tactic_rules  # noqa: E402

ROOT = HERE.parents[1]


def main():
    verbose = "-v" in sys.argv
    total = fired = right = 0
    for spec_path in sorted((HERE / "scripts").glob("*.json")):
        spec = json.loads(spec_path.read_text(encoding="utf-8"))
        case = engine.load_case(ROOT / spec["case"])
        for script in spec["scripts"]:
            for step in script["turns"]:
                if not step.get("expect"):
                    continue
                total += 1
                got = tactic_rules.classify(step["say"], case)
                if got is not None:
                    fired += 1
                    right += got == step["expect"]
                if verbose and got != step["expect"]:
                    print(f"  {'pass' if got is None else 'WRONG':5} expect={step['expect']:<17} got={got!s:<17} {step['say']}")
    print(f"lines {total} | rules answered {fired} ({fired / total:.0%}) | correct when answered {right}/{fired} "
          f"({right / max(1, fired):.0%}) | wrong {fired - right}")


if __name__ == "__main__":
    main()

"""Rule-based reading of the detective's tactic (t), used alongside the model.

Small models misread tactics (E2B ~70%, §14 of the plan), and a misread moves the meters the wrong way.
These rules only answer when a line is unambiguous (a threat, an apology, a jailbreak, a plain question)
and return None otherwise, so the model's label is kept. When they answer, the game uses the rule label.

Returns a tactic id from case.json's "tactics", or None. Mirrors Source/Spade_mystery/Llm/LlmTextRules.cpp (ClassifyTactic).
"""

import json
import re
from pathlib import Path

# Patterns are shared with the C++ (LlmTextRules.cpp): edit Content/Llm/text_rules.json, not this file.
_DATA = json.loads((Path(__file__).resolve().parents[2] / "Content" / "Llm" / "text_rules.json").read_text(encoding="utf-8"))
_RULES = [(t, p) for t, p in _DATA["tactic_rules"]]  # order = priority: the most loaded reading wins
_COMPILED = [(t, re.compile(p, re.I)) for t, p in _RULES]
_QUESTION = re.compile(_DATA["question"], re.I)
_SHOUT = re.compile(_DATA["shout"], re.I)  # "Talk!", "Answer me!"
_TESTIMONY = re.compile(_DATA["cites_testimony"], re.I)


def classify(line, case):
    """Tactic id for `line` if the rules are confident, else None. Only returns tactics defined in the case."""
    tactics = case["tactics"]
    text = " ".join(line.split())
    found = None
    for t, rx in _COMPILED:
        if t in tactics and rx.search(text):
            found = t
            break
    # Short shouted orders ("Talk!", "Answer me!") count as threats ("shouting at you").
    if found is None and "threaten" in tactics and _SHOUT.match(text):
        found = "threaten"
    if found is None and "question" in tactics and _QUESTION.match(_last_sentence(text)) and not _cites_something(text, case):
        found = "question"
    return found


def _last_sentence(text):
    parts = re.split(r"(?<=[.!])\s+", text)
    return parts[-1] if parts else text


def _cites_something(text, case):
    """Lines that bring up evidence or another person's words are often 'logic': leave those to the model."""
    low = text.lower()
    if _TESTIMONY.search(low):
        return True
    for e in case["evidence"]:
        for w in re.findall(r"[a-z]{4,}", e["name"].lower()):
            if re.search(rf"\b{w}s?\b", low):
                return True
    return False


# ---------- vetoes for the model's c and g ----------
# Small models fill c (evidence the detective claims to have) and g (whose words are repeated) at random, and a
# wrong c counts as a bluff, which frightens or angers the suspect. These checks only ever turn a label into
# "none": claiming an item, or quoting someone, requires the line to mention it.

def _words(s):
    return set(re.findall(r"[a-z]+", s.lower()))


def evidence_mentioned(line, evidence):
    """True if the line names the item: a 4+ letter word of its name, or a 3+ letter part of its id."""
    keys = {w for w in re.findall(r"[a-z]{4,}", evidence["name"].lower())}
    keys |= {p for p in evidence["id"].lower().split("_") if len(p) >= 3 and p not in ("the", "and")}
    words = _words(line)
    return any(k in words or (k + "s") in words or (k.endswith("s") and k[:-1] in words) for k in keys)


def suspect_mentioned(line, suspect_def):
    words = _words(line)
    return any(n.lower() in words for n in re.findall(r"[A-Za-z]{3,}", suspect_def["name"]))


def veto_claim(line, c, case):
    """The model's c, or "none" if the line never mentions that evidence."""
    e = case["evidence_by_id"].get(c)
    return c if e is None or evidence_mentioned(line, e) else "none"


def veto_gossip(line, g, case):
    """The model's g, or "none" if the line never names that suspect."""
    d = case["suspect_defs"].get(g)
    return g if d is None or suspect_mentioned(line, d) else "none"

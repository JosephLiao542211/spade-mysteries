"""Checks and cleans the spoken reply (r) before the player sees it.

JSON-schema decoding guarantees the shape of the reply, not its content. Small models in particular
narrate ("Vera smiles."), wrap the line in quotes, repeat the state note ("SECRETS YOU WILL ADMIT..."),
or step out of character ("As an AI..."). Each rule below comes from a failure seen in testing.

clean(r, name) -> tidied spoken line.
issues(r)      -> list of problems; non-empty means: retry the request once, then treat it as an invalid
                  reply (no state change, same as broken JSON).
Mirrors Source/Spade_mystery/Llm/LlmTextRules.cpp (CleanReply, ReplyIssues).
"""

import json
import re
from pathlib import Path

# Patterns are shared with the C++ (LlmTextRules.cpp): edit Content/Llm/text_rules.json, not this file.
_DATA = json.loads((Path(__file__).resolve().parents[2] / "Content" / "Llm" / "text_rules.json").read_text(encoding="utf-8"))
_META = re.compile(_DATA["guard_meta"], re.I)
_ECHO = re.compile(_DATA["guard_echo"], re.I)
_ACTION = re.compile(_DATA["guard_action"], re.I)


def _sentences(t):
    return [s.strip() for s in re.findall(r"[^.!?…]+[.!?…]+[\"”']?|[^.!?…]+$", t) if s.strip()]


def clean(r, name=""):
    first = name.split()[0] if name else ""
    t = r or ""
    t = re.sub(r"<think>.*?(</think>|$)", " ", t, flags=re.S | re.I)
    t = re.sub(r"\*[^*]*\*", " ", t)          # *leans back*
    t = re.sub(r"\[[^\]]*\]", " ", t)          # [stage directions]
    t = re.sub(r"\([^)]*\)", " ", t)           # (sighs)
    if name:
        t = re.sub(rf"^\s*({re.escape(name)}|{re.escape(first)})\s*[:\-–]\s*", "", t, flags=re.I)
    t = " ".join(t.split())
    # Prose with quoted dialogue: keep only the dialogue.
    quotes = [q.strip() for q in re.findall(r"[\"“]([^\"“”]{2,})[\"”]", t)]
    outside = re.sub(r"[\"“][^\"“”]*[\"”]", " ", t)
    if quotes and len(re.findall(r"[A-Za-z]+", outside)) >= 2:
        t = " ".join(quotes)
    # Narrated actions: "Vera smiles." / "I laugh nervously."
    self_third = re.compile(rf"^{re.escape(first)}\s+[a-z]") if first else None
    kept = [s for s in _sentences(t) if not ((self_third and self_third.match(s)) or _ACTION.match(s))]
    if kept:
        t = " ".join(kept)
    m = re.fullmatch(r"[\"“'](.*)[\"”']", t, flags=re.S)
    if m and not re.search(r"[\"“”]", m.group(1)):
        t = m.group(1).strip()
    return re.sub(r"\s+([,.!?])", r"\1", t).strip()


def issues(r):
    out = []
    if len(re.sub(r"[^A-Za-z]", "", r or "")) < 2:
        out.append("empty")
    if _META.search(r or ""):
        out.append("meta")
    if _ECHO.search(r or ""):
        out.append("echo")
    return out

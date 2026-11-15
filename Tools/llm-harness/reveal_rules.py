"""Rule-based "does this line touch an available secret's topic?".

The model decides f (admit a secret now) only when the line touches the secret's topic. Small models almost
never set f (Qwen3 0.6B: 0 reveals in the Spade Club scripts). When this rule finds a match, the game narrows
the schema's f enum to that one secret, so the model has to admit it, and r follows (f comes before r).

Topic words = the secret's `topic` text minus proper nouns (names appear in many unrelated lines) and filler,
plus anything quoted in the topic ('R.'), matched exactly.
Mirrors Source/Spade_mystery/Llm/LlmTextRules.cpp (TouchedSecret).
"""

import re

_FILLER = set("""about after again also anything because been before being between could does doing done during
each either every from going have having into just like made make many more most much must never only other over
said same should some such than that their them then there these they this those through very want what when where
which while whom whose will with without would your yours""".split())
_ASKING = re.compile(r"\?|^\s*(tell me|explain|describe|walk me through)\b", re.I)


def stem(w):
    w = w.lower()
    for suf in ("ings", "ing", "ies", "ied", "ed", "es", "s"):
        if w.endswith(suf) and len(w) - len(suf) >= 4:
            return w[: -len(suf)]
    return w


def topic_stems(topic):
    """Stems of the topic's content words: 4+ letters, not capitalised (names), not filler."""
    out = []
    for w in re.findall(r"[A-Za-z']+", topic):
        if len(w) < 4 or w[0].isupper() or w.lower() in _FILLER:
            continue
        out.append(stem(w))
    return out


def line_stems(line):
    return {stem(w) for w in re.findall(r"[A-Za-z']+", line) if len(w) >= 3}


def quoted_tokens(topic):
    """Quoted bits of a topic ('R.', "the Blue Room") are deliberate identifiers: matched exactly, any length."""
    pat = r"(?:^|\s)['\"‘“]([^'\"’”]{1,40})['\"’”](?=[\s,.;:!?)]|$)"  # opens after a space: "Augie's" is not a quote
    return [q.strip() for q in re.findall(pat, topic) if q.strip()]


def is_asking(line):
    """Secrets are admitted "if asked about <topic>": statements and accusations don't force a reveal."""
    return bool(_ASKING.search(line))


def touched_secret(line, available):
    """available = [(id, text, topic)]. If the line asks something, the id whose topic shares the most stems with
    the line (ties: first listed); otherwise None."""
    if not is_asking(line):
        return None
    words = line_stems(line)
    best, best_n = None, 0
    for sid, _text, topic in available:
        n = sum(1 for s in set(topic_stems(topic)) if s in words)
        n += sum(1 for q in quoted_tokens(topic) if q.lower() in line.lower())
        if n > best_n:
            best, best_n = sid, n
    return best

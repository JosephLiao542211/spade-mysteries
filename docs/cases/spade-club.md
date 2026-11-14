# Case file: The Last Hand at the Spade Club

The jam case. Data lives in `Content/Llm/Cases/SpadeClub/` (`case.json` + one file per suspect); this
document explains it for writers and designers. If you change numbers here, change the JSON too.

**Core idea: "Who do you fear more?"** Everyone saw something and nobody talks, because the killer is a
policeman. The player has to become someone the witnesses **trust more than they fear him**, or **fear more
than they fear him**, and every route has a cost.

## The truth (never shown to the model as a whole)

1931, an unnamed city, Prohibition. The Spade Club is an illegal card club in the basement of a closed laundry.
Owner **Augie Marsh** has paid off-duty officer **Dan Reilly** for "protection" for years, and Reilly kept
raising the price. Augie decided to take his ledger to a newspaper on Monday.

| Time | What happened |
|---|---|
| 11:00 | High-stakes poker game starts in the back room: Moss, Reilly, others. Vera sings in the front room. Lou runs the bank box by the door. |
| 11:20 | Augie, pale, tells Vera "He wants double this week" and goes to his office. |
| 11:30 | Reilly leaves the table "to make a phone call", goes out through the kitchen to the office's back door. |
| 11:35 | With the table short a player, Moss swaps in his marked deck and hides the old one in the card-table drawer. |
| ~11:38 | Office: Augie opens his safe to show Reilly the ledger and says he's going to the papers. Reilly kills him with the brass ashtray. A button tears off Reilly's coat. Reilly takes the ledger and the cash, and leaves his bloodied coat in the kitchen corridor. |
| 11:40 | Vera, fetching ice, sees Reilly come in through the kitchen door in shirtsleeves, in pouring rain. |
| 11:42 | Reilly sits back down and slips his hand into the card-table drawer, taking the ace of spades from Moss's hidden marked deck. |
| ~11:50 | Reilly "checks on Augie" and plants the marked ace under the body to frame the card sharp. |
| 11:55 | Lou takes the night's takings to the office, finds Augie, screams. |

**The red herring:** the ace of spades under the body is marked and comes from Moss's deck. Accusing
Moss loses the case.

## Evidence

| Id | What it is | How the player gets it |
|---|---|---|
| `ace_of_spades` | Marked ace under the body | Crime scene (start) |
| `marked_deck` | 51-card marked deck in the card-table drawer, ace missing | Crime scene (start) |
| `coat_button` | Brass police-coat button with a precinct number, office floor | Crime scene (start) |
| `brass_ashtray` | Murder weapon, wiped clean | Crime scene (start) |
| `ledger_carbon` | Carbon copies of the ledger: weekly payments to "R." | **Lou's secret** `carbon_copy` |
| `reilly_coat` | Wet police coat in the kitchen corridor, missing a button, stained cuff | **Vera's secret** `saw_reilly` |

## Suspects and how to crack them

Every suspect has **trust** and **fear** (0–100) plus one **personal drive**. The player's line is
classified into a tactic; code applies that suspect's effects for it. Secrets unlock when their condition
holds; only then are they added to the prompt (rule 1 of the plan).

### Vera Lake, torch singer (slot 0): trust and hope, never fear
- Start: trust 20, fear 10, **hope** 20 (dreams of leaving the city).
- Works: empathy (+12 trust, −5 fear), reassurance/promises (+10 trust, +15 hope, −10 fear), flattery.
- Backfires: threats (+25 fear, −15 trust), accusations. **While fear ≥ 40 she lies and admits nothing.**
- Fear above 40 also halves trust gains.
- **Repeating her words to another suspect costs her 35 trust.**
- Secrets:
  - `augie_scared` (trust ≥ 40): "He wants double this week."
  - `saw_reilly` (trust ≥ 65, hope ≥ 50, fear < 30): saw Reilly come in coatless at 11:40 → **grants `reilly_coat`**.

### Deacon Moss, card sharp (slot 1): pride, and fear only through his marked deck
- Start: trust 10, fear 0, **pride** 30.
- Works: challenging his skill or memory (+20 pride), sharp reasoning (+10 trust, +5 pride).
- Useless or worse: flattery (−8 trust, he sees through it), threats (bore him), empathy.
- **Bluffs:** claiming evidence you don't hold costs 20 trust and he tells you he caught you.
- Showing the **marked deck** sends fear to 80.
- Secrets:
  - `reilly_absent`: **pride ≥ 60** gives the full version (Reilly's seat empty 11:30–11:42, came back in
    shirtsleeves); **fear ≥ 70** gives only "Reilly left for a while, can't recall how long". The fear route is
    faster but weaker.
  - `drawer` (pride ≥ 80, trust ≥ 30): Reilly put his hand in the drawer with the marked deck.

### Lou Barrett, bookkeeper (slot 2): fear has a sweet spot, guilt opens the rest
- Start: trust 15, fear 10, **guilt** 20.
- Fear opens him fast: `r_payments` at fear ≥ 40 (or trust ≥ 50).
- **At fear ≥ 70 he panics: 3 turns of "I can't remember anything", then fear resets to 50.**
- Empathy (+8 trust, +15 guilt) is the way to the deeper secrets.
- Secrets:
  - `r_payments` (fear ≥ 40 or trust ≥ 50): the weekly "R." payments.
  - `newspaper` (guilt ≥ 60): Augie was taking the ledger to a reporter on Monday.
  - `carbon_copy` (guilt ≥ 60, trust ≥ 40): carbon copies in his bank box → **grants `ledger_carbon`**.

### Officer Dan Reilly, the culprit (slot 3): never threaten him, corner him with proof
- Start: trust 30, fear 0, **anger** 0, **cornered** 0. Friendly; treats the detective as a colleague.
- Threats (+25 anger), accusations (+15), challenging him (+10).
  **At anger ≥ 70 he walks out for good and leans on the witnesses (every other suspect −15 trust).**
- Only evidence corners him: coat button +25, ledger copies +30, wet coat +20, reasoning +5.
- **Telling him what a witness said** corners him (+10), but makes him angry (+10) and **puts that witness in
  danger** (their fear +25, trust −10).
- Secrets:
  - `protection` (cornered ≥ 50): admits the protection payments.
  - `safe_open` (cornered ≥ 80): **slips** that the safe was "already open" when he was in the office, which only
    the killer could know. **This is a confession.**

## Winning (the accusation)

Accusing anyone but Reilly loses. Accusing Reilly wins if **either**:
- he slipped (`safe_open` revealed), **or**
- the player holds a **motive** (`ledger_carbon`), **physical evidence** (`reilly_coat` or `coat_button`), and
  has a **witness who will testify**: Moss revealed `reilly_absent` and trusts the player ≥ 40, or Vera revealed
  `saw_reilly` and trusts the player ≥ 50. **A witness who no longer trusts the player takes back their statement.**

## Intended path (one of several)

1. Comfort Lou → he breaks under guilt, hands over the ledger copies (motive).
2. Reassure Vera and promise to get her out → she saw Reilly coatless, and the coat is found (physical evidence).
3. Needle Moss's pride → he confirms Reilly's empty seat (testimony).
4. Show Reilly the button, the ledger and the coat, and repeat Moss's account → he slips about the safe.
5. Accuse Reilly.

Mistakes have costs: threaten Reilly and the witnesses go cold; tell Reilly what Vera said and she gets scared;
bluff Moss and he stops respecting you; terrify Lou and he's useless for three questions.

## Data format (for new cases)

`case.json`: `setting`, `rules`, `tactics` (id → one-line definition, shown to the model), `emotions`,
`meter_words` (default words for trust/fear bands), `evidence[]`, `suspects[]` (file names), `accusation`.

Suspect file:
- `meters`: `{name: {start, words?: [[min, text], ...]}}`; trust and fear use the case's words if none given.
- `tactic_effects`, `evidence_effects`: `{id: {meter: delta}}`. `bluff_effects`, `bluff_note` (if set, the suspect notices bluffs).
- `fear_dampens_trust_above`: trust gains are halved while fear is at or above this.
- `gossip_penalty`: this suspect's trust loss when the player repeats their words to someone else.
  `gossip_heard.self` / `.source`: effects on this suspect and on the quoted suspect when the player quotes someone to *this* suspect.
- `rules[]`: `mode: "while"` (note and optional `block_secrets` while the condition holds) or `mode: "trigger"`
  (fires once when it becomes true: `shutdown_turns` + `after_shutdown_set`, `end_interview`, `others`).
- `secrets[]`: `id`, `topic` (what the detective must touch to get it), `text`, optional `grants_evidence`,
  `routes[]` of `{when, text?}`; the first matching route wins, and its `text` can be a weaker version.
- Conditions: `<meter>_gte`, `<meter>_lt`, `evidence_seen: [...]`, `revealed: [...]`; all must hold.

Test a change without the model: `python -I Tools/llm-harness/test_engine.py`.
Test it with the model: `python -I Tools/llm-harness/bench.py e4b-gpu` (scripts in `Tools/llm-harness/scripts/spade_club.json`).

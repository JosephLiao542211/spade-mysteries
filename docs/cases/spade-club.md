# Case file: Lights Out at the Black Spade

The jam case. Data lives in `Content/Llm/Cases/SpadeClub/` (`case.json` + one file per suspect); this
document explains it for writers and designers. If you change numbers here, change the JSON too.

**Core idea: "Four minutes of dark. Six people. Six lies."** You never leave the table. There is no room to
search: every clue is something a suspect tells you, and your only tool is what you can make them say and
whose words you repeat to whom. Each suspect shows two bars, like a character screen: **Fear**, and one
**personality bar** that is the key to them.

## The truth (never shown to the model as a whole)

1931, Prohibition. The Black Spade Club is a speakeasy and nightclub. Owner **Jimmy "Ace" Calloway** owned
Vera Lane's contract for twelve years. Tonight he was going to announce Allison Wu as the new headliner, and
he had sold Vera's contract to **Sal Varga**, a Chicago club owner Vera is terrified of.

| Time | What happened |
|---|---|
| 9:00 | Vera pays Tommy $20 to kill the lights at eleven "to ruin Ace's big announcement". |
| ~10:00 | Vera finds out about the contract; the back of the club hears her shouting at Ace. Allison takes Ace's real ledger from the office. |
| 10:20 | Vera has her usual gin behind the bar (the only person Mickey lets back there) and pockets the bar's .38 while he fetches ice. |
| 10:30 | Vera's set starts. |
| 10:45 | Graves leaves Ace's office after an argument about the cooked books. |
| **11:00** | **Tommy pulls the main fuse.** The band vamps in the dark; Vera slips off stage. Allison meets Graves in the cloakroom and hands him the ledger. Logan goes looking for his wife. |
| 11:01 | In the back hall, a woman in gardenia perfume brushes past Logan toward the back stairs. |
| 11:02 | Vera uses her staff key on the back stairs and shoots Ace at his desk under the drum solo. |
| 11:04 | Tommy restores the fuse. Vera is back on stage, singing, out of breath. Logan sees Allison and Graves leave the cloakroom. |
| 11:10 | Mickey takes up Ace's nightly rye, finds him, and finds the bar gun gone. |

**Red herrings, each with a real secret:** Mickey (his gun, a staff key, found the body), Graves and Allison
(a secret meeting in the murder window), Logan (a debt, a missing wife, a lie about his table), Tommy (he
caused the blackout).

## Statements (the only evidence)

The notebook starts empty. A secret, once admitted, adds one or more statements. Presenting a statement to
another suspect counts as **repeating that witness's words** (gossip), with whatever consequences that has.
Claiming a statement you don't have is a **bluff**.

| Id | From | What it says | Unlocked by |
|---|---|---|---|
| `stmt_tommy_fuse` | Mickey | Tommy fixed the lights in under a minute, from the back hall | `tommy_fuse` |
| `stmt_gun_missing` | Mickey | the bar .38 is gone | `gun_missing` |
| `stmt_vera_bar` | Mickey | only Vera was behind the bar, 10:20 | `vera_behind_bar` |
| `stmt_contract` | Graves | Allison's announcement; Vera's contract sold to Varga | `contract_sold` |
| `stmt_graves_cloakroom` | Graves | he was in the cloakroom with Allison; heels on the back stairs | `cloakroom` |
| `stmt_fuse_pulled` | Tommy | he pulled the fuse on purpose, 11:00–11:04 | `pulled_fuse` |
| `stmt_vera_paid` | Tommy | Vera paid him $20 to do it | `who_paid` |
| `stmt_ledger` | Allison | she gave Graves the ledger in the cloakroom | `ledger_theft` |
| `stmt_singing_stopped` | Allison | Vera's voice stopped in the dark; heels to the back stairs | `singing_stopped` |
| `stmt_gardenia` | Logan | a woman in gardenia passed him toward the back stairs | `went_looking` |
| `stmt_cloakroom` | Logan | he saw Allison leave the cloakroom with Graves | `went_looking` |
| `stmt_vera_went_up` | Vera | she went up the back stairs, "he was already dead" | `went_up` |

Everyone at the club knows Vera wears gardenia and Allison wears jasmine (it's in their character sheets).

## Suspects and how to crack them

The player's line is classified into a tactic; code applies that suspect's effects. Secrets unlock when
their condition holds, and only then are they added to the prompt.

### Vera Lane, torch singer (slot 0), the killer: Fear + **Composure** (lower is better)
- Start: fear 10, composure 80. She treats everything as a performance.
- **Traps:** flattery +10 composure; bare accusations and threats +5 (she has played tougher rooms);
  a caught **bluff** +15.
- Statements break her: Allison's singing −25, Tommy's $20 −30, Mickey's bar −20, Logan's gardenia −15,
  the contract −15. Logic −10, empathy −8 (twelve years is her wound).
- While composure < 50 her smile keeps slipping (state note).
- Secrets:
  - `went_up` (composure < 50): **a second lie**. She went up the back stairs but "he was already dead".
    Grants `stmt_vera_went_up`.
  - `confession` (composure < 20, fear ≥ 40, after `went_up`): the gun, the $20, Sal Varga.

### Mickey Doyle, bartender (slot 1): Fear + **Loyalty** to Ace
- Start: fear 55 (jumpy), loyalty 45. **While fear ≥ 70, loyalty gains are halved.**
- Works: reassurance (−15 fear), empathy, appeals to Ace (+10 loyalty).
- Backfires: threats (+20 fear), accusations (+25). **At fear 85 he panics for 3 turns, then fear resets to 60.**
- Hearing statements against Vera raises his loyalty to Ace (+10 to +20).
- Secrets:
  - `tommy_fuse` (loyalty ≥ 50, or fear ≥ 60).
  - `gun_missing` (loyalty ≥ 65, fear < 70).
  - `vera_behind_bar` (loyalty ≥ 80, fear < 70, after `gun_missing`, **and he has heard a statement against
    Vera**: Tommy's $20, Allison's singing, Logan's gardenia or Vera's own admission). Family is family.

### Harlan Graves, Ace's partner (slot 2): Fear + **Greed**
- Start: fear 5, greed 50. Threats bore him (+3 fear). Sees through bluffs (−10 greed, −10 fear).
- Works: deals (+20 greed), flattery (+5).
- Fear only through the cloakroom: Logan's statement +40, Allison's +45.
- Quoting anyone to Graves frightens the person quoted (+15 fear).
- Secrets:
  - `contract_sold` (greed ≥ 75): **the motive**.
  - `cloakroom` (fear ≥ 45): where he really was, and the heels on the back stairs.

### Tommy Mullen, driver and mechanic (slot 3): Fear + **Bravado**
- Start: fear 10, bravado 70. Dares (+12) and flattery (+10) make him brag.
- Mickey's statement deflates him (+35 fear, −40 bravado); so do logic and being linked to a murder.
- **At fear 85 he asks for a lawyer: 2 turns of silence, then fear 70.**
- Secrets:
  - `pulled_fuse` (bravado ≥ 90, he can't resist bragging; or fear ≥ 45).
  - `who_paid` (fear ≥ 55, bravado < 50, after `pulled_fuse`): Vera paid him. Bragging gets you the fuse;
    you have to break him for the name.

### Allison Wu, singer (slot 4): Fear + **Ambition**
- Start: fear 40, ambition 55. Career promises and deals (+10 to +15 ambition), flattery (+8).
- Threats and accusations +15 fear. **While fear ≥ 75 she denies everything.**
- Secrets:
  - `ledger_theft` (ambition ≥ 70, or fear ≥ 60 once she's exposed).
  - `singing_stopped` (ambition ≥ 80 and fear < 50): **she noticed Vera's voice stop**. She will knock Vera
    down if it helps her rise, but only when she feels safe.

### Logan Wu, Allison's husband (slot 5): Fear + **Temper**
- Start: fear 20, temper 30. Respect, empathy and reassurance lower his temper.
- **Quoting someone to him scares him (+15 fear, +5 temper) and puts the quoted person at risk (+20 fear).**
  Quoting Vera's claim that she saw him leave the back hall is the way in.
- Dares, accusations and anything about Allison and Graves raise his temper (+15 to +30).
- **While temper ≥ 60 he denies everything. At temper 80 he walks out and takes Allison with him**: both
  interviews close, and neither can testify.
- Secrets:
  - `ace_debt` (fear ≥ 35): Ace owed him $8,000, so he had no reason to want Ace dead.
  - `went_looking` (fear ≥ 40): the empty dressing room, the woman in gardenia, Allison and Graves.
    Grants `stmt_gardenia` and `stmt_cloakroom`. **Get this before you show him anything about the cloakroom.**

## Winning (the accusation)

- **Vera with a confession** → **Last Song**.
- **Vera with all three proofs** → **Case Closed**. Each proof needs one witness who revealed the secret, is
  still in the club, and still meets the condition, or they **take their statement back**:
  - a motive: Graves `contract_sold` (greed ≥ 40);
  - how she got the gun: Mickey `vera_behind_bar` (fear < 85, loyalty ≥ 60);
  - proof she left the stage: any of Vera `went_up`, Tommy `who_paid` (fear < 85), Allison
    `singing_stopped` (fear < 75), Logan `went_looking` (temper < 80, still in the club).
- **Vera without enough** → **No Case** (the text lists what was missing).
- **Graves** → **Bought and Paid For**. **Anyone else** → **The Wrong Man**.
- **Epilogues** are added by state: Logan furious (took Allison home) or calm (they leave together); Tommy
  charged as an accessory if he is still frightened; the Chicago outfit hook if Graves told you about the cloakroom.

## Chapters (how a playthrough tends to go)

1. **Closing Time.** The briefing (blackout, body, no weapon, six people), then Mickey as the tutorial: his
   Fear bar shakes, and calming him works better than pushing.
2. **Six Alibis.** Everyone's cover story. Vera points at Logan, and everything points at Logan or Mickey.
3. **Who Killed the Lights.** Mickey's fuse statement breaks Tommy's bravado; the blackout was planned.
4. **The Cloakroom.** Allison, Graves and Logan alibi each other once cracked, and give up the gardenia,
   the singing and the motive. Order matters: show Logan the cloakroom too early and he walks out with Allison.
5. **The Gun Under the Bar.** Mickey names Vera; Tommy names who paid him.
6. **The Song With No Break.** Vera, statement by statement: first the false story, then the confession.
7. **Last Call.** The accusation.

## Data format (for new cases)

`case.json`: `setting`, `rules`, `tactics` (id → one-line definition, shown to the model), `emotions`,
`meter_words` (default words for meters such as fear), `evidence[]` (`source` = a suspect id for statements),
`suspects[]` (file names), `accusation`.

Suspect file:
- `meters`: `{name: {start, label?, words?: [[min, text], ...]}}`. `label` is the bar's name in the UI.
- `tactic_effects`, `evidence_effects`: `{id: {meter: delta}}`. `bluff_effects`, `bluff_note` (if set, the suspect notices bluffs).
- `dampen`: `[{meter, when, factor}]`: positive changes to `meter` are multiplied by `factor` while `when` holds.
- `gossip_cost`: effects on this suspect whenever the player repeats their words to anyone.
  `gossip_heard.self` / `.source`: effects on this suspect and on the quoted suspect when the player quotes someone to *this* suspect.
- `rules[]`: `mode: "while"` (note and optional `block_secrets` while the condition holds) or `mode: "trigger"`
  (fires once when it becomes true: `shutdown_turns` + `after_shutdown_set`, `end_interview`, `close_others`, `others`).
- `secrets[]`: `id`, `topic` (what the detective must touch to get it), `text`, optional `grants_evidence` (a list),
  `routes[]` of `{when, text?}`; the first matching route wins, and its `text` can be a weaker version.
- Conditions: `<meter>_gte`, `<meter>_lt`, `evidence_seen: [...]`, `revealed: [...]`; all must hold.

`accusation`: `culprit`, `confession_secrets`, `proofs[]` (`label`, `witnesses[]` of `{suspect, secret, when?, recant?}`),
`endings` (`{id: {title, text}}`, with `{name}` and `{missing}`), `on_confession`, `on_proof`, `on_no_case`,
`on_wrong` (`{suspect id or "default": ending id}`), `epilogues[]` (`{suspect, when, text, endings?}`).

Test a change without the model: `python -I Tools/llm-harness/test_engine.py`.
Test it with the model: `python -I Tools/llm-harness/bench.py e4b-gpu` (scripts in `Tools/llm-harness/scripts/spade_club.json`).

# Finishing Spade Mystery

Written 2026-11-15 from the state of the repo on that day. This is the shortest path from "three separate
pieces" to "a build someone can download and play". Design detail stays in the two existing docs:
`docs/llm-implementation-plan.md` (the model side) and `docs/cases/spade-club.md` (the case).

## 1. Where things stand

The game exists as three pieces on three branches. **They have never been in the same build**, and `main`
is still the empty project.

| Branch | Owner | What it has | What it lacks |
|---|---|---|---|
| `noir-interrogation` | Joseph | The playable shell: 1-bit dither renderer, the room, six video suspects, briefing → talk → accuse → ending. Runs in the editor (on a Mac). | Dialogue is three fixed questions with fixed answers. The culprit is a placeholder ("Tim"). No LLM code. |
| `llm-spike` (this checkout) | Anwar | The brain: llama-server sidecar, the interrogation engine, the six-suspect Black Spade case, Python harness. Engine tests pass (41/41); E4B gets 14/14 secrets in the scripted runs. | The C++ (~2,000 lines) **has never been compiled**. Only a Slate debug window, no game UI. |
| `llm-lowspec-guard` | Reaper8202 | Text rules that make small models usable, case validator, a 1.1 GB model option. | Built on the **old four-suspect case**. C++ never compiled. |

Two things to know before anything else:

- **The Black Spade rewrite is uncommitted.** 24 modified files plus two new suspect files exist only on this
  disk. Until it is pushed, nobody else can integrate against the real case, and one disk failure loses it.
- **Joseph's story and the case file disagree.** His controller has Tim the bellboy as the killer and a
  three-suspect briefing. The case file has Vera as the killer, Mickey Doyle the bartender in that seat, six
  suspects, and the body found at 11:10. The case file wins (it is the one with tests); the shell's text goes.

## 2. What "finished" means

A Windows build where a player can:

1. Read the briefing and sit down across from any of the six suspects.
2. **Type** a line and get an in-character reply within a few seconds.
3. See each suspect's two bars move, and see how their line was read.
4. Collect statements in a notebook and repeat them to other suspects.
5. Accuse someone and get one of the five endings, then restart.

Everything else is polish. If time runs out, steps 1–5 on one decent GPU is a submittable game.

## 3. The work, in order

Each step unblocks the next. Do not start polish (step 7) while step 3 is open.

### Step 0. Push the rewrite (30 minutes)

- [ ] Commit the working tree on `llm-spike` and push. Note the old suspect files are deleted and two are new.
- [ ] Tell Joseph and Reaper8202 the case is now six suspects with statements as evidence.

### Step 1. One branch with everything (1–2 hours)

- [ ] Create `integration` from `noir-interrogation`, merge `llm-spike` into it.
- [ ] Resolve the three conflicts (checked with `git merge-tree`; everything else merges cleanly):
  - `Spade_mystery.Build.cs`: keep both dependency lists. Public needs `Core, CoreUObject, Engine, InputCore,
    EnhancedInput, DeveloperSettings, AnimationCore, ChaosClothAssetEngine, MediaAssets`; private needs
    `HTTP, Json, Sockets, Slate, SlateCore`; keep the Win64 staging block.
  - `Config/DefaultGame.ini`: keep Joseph's Noir Look block **and** the `DirectoriesToAlwaysStageAsUFS=Llm`
    and `LlmSettings` lines.
  - `.gitignore`: keep both sides (`Models/`, `*.gguf`, `ThirdParty/llama/` must stay ignored).
- [ ] `DefaultEngine.ini` comes from Joseph's branch untouched. It already turns off ray tracing and Lumen,
  which the LLM plan asked for.
- [ ] From here on, everyone branches from `integration`.

### Step 2. First compile (half a day; the riskiest step)

Nobody has compiled the `Llm/` folder. Expect a batch of ordinary errors (missing includes, API names).

- [ ] Find the machine: **Windows, UE 5.8, a GPU with ≥ 6 GB**. This PC has the models and llama binaries but
  no Unreal at the default install path. Joseph has Unreal but on a Mac.
- [ ] Build the editor target, fix errors until it links.
- [ ] Run `Llm.DebugChat` in Play mode. Pass = Vera answers a typed line and her meters change.
- [ ] Mac developers: the sidecar launch is Windows-only (`llama-server.exe`, Job Object). On a Mac set
  `bLaunchServer=False` in Local LLM settings and run llama.cpp's Metal `llama-server` by hand on port 8080.
  That keeps Joseph able to work on UI against the real model without a Mac port of the launcher.

### Step 3. Wire the brain into the shell (1–2 days; the actual game)

All of this is in `NoirPlayerController` and `NoirHUD`. The session API is already shaped for it
(`UInterrogationSession::Ask / Present / Accuse`, `OnReply` with `FInterrogationReply`).

- [ ] **Case from data.** Delete the hard-coded `FNoirSuspect` answers, `GetQuestions()` and `Culprit`. Fill
  names and roles from `Session->GetCase()`. Seat order must match the suspects' `slot`: Vera 0, Mickey 1,
  Graves 2, Tommy 3, Allison 4, Logan 5. Seat 2's clip is "Tim"; the name on the plate becomes Mickey Doyle.
- [ ] **Loading screen.** Hold on the briefing until the server is `Ready` and `PreloadAll()` has run. On
  `Failed`, show `GetLastError()` instead of hanging. First launch on a new PC takes ~20 s (shader compile).
- [ ] **Typed input.** Replace the three question rows with one input line drawn by the HUD.
  Gotcha: T, A, D, X, M, R, Y, N and 1–3 are bound as hotkeys today. They must be ignored while the input
  line has focus, or typing "that" closes the panel. Enter sends, Escape leaves the line.
- [ ] **Async replies.** `Ask()` returns at once. Add the player's line to the log, show a "thinking" state
  while `IsBusy()`, add the reply in `OnReply`. The typewriter already works off the line's timestamp.
  An invalid reply (`bOk == false`) changes nothing: show "No answer. Try again."
- [ ] **Two bars per suspect** from `Reply.Meters`, labelled from the suspect JSON (Fear + Composure, Loyalty,
  Greed, Bravado, Ambition, Temper).
- [ ] **"How your line was read."** One line under the transcript from `PlayerTactic` ("Vera took that as a
  threat"). About one line in seven is misread; without this the bars look random.
- [ ] **Clue card + notebook.** On `RevealedSecretId`, show `RevealedText` (the canonical wording, not the
  model's). List the statements in `GetState().Inventory`; picking one calls `Present()` on the current suspect.
- [ ] **Closed interviews.** `bInterviewClosed` and the shutdown rules (Mickey panics, Tommy lawyers up, Logan
  walks out with Allison) need a visible state: empty chair or a "won't talk" plate, input disabled.
- [ ] **Accusation.** `Session->Accuse(id)` returns the title and text for all five endings plus epilogues.
  Replace `Solved/Failed` and `GetEnding()` with one ending screen. Restart builds a fresh session.
- [ ] **Briefing text.** Six suspects, body found at 11:10, no weapon. Take it from `case.json` `setting`.
- [ ] **Faces follow the model.** Map the reply's `e` (calm, angry, sad, …) to the slot's mood clip and remove
  the MOOD [M] button. Only Vera has mood clips today; the others just keep their idle.

### Step 4. Play it start to finish (half a day)

- [ ] One full human playthrough to **Case Closed** and one to **Last Song**, in the real UI, on E4B.
- [ ] One deliberately bad playthrough: threaten everyone, bluff, make Logan walk out. Nothing should lock up.
- [ ] Fix what the playthroughs find before touching anything in step 6 or 7.

### Step 5. Package and test on another PC (half a day)

- [ ] Package for Windows. Check on disk next to the exe: `ThirdParty/llama/…`, `Models/*.gguf`,
  `Content/Llm/…`, and **`Content/Movies/*.mp4`**. The suspects are loose mp4 files referenced by path; nothing
  in the config stages that folder yet, so add it to the packaging settings or the chairs will be empty.
- [ ] Run the packaged build on a PC that has never had the project. This is where missing DLLs, the port,
  the firewall prompt and the 20-second first launch show up.
- [ ] Decide the download: both models is ~8 GB. E4B only is ~5 GB and drops the CPU fallback.

### Step 6. Weak-PC support (only if steps 1–5 are done)

- [ ] Port the text rules from `llm-lowspec-guard` onto the six-suspect case. It edits the same seven files as
  the rewrite, and its patterns and scripts name the old suspects, so this is a port, not a merge.
- [ ] Re-run `bench.py e4b-gpu --assist` to confirm the rules don't hurt E4B (untested).
- [ ] Pick the CPU-tier model (E2B at 3.1 GB, or Qwen3 1.7B at 1.1 GB with the rules) on a real laptop.
- [ ] Hardware tier detection at startup. Until then the existing "GPU, else CPU build" fallback is the tiering.

### Step 7. Polish, in value order

- [ ] **Sound.** There is no audio in the project at all. A room tone, a muffled band and a typewriter tick
  would do more for this game than any other single item on this list.
- [ ] Streaming replies, so text starts in under a second (matters most on CPU).
- [ ] Mood clips for the other five suspects.
- [ ] A title card and a one-screen "how to play" (Mickey as the tutorial: calming him works, pushing doesn't).
- [ ] Voice in/out. Cut unless everything above is done.

### Step 8. Submit

- [ ] Merge `integration` to `main`, tag the commit the build came from.
- [ ] Store page: the screenshots already taken (`image1–8.png` in the folder above the repo), a short clip,
  the minimum spec, and the download size stated up front.
- [ ] Credits and licences: llama.cpp, the Gemma model terms, MetaHuman, the generated suspect art.
- [ ] Check the jam's rules on large downloads and AI-generated art before upload day, not on it.

## 4. If time runs short, cut in this order

1. Voice (never start it).
2. Weak-PC tiers and the text rules: state "needs a 6 GB GPU" and ship E4B only.
3. Mood clips beyond Vera, streaming.
4. Notebook as a nice panel: a plain numbered list of statements is enough.
5. Epilogues.

Never cut: typed input, the two bars, "how your line was read", the clue card, a failure screen when the
model does not start. Without those the game is either not this game or looks broken.

## 5. Risks

| Risk | Why it is real | What to do |
|---|---|---|
| The rewrite is lost or diverges | It is uncommitted on one PC | Step 0, today |
| First compile eats a day | ~2,000 lines of C++ nobody has built | Start step 2 as soon as step 1 lands; one person on it full time |
| No Windows + UE 5.8 + GPU machine | The sidecar only launches on Windows; the shell is developed on a Mac | Name the machine and its owner today |
| Works in the editor, not packaged | Models, llama binaries and mp4s all live outside the .pak | Step 5 early, not on the last day |
| Typing fights the hotkeys | Letter keys are bound to actions | Handle in the same change as the input line |
| Players without a GPU | CPU replies are ~5 s on a fast desktop, slower on laptops | State a minimum spec; step 6 only if there is time |

## 6. Decisions the team has to make

1. **When is the deadline?** Steps 0–5 are roughly three to four working days for one person, less if the UI
   (step 3) and the compile (step 2) run in parallel.
2. **Who owns which step?** Suggested: Anwar steps 0–2 and the session side of 3; Joseph the HUD side of 3 and
   step 7; Reaper8202 step 6.
3. **Which machine is the build machine?**
4. **Minimum spec and download size** (step 5).
5. **Mickey or Tim?** The plan above keeps "Mickey Doyle" on Tim's clip, because the case, tests and scripts
   all use that name. Renaming is possible but touches every one of them.

## 7. Commands

```
python -I Tools/llm-harness/test_engine.py          # rules only, instant, no model
python -I Tools/llm-harness/bench.py e4b-gpu        # scripted interrogations against the model
git merge-tree --write-tree --name-only HEAD origin/noir-interrogation   # preview the step 1 conflicts
```

In the editor: console command `Llm.DebugChat` opens the debug interrogation window.

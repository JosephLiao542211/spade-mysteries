# Local LLM Implementation Plan

How the suspects' dialogue will run on a local model inside Spade Mystery (UE 5.8).
Status: Day-1 spike done for the model side (results in §13); Unreal code written but not yet compiled.
Researched 2026-11-13.

## 1. Where the repo stands

- Blank UE 5.8 C++ project: one runtime module (`Spade_mystery`), no content, no gameplay code.
- `DefaultEngine.ini` has hardware ray tracing, Lumen, Substrate and the **Maximum** graphics preset on.
  This matters: Unreal and the model share the same video memory (see §7).
- Git LFS is set up for assets, but **no rule for `.gguf`**, and GitHub LFS caps single files at 2 GB on
  free plans. Model files must not be committed (see §6).

## 2. Architecture

```
┌──────────────────────── Unreal (game process) ────────────────────────┐
│ UInterrogationSubsystem   game truth: pressure, facts, claims, history │
│        │ builds prompt + per-request JSON schema                       │
│ FPromptBuilder ──► ULlmClient ──HTTP (127.0.0.1)──┐                    │
│ ULlmServerSubsystem  launches / watches / kills   │                    │
└───────────────────────────────────────────────────┼────────────────────┘
                                                    ▼
                 llama-server.exe (llama.cpp sidecar, OpenAI-compatible API)
                 model: gemma-4-E4B-it GGUF (+ mmproj only if we use audio)
```

**Why a sidecar process, not an in-engine plugin:** a model crash can't take down the game, we can
swap llama.cpp builds (CUDA / Vulkan / CPU) without recompiling Unreal, and developers can point the
game at a llama-server they run by hand while iterating on prompts. In-process plugins tie us to the
plugin's llama.cpp version and its UE 5.8 compatibility.

## 3. The sidecar (`ULlmServerSubsystem`, a `UGameInstanceSubsystem`)

Launch on game start, behind the loading screen. Model, `-ngl`, threads and context come from the
hardware tier chosen at startup (§7). Example for the top tier:

```
llama-server.exe -m Models/gemma-4-E4B-it-Q4_K_M.gguf
  --host 127.0.0.1 --port <free port>
  -ngl 99 -fa on
  -ctk q8_0 -ctv q8_0            # quantized KV cache: about half the memory, needs flash attention
  -c 8192 --parallel 4           # 4 slots x 2048 tokens: one per suspect; size to the measured need
  -t <physical cores - 2>        # leave CPU for the game thread
  --jinja --no-webui
  --reasoning off --reasoning-budget 0   # required: see §4
```

- **Health:** poll `GET /health` until 200, then unblock the interrogation UI. Timeout → retry one tier
  lower (finally `-ngl 0`, CPU only) → if that fails, show an error screen. Never hang.
- **Pre-load suspects:** while the loading screen is still up, send each suspect's static prefix once
  (`max_tokens: 0`, own `id_slot`) so the first question to every suspect is fast.
- **Lifetime:** on Windows, put the child in a Job Object with `KILL_ON_JOB_CLOSE` so the server dies
  even if the game crashes. Also terminate it in `Deinitialize()`.
- **Port:** try a fixed port (for example 39281); if busy, step through a small range.
- **Dev override:** `[Llm] bLaunchServer=false, ServerUrl=http://127.0.0.1:8080` in config so the
  editor talks to a hand-started server.
- **Build choice:** ship the **Vulkan** build by default. It runs on NVIDIA, AMD and Intel and needs no
  CUDA runtime DLLs (hundreds of MB). Benchmark CUDA vs Vulkan on Day 1; ship the CUDA build only if
  it's meaningfully faster on judge-class hardware. Current Windows assets on the llama.cpp releases page:
  `win-cpu`, `win-vulkan`, `win-cuda-12.4`/`13.5` (+ `cudart-*`), `win-rocm`, `win-sycl`.
- **Pin one llama.cpp build** for the whole jam and record its tag in this doc. The API and slot-cache
  behaviour have changed between releases.

## 4. Requests (`ULlmClient`)

One `POST /v1/chat/completions` per player line. Add `HTTP`, `Json`, `JsonUtilities` to `Spade_mystery.Build.cs`.

```jsonc
{
  "messages": [...],                // see §5
  "id_slot": 2,                     // suspect's own slot, keeps their prompt prefix cached
  "cache_prompt": true,
  "temperature": 0.8,
  "max_tokens": 120,
  "response_format": { "type": "json_schema", "json_schema": { "schema": { ... } } }
}
```

**The schema is built fresh for each request** so the model physically cannot name a fact the game
hasn't made available:

```jsonc
{
  "type": "object",
  "properties": {
    "t": { "enum": ["small_talk","question","empathy","intimidate","bribe",
                    "logic","accuse","off_topic_or_meta"] },        // player's tactic
    "e": { "enum": ["calm","nervous","angry","sad","smug","afraid"] }, // emotion
    "r": { "type": "string", "maxLength": 300 },                   // reply
    "f": { "enum": ["none", "saw_julian", "left_kitchen"] }        // fact the reply admits: only currently AVAILABLE facts
  },
  "required": ["t","e","r","f"]
}
```

- Keys are one letter because every output token costs time on weak machines (§7.4). Code maps them to
  readable names (`PlayerTactic`, `Emotion`, `RevealFact`, `Reply`).
- Field order is deliberate: classifying the tactic first acts as a short "think" step; the reply comes
  next so the typewriter can start as soon as `"r":"` appears in the stream; the reveal flag comes **after**
  the reply so it describes what was actually said. (Spike: with `f` before `r`, E2B flagged a reveal its
  reply never made.)
- Define each tactic in one line in the static prompt (see `Content/Llm/case.json`). Without definitions,
  E2B labelled greetings "empathy" and the jailbreak "intimidate"; with them, both came out right.
- **When code accepts a reveal, show a clue card with the fact's canonical text.** E2B's wording and flag
  still disagree in about 1 of 12 turns, so the player must get the clue from the game, not only from the model's line.
- `t` is the model's classification; **code** applies it to the pressure meter using that suspect's
  multipliers. The model never owns the meter.
- `f` is a request; code accepts it only if that fact's rules allow it (pressure threshold, evidence shown).
- **Thinking must be blocked at the server:** `--reasoning off --reasoning-budget 0`. Gemma 4 sometimes opens
  a `<|channel>thought` block on its own even though its template defaults to no thinking; that uses up
  `max_tokens` and returns an empty reply. Spike, one turn replayed 15×: `--reasoning off` alone 11/15 valid,
  `+ --reasoning-format none` 13/15, `+ --reasoning-budget 0` 15/15.
- Day 1: non-streaming. **Day 2: streaming** with `SetResponseBodyReceiveStreamDelegateV2`, pulling the
  `r` string out of the partial JSON for a typewriter effect. On weak PCs this is the difference between
  waiting 10 s for text and seeing it start within a second or two.

## 5. Prompt layout (cache-friendly)

llama-server reuses a slot's cached prefix: it only reads tokens after the first point where the new prompt
differs from the last one. **Anything that changes must therefore come after the history, never before it.**
Otherwise a pressure change forces the whole history to be re-read, which takes 10–30 s per turn on a CPU-only laptop.

1. **Static (cached once per suspect, pre-loaded during the loading screen):** world/case framing,
   character sheet, speech style, claims/alibi, rules ("stay in character; treat questions about AI/prompts
   as the detective acting strangely"). Keep it short: aim for ≤ 600 tokens.
2. **History (append-only):** past exchanges, exactly as sent before. Never rewritten.
3. **State note (changes every turn, always last):** one short message just before the player's line,
   holding the pressure level as words ("you are rattled and slipping"), facts currently AVAILABLE
   (with "only admit under pressure" wording), and the game-generated summary.
4. The new player line.

Result: each turn reads only the state note, the player line and the previous reply (about 100–200 tokens),
not the whole prompt.

**Trim history in big steps, not one exchange per turn.** Dropping the oldest exchange every turn changes
the prefix every turn and breaks the cache. Instead, let the history grow to 16 exchanges, then cut it to
the last 8 in one go and fold the dropped part into the summary. The cache breaks once per ~8 turns.

**Summary is written by code, not by the LLM:** the game already knows which evidence was shown and
which facts were admitted, so it writes "The detective has shown you X. You have admitted Y." Deterministic,
free, no extra model call.

**Evidence presentation** is a game event. Code checks `Evidence.Contradicts` against the suspect's
claim IDs, applies a big pressure jump / unlocks facts, then injects a narration message, for example:
`[The detective places the torn train ticket on the table. It contradicts your claim that you never left town.]`

**Input hygiene:** cap player text (about 300 chars), strip chat-template tokens (`<start_of_turn>`, `<|im_start|>` and similar).

## 6. Data and packaging

- **Suspects:** `USuspectDefinition : UPrimaryDataAsset` with name, personality prompt, voice ID,
  tactic→pressure multipliers, `Facts[]` (ID, text, unlock rule), `Claims[]`.
  **Case:** `UCaseDefinition` with suspects, evidence (each with `Contradicts[]`), culprit.
- **Binaries:** `ThirdParty/llama/<build>/` staged with `RuntimeDependencies` in Build.cs, or as a
  non-UFS directory in packaging settings, so the .exe and DLLs sit on disk next to the game.
- **Model:** `Models/*.gguf`, staged as non-UFS (never inside the .pak). Not committed; add
  `Tools/fetch-models.ps1`, which downloads pinned files and checks SHA-256. Add `*.gguf` to `.gitignore`.
- **Mac:** the `Build/Mac` folder is template boilerplate. If anyone develops on Mac, the same sidecar
  design works with llama.cpp's macOS Metal build.

## 7. Low-spec strategy

Players will have far weaker machines than the dev PCs (2× RTX 3090). Everything here is tuned for the
weakest supported tier, not for the dev PCs.

**Proposed minimum spec (to confirm with the team):** a 6 GB GPU, or a CPU-only machine with 16 GB RAM,
with replies finishing in under 5 s.

### 7.1 Graphics settings compete with the model

- Ray tracing + Lumen + Maximum preset (the current `DefaultEngine.ini`) can take most of an 8 GB card
  by themselves, and a GPU busy rendering at full frame rate makes the model generate much more slowly.
- **Turn off hardware ray tracing** and lower the default scalability. Interrogation rooms don't need it.
- **Cap the frame rate to ~30 fps during interrogations** (`t.MaxFPS`), and lower it further while a
  reply is generating, to give the GPU back to the model.

### 7.2 Hardware tiers, chosen at startup

Detect free video memory (DXGI) and system RAM, then pick a tier. Store the choice in user settings and
allow overriding it in the options menu.

| Tier | Hardware | Model | Server flags |
|---|---|---|---|
| High | ≥ 4.5 GB video memory free after the game loads (E4B measured at 3.2 GB) | Gemma 4 E4B Q4_K_M | `-ngl 99` |
| Medium | ≥ 2.5 GB free (E2B measured at 1.6 GB) | Gemma 4 E2B Q4_K_M | `-ngl 99` |
| Integrated | Intel/AMD integrated graphics | Gemma 4 E2B Q4_K_M | Vulkan build, `-ngl 99` (often faster than CPU) |
| CPU | Anything else, ≥ 16 GB RAM | Gemma 4 E2B Q4_K_M | `-ngl 0`, `-t <physical cores - 2>` |

- **Never split a model between GPU and CPU.** Spike: E4B with 16 layers on the GPU took 6.2 s per reply,
  no faster than CPU-only (6.6 s). If E4B doesn't fit fully, run E2B fully on the GPU (0.5 s).
- **CPU tier must be E2B.** On 4 threads of a fast desktop CPU (Ryzen 9 7950X), E2B took 3.7 s per reply
  and E4B 6.6 s. A typical laptop will be roughly 2–3× slower.
- **Never go below Q4 quantization.** A 4B model at Q3 gets noticeably worse; E2B at Q4 beats E4B at Q3.
- Ship both E4B and E2B files, or download only the tier's model on first launch to cut install size.

### 7.3 Model memory

- **Measured file sizes:** E4B Q4_K_M is **4.98 GB** (not ~3 GB); E2B Q4_K_M is 3.11 GB.
  In E4B, `per_layer_token_embd` alone is 1.94 GB (39% of the file).
- **Per-layer embeddings already stay in system RAM:** E2B (3.1 GB file) uses only ~1.6 GB of video memory
  with `-ngl 99`, and adding `--override-tensor per_layer_token_embd=CPU` changed nothing. No flag needed.
- Quantized KV cache (`-ctk q8_0 -ctv q8_0`) and a context sized to the measured need (start at
  4 × 2048 tokens, not 16k).

### 7.4 Fewer tokens, and hide the wait

- On a CPU, a 4B model writes roughly 8–15 tokens/s, so a 100-token reply takes 7–12 s. Every token counts.
- Short JSON keys (`t`, `e`, `r`, `f`) and replies capped at 2 sentences; `max_tokens` about 120.
- **Stream replies from Day 2** with a typewriter effect, so text appears after the first few tokens.
- Suspect "thinking" idle animation (fidget, sip, glance away) while the reply is generating.
- No extra model calls for anything code can do (tactic handling, summary, contradiction checks).

### 7.5 Prompt processing

- Short static prefix (≤ 600 tokens), pre-loaded per suspect during the loading screen (§3).
- Append-only history with the state note last, trimmed in big steps (§5).
- More than 4 suspects: use `/slots/{id}?action=save|restore` (needs `--slot-save-path`) to swap
  caches, or accept a slower first reply after switching suspects.

### 7.6 Voice on low tiers

- Speech-to-text and text-to-speech run on the same machine and compete with the model.
  Voice is **off by default on the Integrated and CPU tiers** (player can still enable it).

### 7.7 Testing like a weak PC on the dev machines

- CPU-only laptop: `llama-server -ngl 0 -t 4`.
- Card that fits only part of the model: `-ngl 16`.
- Run Unreal on Low scalability at the same time, then measure time to first token and time to full reply.
- Confirm on at least one real weak PC (laptop with integrated graphics if possible) before submission.

## 8. Voice (stretch goal, after text works end to end)

- **Speech to text:** llama.cpp merged Gemma 4 E2B/E4B audio input in April 2026 (PR #21421), using
  the separate `mmproj` file. Known caveats: BF16 mmproj recommended, 16 kHz audio, clips above about
  17 s less reliable. It's documented for `llama-mtmd-cli`; **verify llama-server accepts `input_audio`**
  with our pinned build. If yes, run a transcribe-only request, show the text, then feed it into the
  normal flow. If not, fall back to `whisper-server` (whisper.cpp) with a small English model.
- **Text to speech:** Piper's maintained repo (`OHF-Voice/piper1-gpl`) is **GPL-3.0**. Running it as a
  separate executable is fine for a jam entry, but check each voice's licence (some are non-commercial).
  Kokoro (Apache-2.0, 82M) is the permissive alternative if licensing matters.

## 9. Model choice

- **Primary:** Gemma 4 E4B instruct, Q4_K_M.
- **Head-to-head on Day 1:** Qwen 3.5 4B (Apache-2.0, Q4_K_M ≈ 2.7 GB, community GGUFs from unsloth and others).
- **Low-spec fallback:** Gemma 4 E2B.
- Judge on: valid-JSON rate with our schema, staying in character over 20 turns, resisting
  "ignore your instructions", tokens/s on the weakest PC.

## 10. Code layout

```
Source/Spade_mystery/
  Llm/LlmServerSubsystem.{h,cpp}       launch, health, port, kill
  Llm/LlmClient.{h,cpp}                HTTP + JSON, schema building, response parsing
  Interrogation/SuspectDefinition.h    data assets
  Interrogation/CaseDefinition.h
  Interrogation/InterrogationSubsystem.{h,cpp}   pressure, facts, history, evidence events
  Interrogation/PromptBuilder.{h,cpp}
Tools/
  fetch-models.ps1                     pinned downloads + SHA-256
  llm-harness/                         run suspect prompts against llama-server outside Unreal
```

The **prompt harness** (small Python or PowerShell script that reuses the same prompt templates) lets
writers tune personalities without launching the editor. Keep the templates in plain text files that
both the harness and Unreal read.

## 11. Milestones

**Day 1: spike**
1. Hand-run llama-server with Gemma 4 E4B, Gemma 4 E2B and Qwen 3.5 4B; curl the JSON schema; record
   JSON validity, real file sizes and tokens/s, both on GPU and with the weak-PC simulation (§7.7).
2. Test keeping per-layer embeddings in system RAM (`--override-tensor`) and the quantized KV cache; record video memory saved.
3. Turn off hardware ray tracing and lower default scalability in `DefaultEngine.ini`.
4. `ULlmServerSubsystem` launches the server from a packaged path; debug widget chats with one hard-coded suspect.
5. Package a build, copy it to the weakest PC, measure latency and video memory with the game running.

**Day 2: weak-PC essentials**
6. Hardware tier detection and per-tier launch flags (§7.2), with fallback to the next tier down.
7. Streaming replies + typewriter + suspect "thinking" animation.
8. Suspect pre-loading during the loading screen; frame-rate cap during interrogations.

**Days 3–5: interrogation loop**
9. Suspect/Case data assets, pressure meter, fact gating, per-suspect slots, append-only history with big-step trimming.
10. Evidence presentation + contradiction checks.
11. Accusation flow and ending.

**Afterwards:** voice in/out (off by default on low tiers), prompt tuning with the harness, re-test on a real weak PC.

## 12. Open questions for the team

- Minimum spec we promise judges. Proposal: 6 GB GPU, or CPU-only with 16 GB RAM and replies under 5 s.
- Is dropping hardware ray tracing / Maximum preset acceptable for the art direction?
- Ship both E4B and E2B in the download, or fetch only the tier's model on first launch?
- Which real weak PC (laptop?) can we test on before submission?
- How many suspects per case (affects slot count / context size).
- Is voice in scope for the submission, or only a stretch goal?
- Who writes the case and character sheets, and in what format they hand them over.

## 13. Day-1 spike results (2026-11-13)

Setup: llama.cpp b12368 (Vulkan and CPU builds), unsloth Q4_K_M GGUFs, one RTX 3090 / Ryzen 9 7950X.
Test: `Tools/llm-harness/bench.py`, a scripted 12-turn interrogation of Edith Vane (`Content/Llm/`), including a
jailbreak line and an intimidation line. Context 4 × 2048, q8_0 KV cache, `--reasoning off --reasoning-budget 0`.
Raw results and transcripts: `Tools/llm-harness/results/`.

| Config | Video memory | Valid JSON | Tokens read / turn | Generation | Time / reply (avg / max) |
|---|---|---|---|---|---|
| E4B, GPU | 3.2 GB | 12/12 | 157 | 104 tok/s | 0.84 s / 0.92 s |
| E2B, GPU | 1.6 GB | 12/12 | 144 | 162 tok/s | 0.47 s / 0.54 s |
| E4B, 16 layers on GPU | 1.4 GB | 12/12 | 151 | 11.5 tok/s | 6.2 s / 7.6 s |
| E4B, CPU 4 threads | 0 | 12/12 | 152 | 13 tok/s | 6.6 s / 7.6 s |
| E2B, CPU 4 threads | 0 | 12/12 | 153 | 25 tok/s | 3.7 s / 4.2 s |
| E4B, GPU, **old prompt order** | 3.2 GB | 12/12 | 303 (spikes to 1,119) | 99 tok/s | 0.85 s / 1.04 s |

**What we learned**
1. **Thinking must be blocked at the server** (`--reasoning-budget 0`), or about 1 in 4 replies comes back empty (§4).
2. **The prompt-order fix works.** With the state note last, tokens read per turn stay at 126–211. With the
   old order, every pressure change forces a full re-read (511, 911, 623, 1,119 tokens). That is invisible on
   a GPU but costs about 12 s on that turn on a CPU (prompt reading ran at ~90 tok/s on 4 threads).
3. **E4B is clearly the better actor.** All 12 tactic labels were right in both runs, the voice is richer, and
   it treats the jailbreak as odd behaviour ("Are you quite well?"). E2B mislabels about 3 in 12 and is blunter.
4. **E4B guards secrets harder and paraphrases them.** It never admitted the 70-pressure secret even at
   pressure 80, and its admitted clue drifted ("near ten, west wing" vs "9:40, leaving the study"). Needed:
   the clue card with canonical text (§4), and a stronger state-note nudge when the suspect is "close to breaking".
5. **The reveal flag must come after the reply** in the schema (§4).
6. **Per-layer embeddings already stay in RAM**; `--override-tensor` saves nothing (§7.3).
7. **Splitting a model between GPU and CPU is useless** (§7.2).
8. **First request after the very first launch took ~20 s** (Vulkan shader compile); later launches 0.1–0.2 s.
   The loading screen must cover this on a player's first run.
9. **Model downloads:** single-connection downloads from Hugging Face ran at ~2 MB/s; 6 parallel range
   requests ran at ~50 MB/s. `Tools/fetch-models.ps1` should switch to parallel ranges.

**Unreal side:** `Source/Spade_mystery/Llm/` has the server subsystem, prompt builder, chat session and a
Slate debug window (console command `Llm.DebugChat`). **Not compiled yet**: Unreal is not installed on the
machine the spike ran on. Next: build on a machine with UE 5.8, run `Llm.DebugChat` in PIE, then package
and test on a weak PC.

## Sources

- llama.cpp Gemma 4 support (vision, no audio): https://github.com/ggml-org/llama.cpp/pull/21309
- llama.cpp Gemma 4 audio encoder: https://github.com/ggml-org/llama.cpp/pull/21421
- llama.cpp releases: https://github.com/ggml-org/llama.cpp/releases
- KV cache reuse / slot save-restore: https://github.com/ggml-org/llama.cpp/discussions/13606
- Structured output in llama.cpp: https://docs.liquid.ai/deployment/on-device/llama-cpp/structured-output
- Gemma 4 local running notes: https://unsloth.ai/docs/models/gemma-4
- Qwen 3.5 4B GGUF: https://huggingface.co/unsloth/Qwen3.5-4B-GGUF
- UE streaming response delegate: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/HTTP/IHttpRequest/SetResponseBodyReceiveStreamDele-
- Piper vs Kokoro licensing: https://www.promptquorum.com/power-local-llm/piper-vs-kokoro-tts

/* Voice for the interview room, all on this PC.
   Speaking: each new line a suspect says is fetched from /api/voice/speak (Kokoro) and played.
   Hearing:  the Speak button records the microphone, posts the growing clip to /api/voice/hear (whisper.cpp)
             about once a second, and writes what it heard into the question box as the detective talks.
   If the voice files are not installed the buttons never appear and the game is typed, as before. */
(() => {
  "use strict";
  const $ = (id) => document.getElementById(id);
  const NARRATION = new Set(["Detective", "Case note"]);
  const MAX_SECONDS = 30;       // one question, not a speech
  const QUIET_STOP_MS = 1600;   // stop by itself after this much silence following speech
  const LOUD = 0.015;           // RMS of a chunk that counts as someone talking

  let status = { hear: "missing", speak: false };
  let voicesOn = localStorage.getItem("spade-voices") !== "off";
  let game = null;
  let firstSight = true;
  const spoken = new Set();
  let player = null;
  let speakTurn = 0;
  let listening = null;
  let statusTimer;

  // ---------- controls ----------

  const voiceToggle = document.createElement("button");
  voiceToggle.type = "button";
  voiceToggle.id = "voice-toggle";
  voiceToggle.className = "quiet nav-button";
  voiceToggle.hidden = true;

  const mic = document.createElement("button");
  mic.type = "button";
  mic.id = "mic-button";
  mic.className = "mic";
  mic.hidden = true;
  mic.innerHTML = '<svg viewBox="0 0 16 16" aria-hidden="true"><rect x="5.5" y="1" width="5" height="9" rx="2.5"/>'
    + '<path d="M3 7.5v1a5 5 0 0 0 10 0v-1h-1.3v1a3.7 3.7 0 0 1-7.4 0v-1zM7.3 13h1.4v2H7.3z"/></svg><span id="mic-label">Speak</span>';

  const hint = document.createElement("p");
  hint.id = "voice-hint";
  hint.className = "status";
  hint.setAttribute("role", "status");

  function mount() {
    const sound = $("sound-toggle");
    const ask = $("ask-button");
    if (sound) sound.before(voiceToggle);
    if (ask) {
      ask.before(mic);
      ask.closest(".input-row").after(hint);
    }
  }

  function paintControls() {
    voiceToggle.hidden = !status.speak;
    voiceToggle.textContent = voicesOn ? "Voices on" : "Voices off";
    voiceToggle.setAttribute("aria-pressed", String(voicesOn));
    voiceToggle.title = voicesOn ? "Turn the suspects' voices off" : "Hear the suspects speak";

    const field = $("question");
    const loading = status.hear === "loading" || status.hear === "idle";
    mic.hidden = status.hear === "missing";
    mic.disabled = !listening && (status.hear !== "ready" || !field || field.disabled);
    mic.setAttribute("aria-pressed", String(!!listening));
    mic.querySelector("#mic-label").textContent = listening ? "Listening" : "Speak";
    mic.title = listening ? "Stop listening"
      : loading ? "The microphone is getting ready"
      : status.hear === "failed" ? "Speech recognition did not start. You can still type."
      : "Say your question instead of typing it";
  }

  async function refreshStatus() {
    clearTimeout(statusTimer);
    try {
      const response = await fetch("/api/voice", { cache: "no-store" });
      if (response.ok) status = await response.json();
    } catch {
      /* the game page shows its own lost-contact message */
    }
    paintControls();
    if (status.hear === "loading" || status.hear === "idle") statusTimer = setTimeout(refreshStatus, 1500);
  }

  // ---------- speaking ----------

  function hush() {
    speakTurn += 1;
    if (player) {
      player.pause();
      player = null;
    }
  }

  async function say(suspect, text) {
    hush();
    if (!status.speak || !voicesOn || listening) return;
    const turn = speakTurn;
    try {
      const response = await fetch("/api/voice/speak", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ suspect, text: text.slice(0, 400) })
      });
      if (!response.ok || turn !== speakTurn) return;
      const url = URL.createObjectURL(await response.blob());
      if (turn !== speakTurn) {
        URL.revokeObjectURL(url);
        return;
      }
      player = new Audio(url);
      player.onended = player.onerror = () => URL.revokeObjectURL(url);
      await player.play();
    } catch {
      /* no audio device, or the browser wants a click first: the line is still on screen */
    }
  }

  function watch(state) {
    game = state;
    paintControls();
    if (state.phase !== "interview") {
      if (state.phase === "briefing") spoken.clear(); // a reopened case greets you again
      hush();
      firstSight = false;
      return;
    }
    const suspect = (state.suspects || []).find((s) => s.id === state.selectedId);
    const lines = state.transcript || [];
    let index = lines.length - 1;
    while (index >= 0 && (NARRATION.has(lines[index].speaker) || (suspect && lines[index].speaker !== suspect.name))) index -= 1;
    if (!suspect || index < 0) return;
    const key = suspect.id + ":" + index;
    if (!spoken.has(key)) {
      spoken.add(key);
      // A page opened in the middle of an interview does not start by reading the last line aloud.
      if (!firstSight) say(suspect.id, lines[index].text);
    }
    firstSight = false;
  }

  // ---------- hearing ----------

  function tell(message) {
    hint.textContent = message;
  }

  function wavFrom(session) {
    const ratio = session.rate / 16000;
    const total = Math.floor(session.length / ratio);
    const merged = new Float32Array(session.length);
    let offset = 0;
    for (const chunk of session.chunks) {
      merged.set(chunk, offset);
      offset += chunk.length;
    }
    const view = new DataView(new ArrayBuffer(44 + total * 2));
    const text = (at, value) => [...value].forEach((ch, i) => view.setUint8(at + i, ch.charCodeAt(0)));
    text(0, "RIFF"); view.setUint32(4, 36 + total * 2, true); text(8, "WAVE"); text(12, "fmt ");
    view.setUint32(16, 16, true); view.setUint16(20, 1, true); view.setUint16(22, 1, true);
    view.setUint32(24, 16000, true); view.setUint32(28, 32000, true); view.setUint16(32, 2, true); view.setUint16(34, 16, true);
    text(36, "data"); view.setUint32(40, total * 2, true);
    for (let i = 0; i < total; i += 1) {
      // Average each block of input samples: a simple low-pass before dropping to 16 kHz.
      const from = Math.floor(i * ratio);
      const to = Math.max(from + 1, Math.floor((i + 1) * ratio));
      let sum = 0;
      for (let j = from; j < to; j += 1) sum += merged[j];
      const sample = Math.max(-1, Math.min(1, sum / (to - from)));
      view.setInt16(44 + i * 2, sample * 32767, true);
    }
    return new Blob([view], { type: "audio/wav" });
  }

  function write(session, heard) {
    const field = $("question");
    if (!field) return;
    const line = (session.typed ? session.typed + " " : "") + heard;
    field.value = line.slice(0, field.maxLength > 0 ? field.maxLength : 300);
    field.dispatchEvent(new Event("input", { bubbles: true })); // the page keeps a draft per suspect
  }

  async function hear(session, final) {
    if (!final && session.asking) return;
    session.asking = true;
    try {
      const response = await fetch("/api/voice/hear" + (final ? "?final=1" : ""), {
        method: "POST", headers: { "Content-Type": "audio/wav" }, body: wavFrom(session)
      });
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || "Speech recognition is not working.");
      // A late partial must not overwrite the final text.
      if (typeof result.text === "string" && (final || listening === session)) {
        if (result.text) write(session, result.text);
        return result.text;
      }
    } catch (error) {
      if (final) tell(error.message + " You can still type.");
    } finally {
      session.asking = false;
    }
    return undefined;
  }

  async function stopListening() {
    const session = listening;
    if (!session) return;
    listening = null;
    clearInterval(session.timer);
    session.node.disconnect();
    session.source.disconnect();
    session.stream.getTracks().forEach((track) => track.stop());
    session.context.close();
    paintControls();
    if (!session.spoke) {
      tell("I didn't hear anything. Press Speak and try again, or type.");
      return;
    }
    tell("Writing down what you said…");
    const heard = await hear(session, true);
    if (heard === undefined) return;
    tell(heard ? "Press Enter to ask it, or change it first." : "I couldn't make that out. Try again, or type.");
    const field = $("question");
    if (field && !field.disabled) field.focus({ preventScroll: true });
  }

  async function startListening() {
    const field = $("question");
    if (listening || !field || field.disabled || status.hear !== "ready") return;
    hush(); // don't record the suspect's voice coming out of the speakers
    let stream;
    try {
      stream = await navigator.mediaDevices.getUserMedia({
        audio: { channelCount: 1, echoCancellation: true, noiseSuppression: true, autoGainControl: true }
      });
    } catch {
      tell("No microphone, or it is blocked for this window. You can still type.");
      return;
    }
    const context = new AudioContext();
    const source = context.createMediaStreamSource(stream);
    const node = context.createScriptProcessor(4096, 1, 1);
    const silent = context.createGain();
    silent.gain.value = 0; // the processor only runs when connected to an output
    const session = {
      stream, context, source, node, chunks: [], length: 0, rate: context.sampleRate,
      typed: field.value.trim(), spoke: false, loudChunks: 0, lastLoud: 0, lastSent: 0, asking: false, timer: 0
    };
    node.onaudioprocess = (event) => {
      const data = event.inputBuffer.getChannelData(0);
      session.chunks.push(new Float32Array(data));
      session.length += data.length;
      let energy = 0;
      for (let i = 0; i < data.length; i += 1) energy += data[i] * data[i];
      if (Math.sqrt(energy / data.length) > LOUD) {
        session.lastLoud = performance.now();
        session.loudChunks += 1;
        if (session.loudChunks >= 2) session.spoke = true;
      }
    };
    source.connect(node);
    node.connect(silent);
    silent.connect(context.destination);
    session.timer = setInterval(() => {
      if (listening !== session) return;
      const now = performance.now();
      if (session.length / session.rate >= MAX_SECONDS || (session.spoke && now - session.lastLoud > QUIET_STOP_MS)) {
        stopListening();
      } else if (session.spoke && now - session.lastSent > 1100) {
        session.lastSent = now;
        hear(session, false);
      }
    }, 250);
    listening = session;
    tell("Listening… ask your question. I stop when you do.");
    paintControls();
  }

  // ---------- wiring ----------

  mic.addEventListener("click", () => (listening ? stopListening() : startListening()));
  voiceToggle.addEventListener("click", () => {
    voicesOn = !voicesOn;
    localStorage.setItem("spade-voices", voicesOn ? "on" : "off");
    if (!voicesOn) hush();
    paintControls();
  });
  // Asking, switching suspect or accusing ends whatever is being heard or said.
  document.addEventListener("noir-action", () => {
    if (listening) stopListening();
    hush();
    tell("");
  });
  window.addEventListener("pagehide", () => {
    if (listening) stopListening();
    hush();
  });

  mount();
  if (window.NoirUI && typeof window.NoirUI.render === "function") {
    const render = window.NoirUI.render;
    window.NoirUI.render = (state) => {
      render(state);
      watch(state);
    };
  }
  refreshStatus();
})();

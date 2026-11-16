/* An original sixteen-bar club band heard through the floor of the interview room. */
(() => {
  "use strict";
  const beat = 60 / 84;
  const bars = [
    [45, 55, 60, 64], [50, 57, 60, 65], [43, 53, 59, 62], [48, 55, 59, 64],
    [41, 52, 57, 60], [47, 53, 57, 62], [40, 50, 56, 62], [45, 55, 60, 64],
    [50, 57, 60, 65], [43, 53, 59, 62], [48, 55, 59, 64], [41, 52, 57, 60],
    [47, 53, 57, 62], [40, 50, 56, 62], [45, 55, 60, 64], [40, 50, 56, 62]
  ];
  // The rests leave the voices at the interrogation table in the foreground.
  const horn = [
    [0, 0, 64, 67], [65, 0, 0, 62], [59, 62, 0, 0], [64, 0, 0, 0],
    [0, 64, 65, 69], [68, 0, 62, 0], [63, 64, 0, 0], [60, 0, 0, 0],
    [0, 65, 69, 72], [71, 0, 67, 65], [64, 0, 62, 0], [60, 0, 0, 0],
    [0, 62, 65, 0], [64, 62, 0, 59], [60, 0, 0, 0], [0, 0, 0, 0]
  ];
  let context, band, dry, noise, timer, cursor, when, active = false;
  const pitch = (key) => 440 * 2 ** ((key - 69) / 12);

  function setup() {
    context = new AudioContext();
    dry = context.createGain();
    dry.gain.value = 0.28;
    dry.connect(context.destination);
    const wall = context.createBiquadFilter();
    wall.type = "lowpass";
    wall.frequency.value = 860;
    wall.Q.value = 0.5;
    wall.connect(dry);
    band = context.createGain();
    band.gain.value = 0.7;
    band.connect(wall);
    const echo = context.createDelay(0.4);
    echo.delayTime.value = 0.14;
    const distant = context.createGain();
    distant.gain.value = 0.12;
    band.connect(echo).connect(distant).connect(wall);
    noise = context.createBuffer(1, context.sampleRate, context.sampleRate);
    const values = noise.getChannelData(0);
    for (let i = 0; i < values.length; i++) values[i] = Math.random() * 2 - 1;
  }

  function tone(type, midi, at, length, volume, rise, detune = 0) {
    const oscillator = context.createOscillator();
    const gain = context.createGain();
    oscillator.type = type;
    oscillator.frequency.value = pitch(midi);
    oscillator.detune.value = detune;
    gain.gain.setValueAtTime(0.0001, at);
    gain.gain.exponentialRampToValueAtTime(volume, at + rise);
    gain.gain.exponentialRampToValueAtTime(0.0001, at + length);
    oscillator.connect(gain).connect(band);
    oscillator.start(at);
    oscillator.stop(at + length + 0.01);
    oscillator.onended = () => { oscillator.disconnect(); gain.disconnect(); };
  }

  function brush(at, length, volume, frequency, output) {
    const source = context.createBufferSource();
    const filter = context.createBiquadFilter();
    const gain = context.createGain();
    source.buffer = noise;
    filter.type = "bandpass";
    filter.frequency.value = frequency;
    filter.Q.value = 0.65;
    gain.gain.setValueAtTime(volume, at);
    gain.gain.exponentialRampToValueAtTime(0.0001, at + length);
    source.connect(filter).connect(gain).connect(output);
    source.start(at, Math.random() * 0.45, length);
    source.onended = () => { source.disconnect(); filter.disconnect(); gain.disconnect(); };
  }

  function score() {
    if (!active || context.state !== "running") return;
    if (when < context.currentTime) when = context.currentTime + 0.025;
    while (when < context.currentTime + 0.32) {
      const bar = Math.floor(cursor / 4) % bars.length;
      const count = cursor % 4;
      const [root, ...chord] = bars[bar];
      const bass = [root, root + 7, root + 12, bars[(bar + 1) % bars.length][0] + 1][count];
      tone("triangle", bass, when, beat * 0.88, 0.21, 0.012);
      if (count === 1 || count === 3) {
        chord.forEach((note, index) => tone("triangle", note, when + index * 0.014, 0.45, 0.038, 0.01));
        brush(when, 0.17, 0.10, 1850, band);
      }
      brush(when + beat * 0.67, 0.08, 0.045, 2800, band);
      const note = horn[bar][count];
      if (note) {
        const hold = horn[bar][count + 1] === 0 ? 1.43 : 0.83;
        tone("sawtooth", note, when + 0.035, beat * hold, 0.042, 0.079);
        tone("triangle", note, when + 0.045, beat * hold, 0.048, 0.087, -3);
      }
      cursor++;
      when += beat;
    }
  }

  function indicate() {
    const button = document.getElementById("sound-toggle");
    if (button) {
      button.setAttribute("aria-pressed", String(active));
      button.setAttribute("aria-label", active ? "Turn room sound off" : "Turn room sound on");
      button.title = button.getAttribute("aria-label");
      (button.querySelector("#sound-label") || button).textContent = active ? "Sound on" : "Sound off";
    }
  }

  document.addEventListener("click", async (event) => {
    if (!event.target.closest("#sound-toggle")) return;
    try {
      if (active) {
        active = false;
        clearInterval(timer);
        await context.suspend();
      } else {
        if (!context) setup();
        await context.resume();
        active = true;
        cursor ??= 0;
        when = context.currentTime + 0.07;
        score();
        timer = setInterval(score, 180);
      }
    } catch { active = false; }
    indicate();
  });

  document.addEventListener("noir-action", (event) => {
    if (!active) return;
    const at = context.currentTime;
    if (event.detail.action === "ask") {
      brush(at, 0.035, 0.12, 2300, dry);
      brush(at + 0.04, 0.025, 0.07, 850, dry);
    } else if (["select", "present", "begin", "accuse"].includes(event.detail.action)) {
      brush(at, 0.15, 0.09, 1500, dry);
    }
  });

  document.addEventListener("input", (event) => {
    if (active && event.inputType === "insertText" && event.target.matches("input,textarea")) {
      brush(context.currentTime, 0.018, 0.065, 1300 + Math.random() * 800, dry);
    }
  });
  window.addEventListener("pagehide", () => { if (context) context.suspend(); });
})();

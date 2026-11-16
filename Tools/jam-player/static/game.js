(() => {
  "use strict";

  let state;
  let sending = false;
  let refreshTimer;
  let loaded = false;
  const client = crypto.randomUUID();

  async function request(path, body) {
    const options = body ? {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    } : {};
    const response = await fetch(path, { ...options, cache: "no-store" });
    const value = await response.json();
    if (value.state) state = value.state;
    if (!response.ok) throw new Error(value.error || "Could not reach the interview room.");
    return value;
  }

  function paint(override = {}) {
    window.NoirUI.render({ ...state, ...override });
  }

  function schedule() {
    clearTimeout(refreshTimer);
    if (state?.modelState === "loading" || state?.busy) {
      refreshTimer = setTimeout(refresh, 700);
    }
  }

  async function refresh() {
    try {
      state = await request("/api/state");
      paint();
      schedule();
    } catch (error) {
      paint({ status: "Lost contact with the game. Reconnecting…", busy: true });
      refreshTimer = setTimeout(refresh, 2000);
    }
  }

  async function activate() {
    if (loaded) return;
    loaded = true;
    try {
      await request("/api/hello", { client });
    } finally {
      await refresh();
    }
  }

  window.addEventListener("pagehide", () => {
    const payload = new Blob([JSON.stringify({ client })], { type: "application/json" });
    navigator.sendBeacon("/api/leave", payload);
    loaded = false;
    clearTimeout(refreshTimer);
  });
  window.addEventListener("pageshow", activate);

  document.addEventListener("noir-action", async (event) => {
    if (sending || !state) return;
    sending = true;
    clearTimeout(refreshTimer);
    const body = { ...event.detail, revision: state.revision, suspect: state.selectedId };
    const exchange = ["ask", "present"].includes(body.action);
    const suspect = state.suspects.find((s) => s.id === state.selectedId);
    const item = state.notebook.find((e) => e.id === body.id);
    if (exchange) {
      const line = body.action === "ask" ? body.text : "Presented: " + (item?.text || "statement");
      paint({ busy: true, status: (suspect?.name.split(" ")[0] || "The witness") + " is thinking…",
              clue: "", transcript: [...state.transcript, {speaker: "Detective", text: line}] });
    }
    try {
      state = await request("/api/action", body);
      paint();
    } catch (error) {
      paint({ status: error.message });
    } finally {
      sending = false;
      schedule();
    }
  });

  activate();
})();

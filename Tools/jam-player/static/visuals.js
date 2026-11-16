(() => {
  "use strict";
  const $ = id => document.getElementById(id);
  const motion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const ids = new Set(["vera_lane", "mickey_doyle", "harlan_graves", "tommy_mullen", "allison_wu", "logan_wu"]);
  const moods = {
    angry: "vera_angry", annoyed: "vera_angry", hostile: "vera_angry",
    sad: "vera_sad", afraid: "vera_sad", scared: "vera_sad", nervous: "vera_sad",
    calm: "vera_relaxed", amused: "vera_relaxed", pleased: "vera_relaxed", relaxed: "vera_relaxed"
  };
  let state = { phase: "briefing", busy: true, suspects: [], notebook: [], transcript: [] };
  let lastSeat = "";
  let lastPhase = "";
  let lastRoster = "";
  let lastLines = "";
  let lastNotebook = "";
  const drafts = new Map();

  function element(tag, cls, content) {
    const node = document.createElement(tag);
    if (cls) node.className = cls;
    if (content !== undefined) node.textContent = content;
    return node;
  }

  function act(action, fields = {}) {
    document.dispatchEvent(new CustomEvent("noir-action", { detail: { action, ...fields } }));
  }

  function dialog(id) {
    const node = $(id);
    if (!node.open) node.showModal();
  }

  function scene(node, id, emotion, closed) {
    const clip = closed ? "vacant" : id === "vera_lane" && moods[emotion] ? moods[emotion] : id;
    const url = "art/" + clip + (closed || motion.matches ? ".png" : ".webp");
    if (node.getAttribute("src") !== url) node.setAttribute("src", url);
  }

  function castCard(suspect, interactive) {
    const selected = suspect.id === state.selectedId;
    const card = element(interactive ? "button" : "article", "cast-card");
    if (interactive) {
      card.type = "button";
      card.dataset.id = suspect.id;
      card.setAttribute("aria-pressed", String(selected));
      card.disabled = !!state.busy;
      card.setAttribute("aria-label", "Interview " + suspect.name + (suspect.closed ? " (interview closed)" : ""));
    }
    const holder = element("div", "cast-image");
    const photo = element("img");
    if (ids.has(suspect.id)) photo.src = "art/" + suspect.id + "-portrait.png";
    photo.alt = "";
    holder.append(photo, element("span", "slot-tag", String((suspect.slot ?? state.suspects.indexOf(suspect)) + 1).padStart(2, "0")));
    card.append(holder, element("span", "cast-name", suspect.name), element("span", "cast-role", suspect.closed ? "Interview closed" : suspect.role));
    return card;
  }

  function roster() {
    const key = JSON.stringify([state.phase, state.selectedId, !!state.busy, !!state.failed, state.suspects.map(s => [s.id,s.name,s.role,s.closed])]);
    if (lastRoster === key) return;
    lastRoster = key;
    const interviewing = state.phase === "interview";
    const root = $(interviewing ? "seat-roster" : "brief-roster");
    const previousFocus = root.contains(document.activeElement) ? document.activeElement.dataset.id : null;
    root.replaceChildren(...state.suspects.map(s => castCard(s, interviewing)));
    if (previousFocus) [...root.children].find(c => c.dataset.id === previousFocus)?.focus({preventScroll: true});
  }

  function drawMeters(suspect) {
    const items = (suspect.meters || []).map(meter => {
      const val = Math.max(0, Math.min(100, Number(meter.value) || 0));
      const row = element("div", "meter");
      const label = element("div", "meter-label");
      label.append(element("span", "", meter.label), element("span", "meter-value", String(val)));
      const track = element("div", "track");
      track.setAttribute("role", "meter");
      track.setAttribute("aria-label", meter.label);
      track.setAttribute("aria-valuemin", "0");
      track.setAttribute("aria-valuemax", "100");
      track.setAttribute("aria-valuenow", String(val));
      const bar = element("span", "fill");
      bar.style.width = val + "%";
      track.append(bar);
      row.append(label, track);
      return row;
    });
    $("meters").replaceChildren(...items);
  }

  function renderLines(suspect) {
    const area = $("transcript");
    const key = JSON.stringify([suspect.id, suspect.closed, state.transcript]);
    if (lastLines === key) return;
    const changedSeat = lastSeat !== suspect.id;
    const end = changedSeat || area.scrollHeight - area.clientHeight - area.scrollTop < 50;
    lastLines = key;
    const entries = state.transcript.map(line => {
      const row = element("div", "exchange" + (line.speaker === "Detective" ? " detective" : ""));
      row.append(element("div", "eyebrow", line.speaker), element("p", "", line.text));
      return row;
    });
    if (!entries.length) {
      const invitation = suspect.closed ? "This witness has left the club." : "The recorder is ready. Ask " + suspect.name.split(" ")[0] + " where they were when the lights went out.";
      entries.push(element("p", "transcript-empty", invitation));
    }
    area.replaceChildren(...entries);
    if (end) area.scrollTop = area.scrollHeight;
    const turns = state.transcript.filter(line => line.speaker === "Detective").length;
    $("turn-count").textContent = turns + (turns === 1 ? " question" : " questions");
  }

  function drawNotebook(suspect) {
    const blocked = !suspect || suspect.closed || state.busy || state.failed;
    const key = JSON.stringify([suspect?.id, !!blocked, state.notebook]);
    if (lastNotebook === key) return;
    lastNotebook = key;
    $("notebook-hint").textContent = !state.notebook.length ? "No statements recorded. Find out what the witnesses are holding back." : "Statements are recorded in the witness’s own case file. Choose one to quote at the table.";
    const items = state.notebook.map(item => {
      const row = element("article", "note-entry");
      row.append(element("span", "eyebrow", item.source), element("p", "", item.text));
      const button = element("button", "subtle-button", "Quote to " + (suspect?.name.split(" ")[0] || "witness") + " →");
      button.type = "button";
      button.dataset.id = item.id;
      button.disabled = blocked;
      row.append(button);
      return row;
    });
    $("notebook-list").replaceChildren(...items);
  }

  function interview() {
    const suspect = state.suspects.find(s => s.id === state.selectedId);
    if (!suspect) return;
    const disabled = suspect.closed || state.busy || state.failed;
    const field = $("question");
    if (lastSeat !== suspect.id) field.value = drafts.get(suspect.id) || "";
    field.disabled = !!(suspect.closed || state.failed);
    field.readOnly = !!state.busy;
    field.placeholder = suspect.closed ? "Interview closed" : "Ask about the blackout…";
    $("ask-button").disabled = !!disabled;
    $("accuse-button").disabled = !!state.busy;
    $("accused").disabled = $("file-charge").disabled = !!state.busy;
    $("suspect-name").textContent = suspect.name;
    $("suspect-role").textContent = suspect.closed ? suspect.role + " · Interview closed" : suspect.role;
    $("seat-number").textContent = "Seat " + String((suspect.slot ?? state.suspects.indexOf(suspect)) + 1).padStart(2, "0");
    $("seat-photo").alt = suspect.closed ? "An empty chair at the interview table." : suspect.name + " at the interview table.";
    if (ids.has(suspect.id)) scene($("seat-photo"), suspect.id, state.emotion, suspect.closed);
    drawMeters(suspect);
    renderLines(suspect);
    $("tactic").hidden = !state.tactic;
    $("tactic").textContent = state.tactic || "";
    $("clue").hidden = !state.clue;
    if (state.clue) {
      $("clue").replaceChildren(element("b", "", "Statement on record ↗"), document.createTextNode(state.clue));
    }
    $("interview-status").textContent = state.status || (suspect.closed ? "This witness has left. Select another suspect to continue." : "");
    $("interview-retry").hidden = !state.failed;
    drawNotebook(suspect);
    lastSeat = suspect.id;
  }

  function render(view = {}) {
    state = { ...state, ...view };
    const changed = lastPhase !== state.phase;
    if (changed) {
      document.querySelectorAll("dialog[open]").forEach(node => node.close());
      const question = $("question");
      if (state.phase === "briefing") {
        drafts.clear();
        question.value = "";
        lastSeat = "";
      }
      if (lastPhase) window.scrollTo(0, 0);
    }
    for (const id of ["briefing","interview","ending"]) $(id).hidden = state.phase !== id;
    $("notes-button").hidden = state.phase !== "interview";
    $("notes-count").textContent = state.notebook.length;
    $("case-setting").textContent = state.setting || "At 11:10 the bartender found Jimmy Calloway shot dead at his desk. Six people from the Black Spade Club have been held for questioning.";
    if (state.phase === "briefing") {
      scene($("cover-photo"), "vera_lane");
      $("begin-button").disabled = !!(state.busy || state.failed || !state.suspects.length);
      $("brief-retry").hidden = !state.failed;
      $("brief-status").textContent = state.status || "";
      roster();
    } else if (state.phase === "interview") {
      interview();
      roster();
      if (changed) $("question").focus({preventScroll: true});
    } else if (state.phase === "ending") {
      const end = state.ending || {};
      $("ending-title").textContent = end.title || "Final report";
      $("ending-text").textContent = end.text || "";
      $("epilogues").replaceChildren(...(Array.isArray(end.epilogues) ? end.epilogues : []).map(line => element("p", "", line)));
      if (changed) $("restart").focus({preventScroll: true});
    }
    lastPhase = state.phase;
  }

  $("file-button").addEventListener("click", () => dialog("case-dialog"));
  $("notes-button").addEventListener("click", () => dialog("notebook-dialog"));
  $("clue").addEventListener("click", () => dialog("notebook-dialog"));
  $("accuse-button").addEventListener("click", () => {
    const choices = state.suspects.map(s => {
      const opt = element("option", "", s.name);
      opt.value = s.id;
      return opt;
    });
    $("accused").replaceChildren(...choices);
    $("accused").value = state.selectedId;
    dialog("accuse-dialog");
  });
  document.querySelectorAll(".close-dialog,.close-action").forEach(btn => btn.addEventListener("click", () => btn.closest("dialog").close()));
  $("seat-roster").addEventListener("click", event => {
    const card = event.target.closest("button[data-id]");
    if (card && !card.disabled && card.dataset.id !== state.selectedId) act("select", {id:card.dataset.id});
  });
  $("question").addEventListener("input", event => drafts.set(state.selectedId, event.target.value));
  $("question-form").addEventListener("submit", event => {
    event.preventDefault();
    const input = $("question");
    const line = input.value.trim();
    if (!line || input.disabled || input.readOnly) return;
    input.value = "";
    drafts.delete(state.selectedId);
    input.focus({preventScroll: true});
    act("ask", {text:line});
  });
  $("notebook-list").addEventListener("click", event => {
    const button = event.target.closest("button[data-id]");
    if (!button || button.disabled) return;
    $("notebook-dialog").close();
    act("present", {id:button.dataset.id});
  });
  $("file-charge").addEventListener("click", () => {
    const id = $("accused").value;
    if (!ids.has(id) || state.busy) return;
    $("accuse-dialog").close();
    act("accuse", {id});
  });
  $("begin-button").addEventListener("click", () => act("begin"));
  $("restart").addEventListener("click", () => act("restart"));
  for (const id of ["brief-retry","interview-retry"]) $(id).addEventListener("click", () => act("retry"));
  motion.addEventListener("change", () => render());
  window.NoirUI = { render };
})();



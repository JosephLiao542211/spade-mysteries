"""Windows launcher and local session for Lights Out at the Black Spade."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes
import json
import logging
import mimetypes
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import threading
import time
from urllib.parse import unquote, urlsplit
import webbrowser

HERE = Path(__file__).resolve().parent
FROZEN = getattr(sys, "frozen", False)
ROOT = Path(sys.executable).resolve().parent if FROZEN else HERE.parents[1]
STATIC = ROOT / "static" if FROZEN else HERE / "static"
if not FROZEN:
    sys.path.insert(0, str(ROOT / "Tools" / "llm-harness"))
import engine
from local_model import LocalModel

ROLE = {
    "vera_lane": "Torch singer", "mickey_doyle": "Bartender",
    "harlan_graves": "Club partner", "tommy_mullen": "Driver & mechanic",
    "allison_wu": "Singer", "logan_wu": "Importer",
}
# The opening lines match the game's authored cast sheet in NoirPlayerController.
GREETING = {
    "vera_lane": "Make it quick, detective. I've got a second set at one, and I'd rather sing to an empty room than to you.",
    "mickey_doyle": "Get you something, detective? On the house. Tonight everything's on the house. Twelve years behind that bar and I never seen a night like it.",
    "harlan_graves": "Sit down if you must. My lawyer will hear every word of this.",
    "tommy_mullen": "Detective! Pull up a chair. Rotten night, sure, but you look like a fella who could use a smile.",
    "allison_wu": "Detective. Sit, if you like. I don't bite, whatever they told you downstairs.",
    "logan_wu": "Detective. Forgive the handkerchief. This room is warm, and the evening has been... long.",
}
TACTIC = {
    "small_talk": "small talk", "question": "a question", "empathy": "sympathy",
    "flattery": "flattery", "reassure": "reassurance", "threaten": "a threat",
    "deal": "an offer", "logic": "a challenge to the story",
    "challenge_pride": "a dare", "accuse": "an accusation", "appeal": "an appeal to conscience",
    "off_topic_or_meta": "a change of subject",
}


class Session:
    def __init__(self, root, log_dir):
        self.case = engine.load_case(root / "Content" / "Llm" / "Cases" / "SpadeClub")
        self.model = LocalModel(root, log_dir)
        self.lock = threading.RLock()
        self.model_state = "idle"
        self.error = ""
        self.revision = 0
        self.reset()

    def reset(self):
        self.state = engine.CaseState(self.case)
        self.phase = "briefing"
        self.ending = None
        ordered = sorted(self.case["suspect_defs"].values(), key=lambda s: s["slot"])
        self.selected = next((s["id"] for s in ordered if s["id"] == "mickey_doyle"), ordered[0]["id"])
        self.conversations = {s["id"]: [{"speaker": s["name"], "text": GREETING.get(s["id"], "Detective.")}]
                              for s in ordered}
        self.last = {s["id"]: {} for s in ordered}
        self.turn_busy = False
        self.revision += 1

    def start_model(self):
        with self.lock:
            if self.model_state in ("loading", "ready"):
                return
            self.model_state = "loading"
            self.error = ""

        def prepare():
            try:
                self.model.start()
                def warm_seat(suspect):
                    sid = suspect["id"]
                    messages = [{"role": "system", "content": engine.static_system(self.case, sid)},
                                {"role": "user", "content": engine.detective_message("...")}]
                    self.model.warm(messages, engine.schema(self.state, sid), suspect["slot"])

                with ThreadPoolExecutor(max_workers=6, thread_name_prefix="prepare-seat") as pool:
                    list(pool.map(warm_seat, self.case["suspect_defs"].values()))
                ready, error = "ready", ""
            except Exception as exc:
                logging.exception("Interview startup failed")
                ready, error = "failed", str(exc)
            with self.lock:
                self.model_state, self.error = ready, error
                self.revision += 1

        threading.Thread(target=prepare, daemon=True, name="prepare-interviews").start()

    def view(self):
        with self.lock:
            sid = self.selected
            defs = self.case["suspect_defs"]
            cast = []
            for d in sorted(defs.values(), key=lambda item: item["slot"]):
                st = self.state.suspects[d["id"]]
                cast.append({
                    "id": d["id"], "slot": d["slot"], "name": d["name"], "role": ROLE.get(d["id"], "Witness"),
                    "closed": st.closed, "meters": [
                        {"label": val.get("label", key.title()), "value": round(st.meters[key])}
                        for key, val in d["meters"].items()
                    ],
                })
            status = {"loading": "Preparing the interview room…", "ready": "",
                      "failed": self.error, "idle": ""}[self.model_state]
            if self.turn_busy:
                status = f"{defs[sid]['name'].split()[0]} is thinking…"
            return {
                "phase": self.phase, "setting": self.case["setting"], "title": self.case["title"],
                "suspects": cast, "selectedId": sid, "transcript": list(self.conversations[sid]),
                "tactic": self.last[sid].get("tactic", ""), "clue": self.last[sid].get("clue", ""),
                "emotion": self.last[sid].get("emotion", ""), "busy": self.turn_busy or self.model_state == "loading",
                "failed": self.model_state == "failed", "modelState": self.model_state,
                "modelLabel": getattr(self.model, "label", ""), "status": status, "revision": self.revision,
                "notebook": [{"id": e["id"], "source": defs[e["source"]]["name"], "name": e["name"],
                              "text": e["description"]} for e in self.case["evidence"] if e["id"] in self.state.inventory],
                "ending": self.ending,
            }

    def action(self, body):
        action = body.get("action")
        if action == "retry":
            self.start_model()
            return self.view()
        with self.lock:
            if body.get("revision") != self.revision or self.turn_busy:
                raise ValueError("The case has changed. Check the latest interview and try again.")
            if self.model_state != "ready":
                if self.model_state != "failed":
                    raise ValueError("The interviews are still preparing.")
                if action not in ("select", "accuse", "restart"):
                    raise ValueError("Reconnect the interview engine before questioning a witness.")
            if action == "begin" and self.phase == "briefing":
                self.phase = "interview"
            elif action == "restart" and self.phase == "ending":
                self.reset()
            elif self.phase != "interview":
                raise ValueError("That action is unavailable on this page.")
            elif action in ("select", "accuse"):
                sid = body.get("id")
                if sid not in self.state.suspects:
                    raise ValueError("Choose a suspect from the roster.")
                if action == "select":
                    self.selected = sid
                else:
                    self.ending = engine.accuse(self.state, sid)
                    self.phase = "ending"
            elif action in ("ask", "present"):
                if body.get("suspect") != self.selected:
                    raise ValueError("Choose the witness again before speaking.")
                sid = self.selected
                st = self.state.suspects[sid]
                if st.closed:
                    raise ValueError("This witness has left the club.")
                eid = body.get("id") if action == "present" else None
                if action == "present":
                    if not isinstance(eid, str) or eid not in self.state.inventory:
                        raise ValueError("Choose a statement from your notebook.")
                    msg = engine.evidence_message(self.case, eid)
                    display = "Presented: " + self.case["evidence_by_id"][eid]["description"]
                else:
                    value = body.get("text")
                    if not isinstance(value, str):
                        raise ValueError("Type a question for the witness.")
                    display = " ".join(value.split())
                    if not 1 <= len(display) <= 300:
                        raise ValueError("Use between 1 and 300 characters.")
                    msg = engine.detective_message(display)
                self.turn_busy = True
                self.conversations[sid].append({"speaker": "Detective", "text": display})
            else:
                raise ValueError("Unknown action.")
            self.revision += 1
        if action in ("ask", "present"):
            self.exchange(sid, msg, eid)
        return self.view()

    def exchange(self, sid, message, eid):
        try:
            with self.lock:
                messages = engine.build_messages(self.state, sid, message)
                # Keep the stable prefix and the latest four exchanges in a 4K slot.
                if len(messages) > 10:
                    messages = [messages[0], *messages[-9:]]
                schema = engine.schema(self.state, sid)
                allowed = dict(engine.available_secrets(self.state, sid))
                actor = self.case["suspect_defs"][sid]
            parsed, raw = self.model.reply(messages, schema, actor["slot"])
            with self.lock:
                event = engine.resolve_turn(self.state, sid, message, parsed, raw, allowed, presented=eid)
                self.conversations[sid].append({"speaker": actor["name"], "text": parsed["r"]})
                clue = ""
                if event["revealed"]:
                    clue = engine.clue_text(self.case, sid, event["revealed"], allowed[event["revealed"]])
                    self.conversations[sid].append({"speaker": "Case note", "text": clue})
                for note in event["triggers"]:
                    self.conversations[sid].append({"speaker": "Case note", "text": note})
                tactic = "Heard the statement" if eid else actor["name"].split()[0] + " heard " + TACTIC.get(parsed["t"], parsed["t"].replace("_", " "))
                if event["bluff"]:
                    tactic += " · Unsubstantiated claim"
                self.last[sid] = {"emotion": parsed["e"], "tactic": tactic, "clue": clue}
        except Exception as exc:
            logging.exception("Interview reply failed")
            with self.lock:
                self.conversations[sid].append({"speaker": "Case note", "text": "No answer. Try your line again."})
                # A dead process can be restarted while keeping the case and notebook.
                if not self.model.alive:
                    self.model_state, self.error = "failed", "The interview engine stopped. Reopen the interview engine to continue."
            raise ValueError(f"No answer. {exc}") from exc
        finally:
            with self.lock:
                self.turn_busy = False
                self.revision += 1


class GameServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, port, session):
        super().__init__(("127.0.0.1", port), RequestHandler)
        self.game = session
        self.address = f"127.0.0.1:{self.server_port}"
        self.opened = False
        self.leaves = {}
        self.clients = set()
        self.client_lock = threading.Lock()

    def window_event(self, client, leaving=False):
        if not isinstance(client, str) or not 1 <= len(client) <= 100:
            return
        with self.client_lock:
            self.opened = True
            if leaving:
                if client in self.clients:
                    self.leaves[client] = time.monotonic() + 5
            else:
                self.clients.add(client)
                self.leaves.pop(client, None)

    def watch_windows(self):
        started = time.monotonic()
        while True:
            time.sleep(2)
            with self.client_lock:
                now = time.monotonic()
                for client, deadline in list(self.leaves.items()):
                    if now > deadline:
                        self.clients.discard(client)
                        del self.leaves[client]
                if (self.opened and not self.clients) or (not self.opened and now - started > 180):
                    break
        self.shutdown()


class RequestHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def output(self, status, content, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store" if content_type.startswith(("application/json", "text/")) else "private, max-age=86400")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.end_headers()
        try:
            self.wfile.write(content)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def json_out(self, status, data):
        self.output(status, json.dumps(data, ensure_ascii=False).encode(), "application/json;charset=utf-8")

    def correct_host(self):
        if self.headers.get("Host") == self.server.address:
            return True
        self.send_error(403)
        return False

    def do_GET(self):
        if not self.correct_host():
            return
        path = unquote(urlsplit(self.path).path)
        if path == "/api/state":
            self.json_out(200, self.server.game.view())
            return
        target = (STATIC / (path.lstrip("/") or "index.html")).resolve()
        if not target.is_relative_to(STATIC.resolve()) or not target.is_file():
            self.send_error(404)
            return
        # Byte ranges support seeking and looping the short cast films in Edge.
        size = target.stat().st_size
        content_type = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
        span = self.headers.get("Range")
        if not span:
            self.output(200, target.read_bytes(), content_type)
            return
        try:
            unit, bounds = span.split("=", 1)
            left, right = bounds.split("-", 1)
            if unit != "bytes" or (not left and not right):
                raise ValueError
            start = int(left) if left else max(0, size - int(right))
            end = min(size - 1, int(right)) if left and right else size - 1
            if not 0 <= start <= end < size:
                raise ValueError
        except ValueError:
            self.send_response(416)
            self.send_header("Content-Range", f"bytes */{size}")
            self.end_headers()
            return
        self.send_response(206)
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(end - start + 1))
        self.send_header("Content-Type", content_type)
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()
        with target.open("rb") as f:
            f.seek(start)
            try:
                self.wfile.write(f.read(end - start + 1))
            except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                pass

    def do_POST(self):
        if not self.correct_host():
            return
        origin = self.headers.get("Origin")
        if origin and origin != f"http://{self.server.address}":
            self.send_error(403)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 1 <= length <= 4096 or self.headers.get_content_type() != "application/json":
                raise ValueError("Invalid game request.")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict):
                raise ValueError("Invalid game request.")
            if self.path in ("/api/hello", "/api/leave"):
                self.server.window_event(body.get("client"), self.path == "/api/leave")
                self.json_out(200, {})
            elif self.path == "/api/action":
                self.json_out(200, self.server.game.action(body))
            else:
                self.send_error(404)
        except (ValueError, TypeError) as exc:
            self.json_out(400, {"error": str(exc), "state": self.server.game.view()})
        except Exception:
            logging.exception("Unexpected action failure")
            self.json_out(500, {"error": "The interview could not be updated. Try again.", "state": self.server.game.view()})


def launch_window(url):
    for base, exe in [("ProgramFiles(x86)", "Microsoft/Edge/Application/msedge.exe"),
                      ("ProgramFiles", "Microsoft/Edge/Application/msedge.exe"),
                      ("ProgramFiles", "Google/Chrome/Application/chrome.exe")]:
        browser = Path(os.environ.get(base, "")) / exe
        if browser.is_file():
            try:
                subprocess.Popen([str(browser), "--app=" + url], close_fds=True)
                return
            except OSError:
                continue
    if not webbrowser.open(url):
        raise RuntimeError(f"Open {url} in a browser on this PC to play.")


def main():
    parser = argparse.ArgumentParser(description="Lights Out at the Black Spade")
    parser.add_argument("--no-browser", action="store_true")
    parser.add_argument("--port", type=int, default=0)
    args = parser.parse_args()
    logs = Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "Black Spade" / "logs"
    logs.mkdir(exist_ok=True, parents=True)
    logging.basicConfig(filename=logs / "player.log", level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s", encoding="utf-8")
    session = Session(ROOT, logs)
    server = GameServer(args.port, session)
    url = "http://" + server.address + "/"
    logging.info("Opened case at %s", url)
    try:
        session.start_model()
        if not args.no_browser:
            launch_window(url)
            threading.Thread(target=server.watch_windows, daemon=True).start()
        if sys.stdout:
            print(url, flush=True)
        server.serve_forever(poll_interval=0.3)
    finally:
        server.server_close()
        session.model.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except Exception as error:
        logging.exception("Launcher failed")
        if FROZEN:
            ctypes.windll.user32.MessageBoxW(None, str(error), "Lights Out at the Black Spade", 0x10)
        else:
            raise

"""Offline voice for the Black Spade: whisper.cpp hears the detective, Kokoro speaks for the suspects.

Both run on this PC, like the dialogue model. Nothing here is required to play: when the files are missing
(see Tools/fetch-voice.ps1) the game reports that voice is unavailable and stays text-only.

  hearing : ThirdParty/whisper/<build>/Release/whisper-server.exe + Models/voice/ggml-base.en.bin
            One long-lived local HTTP server. The page posts 16 kHz mono WAV while the player talks.
  speaking: ThirdParty/sherpa-onnx/<build>/bin/sherpa-onnx-offline-tts.exe + Models/voice/kokoro-en-v0_19
            One short process per spoken line (about 1.3 s to load the model, then 4x faster than real time).
            Lines are cached on disk, so greetings and repeats play at once.
"""

import array
import hashlib
import io
import json
import logging
import os
import re
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request
import uuid
import wave
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

# Kokoro v0.19 speaker ids: 0 af, 1 af_bella, 2 af_nicole, 3 af_sarah, 4 af_sky, 5 am_adam, 6 am_michael,
# 7 bf_emma, 8 bf_isabella, 9 bm_george, 10 bm_lewis. Second value = length scale (larger = slower).
VOICES = {
    "vera_lane": (2, 1.0),       # af_nicole: the lowest, slowest woman's voice
    "mickey_doyle": (6, 0.9),    # am_michael, hurried
    "harlan_graves": (10, 1.08), # bm_lewis: the deepest voice
    "tommy_mullen": (5, 0.95),   # am_adam
    "allison_wu": (1, 1.0),      # af_bella
    "logan_wu": (9, 1.05),       # bm_george, formal
}
DEFAULT_VOICE = (0, 1.0)

# Whisper spells names it has been shown. Without this, "Vera Lane" comes back as "Veraline".
HEARING_PROMPT = "Vera Lane, Mickey Doyle, Harlan Graves, Tommy Mullen, Allison Wu, Logan Wu, Ace Calloway, Sal Varga."
MAX_WAV_BYTES = 2_000_000   # about 60 s of 16 kHz 16-bit mono
MAX_SPOKEN_CHARS = 400
_NOISE = re.compile(r"\[[^\]]*\]|\([^)]*\)|\*[^*]*\*")  # [BLANK_AUDIO], (music), *sighs*
_NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)


def _has_speech(wav):
    """False for silence and room hum. Whisper invents a sentence for silence (it recited the prompt), so quiet
    clips never reach it: at least 0.12 s of 20 ms frames must be louder than about -40 dBFS."""
    try:
        with wave.open(io.BytesIO(wav)) as clip:
            if clip.getsampwidth() != 2 or clip.getnchannels() != 1:
                raise ValueError("Send 16-bit mono audio.")
            samples = array.array("h", clip.readframes(clip.getnframes()))
            frame = max(1, clip.getframerate() // 50)
    except (wave.Error, EOFError) as exc:
        raise ValueError("Send a WAV recording of up to one minute.") from exc
    loud = 0
    for start in range(0, len(samples) - frame + 1, frame):
        chunk = samples[start:start + frame]
        if sum(s * s for s in chunk) / frame > 330 * 330:
            loud += 1
            if loud >= 6:
                return True
    return False


def _first(root, pattern):
    return next(iter(sorted(root.glob(pattern))), None)


class Voice:
    def __init__(self, root, log_dir):
        root, self.log_dir = Path(root).resolve(), Path(log_dir).resolve()
        self.hear_exe = _first(root, "ThirdParty/whisper/*/Release/whisper-server.exe")
        self.hear_model = root / "Models" / "voice" / "ggml-base.en.bin"
        self.speak_exe = _first(root, "ThirdParty/sherpa-onnx/*/bin/sherpa-onnx-offline-tts.exe")
        self.kokoro = root / "Models" / "voice" / "kokoro-en-v0_19"
        self.can_hear = bool(self.hear_exe) and self.hear_model.is_file()
        self.can_speak = bool(self.speak_exe) and (self.kokoro / "model.onnx").is_file()
        self.cache = self.log_dir.parent / "voice-cache"
        self._http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        self._lock = threading.RLock()
        self._hearing = threading.Lock()   # one transcription at a time
        self._speaking = threading.Lock()  # one synthesis at a time
        self._closed = threading.Event()
        self._process = self._job = self._url = None
        self.hear_state = "idle" if self.can_hear else "missing"
        self.hear_error = ""

    # ---------- status ----------

    def status(self):
        with self._lock:
            return {"hear": self.hear_state, "hearError": self.hear_error, "speak": self.can_speak,
                    "voices": sorted(VOICES)}

    # ---------- hearing (whisper.cpp) ----------

    def start(self):
        """Load the speech model in the background. Safe to call again after a failure."""
        with self._lock:
            if not self.can_hear or self.hear_state in ("loading", "ready"):
                return
            self.hear_state, self.hear_error = "loading", ""

        def load():
            try:
                self._launch()
                state, error = "ready", ""
            except Exception as exc:  # the game goes on without the microphone
                logging.exception("Speech recognition did not start")
                self._stop()
                state, error = "failed", str(exc)
            with self._lock:
                self.hear_state, self.hear_error = state, error

        threading.Thread(target=load, daemon=True, name="voice-hearing").start()

    def _launch(self):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        threads = str(min(8, max(2, (os.cpu_count() or 4) - 2)))
        args = [str(self.hear_exe), "-m", str(self.hear_model), "--host", "127.0.0.1", "--port", str(port),
                "-t", threads, "-l", "en", "-nt", "-sns", "--prompt", HEARING_PROMPT]
        self.log_dir.mkdir(parents=True, exist_ok=True)
        log_path = self.log_dir / f"whisper-{os.getpid()}.log"
        with self._lock, log_path.open("wb") as log:
            if self._closed.is_set():
                raise RuntimeError("Voice stopped.")
            self._url = f"http://127.0.0.1:{port}"
            self._process = subprocess.Popen(args, cwd=self.hear_exe.parent, stdin=subprocess.DEVNULL, stdout=log,
                                             stderr=subprocess.STDOUT, creationflags=_NO_WINDOW)
            if os.name == "nt":
                try:  # the same kill-with-the-game job the dialogue model uses
                    from local_model import _WindowsJob
                    self._job = _WindowsJob(self._process)
                except Exception:
                    logging.exception("No job object for whisper-server; it is stopped on close instead")
            process, url = self._process, self._url
        deadline = time.monotonic() + 60
        while not self._closed.is_set() and time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"whisper-server exited ({process.returncode}). See {log_path}")
            try:
                with self._http.open(url + "/", timeout=1) as response:
                    if response.status == 200:
                        return
            except (OSError, ValueError, urllib.error.URLError):
                pass  # still loading the model
            self._closed.wait(0.2)
        raise RuntimeError(f"Speech recognition timed out while loading. See {log_path}")

    def transcribe(self, wav, wait=True):
        """Text for a 16 kHz mono WAV. wait=False returns None at once if another clip is being heard."""
        if not (wav[:4] == b"RIFF" and wav[8:12] == b"WAVE") or len(wav) > MAX_WAV_BYTES:
            raise ValueError("Send a WAV recording of up to one minute.")
        with self._lock:
            if self.hear_state != "ready" or self._process is None or self._process.poll() is not None:
                raise RuntimeError("Speech recognition is not running.")
            url = self._url + "/inference"
        if not _has_speech(wav):
            return ""
        if not self._hearing.acquire(blocking=wait):
            return None
        try:
            boundary = uuid.uuid4().hex
            fields = {"response_format": "json", "temperature": "0.0", "prompt": HEARING_PROMPT}
            body = b"".join(f'--{boundary}\r\nContent-Disposition: form-data; name="{k}"\r\n\r\n{v}\r\n'.encode()
                            for k, v in fields.items())
            body += (f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="line.wav"\r\n'
                     "Content-Type: audio/wav\r\n\r\n").encode() + wav + f"\r\n--{boundary}--\r\n".encode()
            request = urllib.request.Request(url, data=body, headers={
                "Content-Type": f"multipart/form-data; boundary={boundary}"})
            try:
                with self._http.open(request, timeout=60) as response:
                    text = json.load(response).get("text", "")
            except (OSError, ValueError) as exc:
                raise RuntimeError("Speech recognition could not hear that. Try again.") from exc
        finally:
            self._hearing.release()
        heard = " ".join(_NOISE.sub(" ", text).split())
        return "" if heard and heard.lower().rstrip(".") in HEARING_PROMPT.lower() else heard  # the prompt, echoed

    # ---------- speaking (Kokoro) ----------

    def speak(self, suspect, text):
        """WAV bytes of the suspect saying the line."""
        if not self.can_speak:
            raise RuntimeError("Character voices are not installed.")
        line = " ".join(str(text).replace("’", "'").replace("…", "...").split())[:MAX_SPOKEN_CHARS]
        line = line.lstrip("-–— ")  # a leading dash would be read as a command-line option
        if not re.search(r"[A-Za-z]", line):
            raise ValueError("There is nothing to say.")
        sid, pace = VOICES.get(suspect, DEFAULT_VOICE)
        name = hashlib.sha1(f"{sid}|{pace}|{line}".encode()).hexdigest() + ".wav"
        target = self.cache / name
        with self._speaking:
            if not target.is_file():
                self.cache.mkdir(parents=True, exist_ok=True)
                partial = self.cache / (name + ".part")
                args = [str(self.speak_exe),
                        f"--kokoro-model={self.kokoro / 'model.onnx'}", f"--kokoro-voices={self.kokoro / 'voices.bin'}",
                        f"--kokoro-tokens={self.kokoro / 'tokens.txt'}", f"--kokoro-data-dir={self.kokoro / 'espeak-ng-data'}",
                        f"--kokoro-length-scale={pace}", f"--sid={sid}", f"--num-threads={min(4, os.cpu_count() or 2)}",
                        f"--output-filename={partial}", line]
                try:
                    done = subprocess.run(args, cwd=self.speak_exe.parent, stdin=subprocess.DEVNULL,
                                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=90,
                                          creationflags=_NO_WINDOW)
                except (OSError, subprocess.TimeoutExpired) as exc:
                    raise RuntimeError("The voice could not be made.") from exc
                if done.returncode != 0 or not partial.is_file() or partial.stat().st_size < 1000:
                    logging.error("Kokoro failed (%s): %s", done.returncode, done.stderr[-600:].decode("utf-8", "replace"))
                    partial.unlink(missing_ok=True)
                    raise RuntimeError("The voice could not be made.")
                partial.replace(target)
            return target.read_bytes()

    # ---------- lifetime ----------

    def _stop(self):
        with self._lock:
            process, job = self._process, self._job
            self._process = self._job = self._url = None
        if job:
            job.close()
        elif process and process.poll() is None:
            process.terminate()
        if process:
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()

    def close(self):
        self._closed.set()
        self._stop()


def install(handler, voice):
    """Add /api/voice (status), /api/voice/hear (WAV in, text out) and /api/voice/speak (line in, WAV out)
    to the game's request handler, with the same Host and Origin checks as the rest of the API."""
    game_get, game_post = handler.do_GET, handler.do_POST

    def do_GET(self):
        if urlsplit(self.path).path != "/api/voice":
            return game_get(self)
        if self.correct_host():
            self.json_out(200, voice.status())

    def do_POST(self):
        url = urlsplit(self.path)
        if url.path not in ("/api/voice/hear", "/api/voice/speak"):
            return game_post(self)
        if not self.correct_host():
            return
        origin = self.headers.get("Origin")
        if origin and origin != f"http://{self.server.address}":
            self.send_error(403)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if url.path == "/api/voice/hear":
                if not 44 < length <= MAX_WAV_BYTES:
                    raise ValueError("Send a WAV recording of up to one minute.")
                final = parse_qs(url.query).get("final") == ["1"]
                text = voice.transcribe(self.rfile.read(length), wait=final)
                self.json_out(200, {"busy": True} if text is None else {"text": text})
            else:
                if not 1 <= length <= 4096:
                    raise ValueError("Invalid voice request.")
                body = json.loads(self.rfile.read(length))
                if not isinstance(body, dict) or not isinstance(body.get("text"), str):
                    raise ValueError("Invalid voice request.")
                self.output(200, voice.speak(body.get("suspect"), body["text"]), "audio/wav")
        except (ValueError, TypeError) as exc:
            self.json_out(400, {"error": str(exc)})
        except RuntimeError as exc:
            self.json_out(503, {"error": str(exc)})
        except Exception:
            logging.exception("Voice request failed")
            self.json_out(500, {"error": "Voice is not working. You can still type."})

    handler.do_GET, handler.do_POST = do_GET, do_POST

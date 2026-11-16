"""Run the Black Spade's bundled llama.cpp model on the local machine."""

import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request


class _JobLimits(ctypes.Structure):
    # JOBOBJECT_BASIC_LIMIT_INFORMATION, followed by the remaining fields of
    # JOBOBJECT_EXTENDED_LIMIT_INFORMATION. Windows uses its native alignment.
    _fields_ = [
        ("process_time", ctypes.c_int64), ("job_time", ctypes.c_int64),
        ("flags", wintypes.DWORD), ("min_working_set", ctypes.c_size_t),
        ("max_working_set", ctypes.c_size_t), ("active_processes", wintypes.DWORD),
        ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD),
        ("scheduling", wintypes.DWORD), ("io_counters", ctypes.c_uint64 * 6),
        ("memory_limits_and_peaks", ctypes.c_size_t * 4),
    ]


class _WindowsJob:
    """Windows kills every attached server when the game exits, even on a crash."""

    def __init__(self, process):
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
        kernel.CreateJobObjectW.restype = wintypes.HANDLE
        kernel.SetInformationJobObject.argtypes = [
            wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
        kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel = kernel
        self.handle = kernel.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            limits = _JobLimits()
            limits.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if not kernel.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            if not kernel.AssignProcessToJobObject(self.handle, int(process._handle)):
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            self.close()
            raise

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


class LocalModel:
    def __init__(self, root: Path, log_dir: Path):
        self.root = Path(root).resolve()
        self.log_dir = Path(log_dir).resolve()
        self.label = "Loading local model"
        self._http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
        self._lock = threading.RLock()
        self._closed = threading.Event()
        self._process = None
        self._job = None
        self._url = None

    @property
    def alive(self):
        with self._lock:
            return self._process is not None and self._process.poll() is None

    def start(self):
        if self.alive:
            return
        self._stop_process()  # Release any job handle left after a failed interview.
        builds = self.root / "ThirdParty" / "llama"
        models = self.root / "Models"
        large = models / "gemma-4-E4B-it-Q4_K_M.gguf"
        small = models / "gemma-4-E2B-it-Q4_K_M.gguf"
        gpu = builds / "b12368-vulkan" / "llama-server.exe"
        cpu = builds / "b12368-cpu" / "llama-server.exe"
        self.log_dir.mkdir(parents=True, exist_ok=True)
        attempts = []
        if os.environ.get("BLACK_SPADE_CPU") != "1" and gpu.is_file() and large.is_file():
            attempts.append(("Vulkan", gpu, large))
        cpu_exe = cpu if cpu.is_file() else gpu
        cpu_model = small if small.is_file() else large
        if cpu_exe.is_file() and cpu_model.is_file():
            attempts.append(("CPU", cpu_exe, cpu_model))
        if not attempts:
            raise RuntimeError("The local model files are missing. Extract the full game folder, including Models and ThirdParty.")
        failures = []
        for backend, exe, model in attempts:
            if self._closed.is_set():
                raise RuntimeError("Local model stopped.")
            try:
                self._launch(backend, exe, model)
                self.label = f"Gemma 4 {'E4B' if model == large else 'E2B'} · {backend}"
                return
            except (OSError, RuntimeError) as exc:
                self._stop_process()
                if self._closed.is_set():
                    raise RuntimeError("Local model stopped.") from exc
                failures.append(f"{backend}: {exc}")
        raise RuntimeError("Local model could not start. " + " / ".join(failures))

    def _launch(self, backend, exe, model):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        args = [str(exe), "-m", str(model), "--host", "127.0.0.1", "--port", str(port),
                "-c", "24576", "--parallel", "6", "--jinja", "--no-webui", "-fa", "on",
                "-ctk", "q8_0", "-ctv", "q8_0", "--reasoning", "off", "--reasoning-budget", "0"]
        env = os.environ.copy()
        if backend == "Vulkan":
            args.extend(["-ngl", "99"])
            env.setdefault("GGML_VK_VISIBLE_DEVICES", "0")
        else:
            args.extend(["-ngl", "0", "--device", "none", "-t", str(min(8, max(2, (os.cpu_count() or 4) - 2)))])
        log_path = self.log_dir / f"llama-{backend.lower()}-{os.getpid()}.log"
        with self._lock, log_path.open("wb") as log:
            if self._closed.is_set():
                raise RuntimeError("Local model stopped.")
            self._url = f"http://127.0.0.1:{port}"
            self._process = subprocess.Popen(
                args, cwd=exe.parent, env=env, stdin=subprocess.DEVNULL, stdout=log,
                stderr=subprocess.STDOUT, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
            if os.name == "nt":
                self._job = _WindowsJob(self._process)
            process, url = self._process, self._url
        deadline = time.monotonic() + 120
        while not self._closed.is_set() and time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError(f"llama-server exited ({process.returncode}). See {log_path}")
            try:
                with self._http.open(url + "/health", timeout=1) as response:
                    if response.status == 200 and json.load(response).get("status") == "ok":
                        return
            except (OSError, ValueError, urllib.error.URLError):
                pass  # Connection refused or HTTP 503 while loading the model.
            self._closed.wait(0.25)
        raise RuntimeError(f"Startup timed out. See {log_path}")

    def _chat_request(self, messages, schema, slot, max_tokens):
        with self._lock:
            if self._process is None or self._process.poll() is not None:
                raise RuntimeError("The local model has stopped. Retry the interview engine to continue.")
            url = self._url + "/v1/chat/completions"
        body = {
            "messages": messages, "id_slot": slot, "cache_prompt": True,
            "temperature": 0.8, "max_tokens": max_tokens,
            "chat_template_kwargs": {"enable_thinking": False},
            "response_format": {"type": "json_schema", "json_schema": {"name": "reply", "schema": schema}},
        }
        return urllib.request.Request(url, data=json.dumps(body).encode("utf-8"),
                                      headers={"Content-Type": "application/json"})

    def warm(self, messages, schema, slot):
        request = self._chat_request(messages, schema, slot, 1)
        with self._http.open(request, timeout=120) as response:
            response.read()

    def reply(self, messages, schema, slot):
        req = self._chat_request(messages, schema, slot, 160)
        # Occasionally generation ends before the closing brace. Retry once on
        # the same cached prefix. Game state only changes after a valid reply.
        for attempt in range(2):
            try:
                with self._http.open(req, timeout=120) as response:
                    raw = json.load(response)["choices"][0]["message"]["content"]
                parsed = json.loads(raw)
                if not isinstance(parsed, dict) or not all(isinstance(parsed.get(k), str) for k in schema["required"]):
                    raise ValueError("Incomplete reply")
                if not parsed["r"].strip() or any(
                        parsed[key] not in prop["enum"] for key, prop in schema["properties"].items() if "enum" in prop):
                    raise ValueError("Invalid reply")
                return parsed, raw
            except urllib.error.HTTPError as exc:
                raise RuntimeError(f"The local model returned HTTP {exc.code}. Try the line again.") from exc
            except OSError as exc:
                raise RuntimeError("The local model could not answer. Try the line again.") from exc
            except (KeyError, IndexError, TypeError, ValueError) as exc:
                if attempt:
                    raise RuntimeError("The suspect did not finish an answer. Try the line again.") from exc

    def _stop_process(self):
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
                    process.wait(timeout=5)

    def close(self):
        self._closed.set()
        self._stop_process()

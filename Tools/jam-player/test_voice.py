"""Tests for voice.py. Run: python Tools/jam-player/test_voice.py

The first checks need nothing installed. If the voice files are present (Tools/fetch-voice.ps1), every suspect's
voice then says a line and the speech recogniser has to write it back: a test of both halves that needs no ears
and no microphone.
"""

import array
import io
import re
import sys
import tempfile
import time
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import voice  # noqa: E402

ROOT = HERE.parents[1]


def check(name, cond):
    print(("PASS " if cond else "FAIL ") + name)
    return bool(cond)


def clip(samples, rate=16000):
    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array.array("h", samples).tobytes())
    return out.getvalue()


def words(text):
    text = text.lower().replace("$20", "twenty dollars").replace("11 o'clock", "eleven o'clock").replace("mr.", "mister")
    return re.findall(r"[a-z']+", text.replace("mr ", "mister "))


def main():
    ok = True
    ok &= check("silence is not speech", not voice._has_speech(clip([0] * 32000)))
    ok &= check("room hum is not speech", not voice._has_speech(clip([120, -120] * 16000)))
    ok &= check("a loud clip is speech", voice._has_speech(clip([4000, -4000] * 4000)))
    ok &= check("a click is not speech", not voice._has_speech(clip([0] * 8000 + [20000] * 200 + [0] * 8000)))
    try:
        voice._has_speech(b"RIFF....WAVEjunk")
        ok &= check("a broken upload is refused", False)
    except ValueError:
        ok &= check("a broken upload is refused", True)
    ok &= check("every suspect in the case has a voice", set(voice.VOICES) == {
        p.stem for p in (ROOT / "Content/Llm/Cases/SpadeClub").glob("*_*.json")})
    ok &= check("no two suspects share a voice", len({sid for sid, _ in voice.VOICES.values()}) == len(voice.VOICES))

    with tempfile.TemporaryDirectory() as tmp:
        v = voice.Voice(ROOT, Path(tmp) / "logs")
        if not (v.can_hear and v.can_speak):
            print("\nVoice files are not installed (Tools/fetch-voice.ps1): skipping the spoken round trip.")
        else:
            v.start()
            while v.status()["hear"] == "loading":
                time.sleep(0.2)
            ok &= check("speech recognition starts", v.status()["hear"] == "ready")
            lines = {
                "vera_lane": "Did Vera Lane pay Tommy Mullen twenty dollars to cut the lights?",
                "mickey_doyle": "Mickey, was there a gun behind the bar tonight?",
                "harlan_graves": "What was Ace Calloway going to announce tonight, Mister Graves?",
                "tommy_mullen": "Who asked you to pull the fuse at eleven o'clock?",
                "allison_wu": "Where were you during the blackout, Miss Wu?",
                "logan_wu": "Tell me about the loan and your business with Ace.",
            }
            try:
                for sid, line in lines.items():
                    started = time.time()
                    heard = v.transcribe(v.speak(sid, line))
                    ok &= check(f"{sid} is heard back ({time.time() - started:.1f}s): {heard}", words(heard) == words(line))
                ok &= check("silence comes back empty", v.transcribe(clip([0] * 32000)) == "")
            finally:
                v.close()

    print("\nALL PASS" if ok else "\nSOME FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

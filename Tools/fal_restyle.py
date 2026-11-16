"""Redraw a character portrait in the style of a reference portrait (fal nano-banana edit), and prime a
fal_suspect.py job so `python3 Tools/fal_suspect.py <out_dir> <name> --resume` animates the new portrait.

Usage:  python3 Tools/fal_restyle.py <style_ref.png> <character.png> <out_dir> <name> <prompt> <motion_prompt>
Needs FAL_KEY in the environment and `sips` (macOS) for JPEG conversion.
"""

import base64
import json
import os
import subprocess
import sys
import tempfile
import time
import urllib.request

KEY = os.environ["FAL_KEY"]


def http(url, body=None):
    req = urllib.request.Request(url, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": f"Key {KEY}", "Content-Type": "application/json"},
                                 method="POST" if body is not None else "GET")
    return json.load(urllib.request.urlopen(req, timeout=120))


def data_uri(path):
    jpg = os.path.join(tempfile.mkdtemp(), "img.jpg")
    subprocess.run(["sips", "-s", "format", "jpeg", "-s", "formatOptions", "90", path, "--out", jpg], check=True, capture_output=True)
    return "data:image/jpeg;base64," + base64.b64encode(open(jpg, "rb").read()).decode()


def main():
    style, character, out_dir, name, prompt, motion = sys.argv[1:7]
    job = http("https://queue.fal.run/fal-ai/nano-banana/edit", {
        "prompt": prompt, "image_urls": [data_uri(style), data_uri(character)], "aspect_ratio": "16:9", "output_format": "png"})
    print("restyle queued", job["request_id"], flush=True)
    while True:
        status = http(job["status_url"]).get("status")
        if status == "COMPLETED":
            break
        if status not in ("IN_QUEUE", "IN_PROGRESS"):
            raise RuntimeError(f"restyle failed: {status}")
        time.sleep(5)
    url = http(job["response_url"])["images"][0]["url"]
    os.makedirs(out_dir, exist_ok=True)
    urllib.request.urlretrieve(url, os.path.join(out_dir, f"{name}.png"))
    json.dump({"portrait_prompt": prompt, "motion_prompt": motion, "image_url": url},
              open(os.path.join(out_dir, f"{name}_job.json"), "w"), indent=2)
    print("restyled portrait saved", flush=True)


if __name__ == "__main__":
    main()

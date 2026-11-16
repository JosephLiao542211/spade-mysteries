"""Make a suspect clip on fal.ai: FLUX Ultra portrait -> Kling 2.1 Pro looping video (same start and end frame).

Usage:
  python3 Tools/fal_suspect.py <out_dir> <name> <portrait_prompt> <motion_prompt>
  python3 Tools/fal_suspect.py <out_dir> <name> --resume        # finish after going offline

Writes <out_dir>/<name>.png, <name>_idle.mp4 and <name>_job.json (fal request ids, for resuming).
Needs FAL_KEY in the environment.
"""

import json
import os
import sys
import time
import urllib.request

KEY = os.environ["FAL_KEY"]
FLUX = "fal-ai/flux-pro/v1.1-ultra"
KLING = "fal-ai/kling-video/v2.1/pro/image-to-video"


def http(url, body=None):
    req = urllib.request.Request(
        url,
        data=json.dumps(body).encode() if body is not None else None,
        headers={"Authorization": f"Key {KEY}", "Content-Type": "application/json"},
        method="POST" if body is not None else "GET",
    )
    return json.load(urllib.request.urlopen(req, timeout=120))


def submit(model, args):
    job = http(f"https://queue.fal.run/{model}", args)
    return {"model": model, "request_id": job["request_id"], "status_url": job["status_url"], "response_url": job["response_url"]}


def wait(job):
    while True:
        try:
            status = http(job["status_url"]).get("status")
        except OSError as error:  # offline: keep trying, the job keeps running on fal
            print("network error, retrying:", error, flush=True)
            time.sleep(30)
            continue
        if status == "COMPLETED":
            return http(job["response_url"])
        if status not in ("IN_QUEUE", "IN_PROGRESS"):
            raise RuntimeError(f"job failed: {status}")
        time.sleep(10)


def main():
    out_dir, name = sys.argv[1], sys.argv[2]
    state_path = os.path.join(out_dir, f"{name}_job.json")
    os.makedirs(out_dir, exist_ok=True)
    if sys.argv[3] == "--resume":
        state = json.load(open(state_path))
    else:
        state = {"portrait_prompt": sys.argv[3], "motion_prompt": sys.argv[4]}

    def save():
        json.dump(state, open(state_path, "w"), indent=2)

    png = os.path.join(out_dir, f"{name}.png")
    if "image_url" not in state:
        if "portrait_job" not in state:
            state["portrait_job"] = submit(FLUX, {"prompt": state["portrait_prompt"], "aspect_ratio": "16:9", "output_format": "png", "safety_tolerance": os.environ.get("FAL_SAFETY", "2")})
            save()
            print("portrait queued", state["portrait_job"]["request_id"], flush=True)
        result = wait(state["portrait_job"])
        if any(result.get("has_nsfw_concepts") or []):
            del state["portrait_job"]
            save()
            raise RuntimeError("portrait was blanked by fal's safety filter; reword the prompt or raise FAL_SAFETY")
        state["image_url"] = result["images"][0]["url"]
        save()
    if not os.path.exists(png):
        urllib.request.urlretrieve(state["image_url"], png)
        print("portrait saved", png, flush=True)

    mp4 = os.path.join(out_dir, f"{name}_idle.mp4")
    if "video_job" not in state:
        state["video_job"] = submit(KLING, {
            "prompt": state["motion_prompt"], "image_url": state["image_url"], "tail_image_url": state["image_url"], "duration": "10",
            "negative_prompt": "camera movement, zoom, pan, cut, blur, distortion, extra fingers, color", "cfg_scale": 0.5})
        save()
        print("video queued", state["video_job"]["request_id"], flush=True)
    if not os.path.exists(mp4):
        urllib.request.urlretrieve(wait(state["video_job"])["video"]["url"], mp4)
    print("video saved", mp4, os.path.getsize(mp4), flush=True)


if __name__ == "__main__":
    main()

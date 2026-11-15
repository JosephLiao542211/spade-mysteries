"""Cut a suspect clip out of its background for the game: BiRefNet video matte on fal.ai, then pack colour + matte
into one H.264 file (top half = colour, bottom half = matte) that the NoirVideoCard material splits back apart.

Usage:  python3 Tools/fal_matte.py <kling_request_id> <colour_source.mp4> <out_stacked.mp4>
Needs FAL_KEY in the environment and ffmpeg on PATH.
"""

import json
import os
import subprocess
import sys
import time
import urllib.request

KEY = os.environ["FAL_KEY"]


def http(url, body=None):
    req = urllib.request.Request(url, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": f"Key {KEY}", "Content-Type": "application/json"},
                                 method="POST" if body is not None else "GET")
    return json.load(urllib.request.urlopen(req, timeout=120))


def wait(job):
    while True:
        try:
            status = http(job["status_url"]).get("status")
        except OSError as error:
            print("network error, retrying:", error, flush=True)
            time.sleep(30)
            continue
        if status == "COMPLETED":
            return http(job["response_url"])
        if status not in ("IN_QUEUE", "IN_PROGRESS"):
            raise RuntimeError(f"job failed: {status}")
        time.sleep(5)


def main():
    kling_id, colour, out = sys.argv[1], sys.argv[2], sys.argv[3]
    video_url = http(f"https://queue.fal.run/fal-ai/kling-video/requests/{kling_id}")["video"]["url"]
    job = http("https://queue.fal.run/fal-ai/birefnet/v2/video", {
        "video_url": video_url, "model": "General Use (Heavy)", "operating_resolution": "1024x1024",
        "output_mask": True, "refine_foreground": False, "video_quality": "high"})
    print("matte queued", job["request_id"], flush=True)
    result = wait(job)
    mask = out.replace(".mp4", "_mask.mp4")
    urllib.request.urlretrieve(result["mask_video"]["url"], mask)
    # Top: colour, bottom: matte; 1280 wide keeps it inside H.264 Baseline level 4.1 (plays in Unreal's Electra player).
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", colour, "-i", mask, "-filter_complex",
                    "[0:v]scale=1280:712,format=yuv420p[c];[1:v]scale=1280:712,format=gray,format=yuv420p[m];[c][m]vstack",
                    "-an", "-c:v", "libx264", "-profile:v", "baseline", "-level", "4.1", "-crf", "16", "-r", "24",
                    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out], check=True)
    print("stacked", out, flush=True)


if __name__ == "__main__":
    main()

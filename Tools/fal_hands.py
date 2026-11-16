"""Trace a suspect's hands through their clip (fal SAM 2 video, box prompt on frame 0) and build a 3-row clip:
row 1 = colour, row 2 = body matte, row 3 = hands matte. The game draws the hands row on a second card in front of the desk.

Usage:  python3 Tools/fal_hands.py <kling_request_id> <colour.mp4> <body_mask.mp4> <x_min> <y_min> <x_max> <y_max> <out.mp4>
Box coordinates are in the clip's own pixels (1928x1072). Needs FAL_KEY and ffmpeg.
"""

import json
import os
import subprocess
import sys
import time
import urllib.request

KEY = os.environ["FAL_KEY"]
W, H = 960, 534  # per row; 3 rows stay inside H.264 Baseline level 4.1


def http(url, body=None):
    req = urllib.request.Request(url, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": f"Key {KEY}", "Content-Type": "application/json"},
                                 method="POST" if body is not None else "GET")
    return json.load(urllib.request.urlopen(req, timeout=120))


def main():
    kling_id, colour, body, x0, y0, x1, y1, out = sys.argv[1:9]
    video_url = http(f"https://queue.fal.run/fal-ai/kling-video/requests/{kling_id}")["video"]["url"]
    body_args = {"video_url": video_url, "apply_mask": False}
    if os.environ.get("NOIR_POINTS"):
        # JSON list of [x, y, label] on frame 0 (label 1 = include, 0 = exclude), e.g. along both arms.
        body_args["prompts"] = [{"frame_index": 0, "x": int(x), "y": int(y), "label": int(l)} for x, y, l in json.loads(os.environ["NOIR_POINTS"])]
    else:
        body_args["box_prompts"] = [{"frame_index": 0, "x_min": int(x0), "y_min": int(y0), "x_max": int(x1), "y_max": int(y1)}]
    job = http("https://queue.fal.run/fal-ai/sam2/video", body_args)
    print("hands queued", job["request_id"], flush=True)
    while True:
        try:
            status = http(job["status_url"]).get("status")
        except OSError as error:
            print("network error, retrying:", error, flush=True)
            time.sleep(20)
            continue
        if status == "COMPLETED":
            break
        if status not in ("IN_QUEUE", "IN_PROGRESS"):
            raise RuntimeError(f"hands failed: {status}")
        time.sleep(5)
    hands = out.replace(".mp4", "_hands.mp4")
    urllib.request.urlretrieve(http(job["response_url"])["video"]["url"], hands)
    print("hands mask saved", hands, flush=True)
    if len(sys.argv) > 9 and sys.argv[9] == "--mask-only":
        return
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", colour, "-i", body, "-i", hands, "-filter_complex",
                    f"[0:v]scale={W}:{H},format=yuv420p[c];[1:v]scale={W}:{H},format=gray,format=yuv420p[b];"
                    f"[2:v]scale={W}:{H},format=gray,lutyuv=y='if(gt(val,40),255,0)',format=yuv420p[h];[c][b][h]vstack=inputs=3",
                    "-an", "-c:v", "libx264", "-profile:v", "baseline", "-level", "4.1", "-crf", "16", "-r", "24",
                    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out], check=True)
    print("stacked", out, flush=True)


if __name__ == "__main__":
    main()

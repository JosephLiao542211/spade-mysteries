"""Trim a stacked (colour over matte) suspect clip so nothing below the hands is drawn, frame by frame.

Usage:  python3 Tools/trim_below_hands.py <stacked_in.mp4> <hands_mask.mp4> <rest_frac> <out.mp4>
For each frame the cut is the lowest hand pixel (from a SAM hands mask), but never above rest_frac
(fraction of the clip height where the hands usually rest), so a raised hand doesn't cut the body.
Then place the card so the clip's lowest hand point sits on the desk: nothing under the hands is left to show.
"""

import subprocess
import sys

import numpy as np

W, H = 1280, 712


def frames(path, w, h, pix):
    proc = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", path, "-vf", f"scale={w}:{h}", "-f", "rawvideo", "-pix_fmt", pix, "-"],
                            stdout=subprocess.PIPE)
    size = w * h * (3 if pix == "rgb24" else 1)
    while True:
        buf = proc.stdout.read(size)
        if len(buf) < size:
            break
        yield np.frombuffer(buf, np.uint8).reshape(h, w, -1) if pix == "rgb24" else np.frombuffer(buf, np.uint8).reshape(h, w)


def main():
    stacked, hands, rest, out = sys.argv[1], sys.argv[2], float(sys.argv[3]), sys.argv[4]
    enc = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H * 2}", "-r", "24", "-i", "-",
                            "-an", "-c:v", "libx264", "-profile:v", "baseline", "-level", "4.1", "-crf", "16", "-pix_fmt", "yuv420p",
                            "-movflags", "+faststart", out], stdin=subprocess.PIPE)
    rest_row = int(rest * H)
    for frame, hand in zip(frames(stacked, W, H * 2, "rgb24"), frames(hands, W, H, "gray")):
        rows = np.where((hand > 100).sum(axis=1) > 6)[0]
        cut = max(int(rows.max()) if len(rows) else rest_row, rest_row)
        frame = frame.copy()
        frame[H + cut + 1:, :, :] = 0  # matte (bottom half): nothing below the hands
        enc.stdin.write(frame.tobytes())
    enc.stdin.close()
    enc.wait()
    print("trimmed", out)


if __name__ == "__main__":
    main()

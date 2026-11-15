"""Make a matte solid below the character's outline (fills holes in dark sleeves/hands), then re-stack colour + matte.

Usage:  python3 Tools/fill_matte.py <colour.mp4> <mask.mp4> <out_stacked.mp4>
Each matte column is filled from its first solid pixel down to the bottom, so arms and hands can't go see-through.
"""

import subprocess
import sys

import numpy as np

W, H = 1280, 712


def main():
    colour, mask, out = sys.argv[1], sys.argv[2], sys.argv[3]
    dec = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-i", mask, "-vf", f"scale={W}:{H}", "-f", "rawvideo", "-pix_fmt", "gray", "-"],
                           stdout=subprocess.PIPE)
    filled = out.replace(".mp4", "_filledmask.mp4")
    enc = subprocess.Popen(["ffmpeg", "-loglevel", "error", "-y", "-f", "rawvideo", "-pix_fmt", "gray", "-s", f"{W}x{H}", "-r", "24", "-i", "-",
                            "-c:v", "libx264", "-crf", "12", "-pix_fmt", "yuv420p", filled], stdin=subprocess.PIPE)
    size = W * H
    while True:
        buf = dec.stdout.read(size)
        if len(buf) < size:
            break
        m = np.frombuffer(buf, np.uint8).reshape(H, W)
        solid = (m > 96).astype(np.uint8) * 255
        solid = np.maximum.accumulate(solid, axis=0)  # fill downward from the outline
        enc.stdin.write(np.maximum(m, solid).tobytes())
    enc.stdin.close()
    enc.wait()
    dec.wait()
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", colour, "-i", filled, "-filter_complex",
                    f"[0:v]scale={W}:{H},format=yuv420p[c];[1:v]scale={W}:{H},format=gray,format=yuv420p[m];[c][m]vstack",
                    "-an", "-c:v", "libx264", "-profile:v", "baseline", "-level", "4.1", "-crf", "16", "-r", "24",
                    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out], check=True)
    print("stacked", out)


if __name__ == "__main__":
    main()

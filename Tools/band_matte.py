"""Build a 3-row suspect clip whose 'hands' row is a solid band across the bottom of the character
(body matte between two heights, joined with the traced arms inside that band). The band is what the game
draws on top of the desk, so it can't have holes and never splits a raised arm from the body.

Usage:  python3 Tools/band_matte.py <colour.mp4> <body_mask.mp4> <arms_mask.mp4|-> <top 0-1> <bottom 0-1> <out.mp4>
"""

import subprocess
import sys

W, H = 960, 534
FADE = 0.05  # fraction of the clip height over which the strip fades out


def main():
    colour, body, arms, top, bottom, out = sys.argv[1:7]
    top, bottom = float(top), float(bottom)
    inputs = ["-i", colour, "-i", body] + (["-i", arms] if arms != "-" else [])
    union = "[b][a]blend=all_mode=lighten[u]" if arms != "-" else "[b]null[u]"
    arms_chain = f"[2:v]scale={W}:{H},format=gray[a];" if arms != "-" else ""
    graph = (f"[0:v]scale={W}:{H},format=yuv420p[c];"
             f"[1:v]scale={W}:{H},format=gray,split[b][b2];{arms_chain}{union};"
             # Solid inside the band, fading out over FADE at the top and bottom (the game dithers the fade).
             f"[u]geq=lum='if(gt(lum(X\\,Y)\\,90)\\,255*clip(min((Y/H-({top}-{FADE}))/{FADE}\\,(({bottom}+{FADE})-Y/H)/{FADE})\\,0\\,1)\\,0)',format=yuv420p[h];"
             f"[b2]format=yuv420p[bb];[c][bb][h]vstack=inputs=3")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", *inputs, "-filter_complex", graph, "-an",
                    "-c:v", "libx264", "-profile:v", "baseline", "-level", "4.1", "-crf", "16", "-r", "24",
                    "-pix_fmt", "yuv420p", "-movflags", "+faststart", out], check=True)
    print("stacked", out)


if __name__ == "__main__":
    main()

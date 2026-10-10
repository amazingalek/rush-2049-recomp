"""Make or apply a patch for an upscaled title picture (src/hires_font.cpp, "Upscaled title").

Usage: .venv/Scripts/python -I tools/rush2049/title_patch.py make <original.ppm> <title.png> <title_patch.png>
       .venv/Scripts/python -I tools/rush2049/title_patch.py apply <original.ppm> <title_patch.png> <title.png>

original.ppm is the title as the game draws it, written to hires_title/ by a run with title.png there. The patch is
the picture's size: per pixel, the picture minus the original pixel under it (mod 256), and in alpha a check byte
of that original pixel, which the game matches against each strip before it rebuilds the picture from it.
"""
import sys

import numpy as np
from PIL import Image

WIDTH, HEIGHT = 320, 220
# A channel out of 255 from 5 bits, as the game decodes the palette, and back.
EXPAND = np.array([c * 255 // 31 for c in range(32)], dtype=np.uint8)
FIVE = np.zeros(256, dtype=np.uint32)
FIVE[EXPAND] = np.arange(32, dtype=np.uint32)


def load_original(path):
    original = np.asarray(Image.open(path).convert("RGB"))
    if original.shape != (HEIGHT, WIDTH, 3):
        raise SystemExit(f"{path} is {original.shape[1]}x{original.shape[0]}, not {WIDTH}x{HEIGHT}")
    if not np.array_equal(EXPAND[FIVE[original]], original):
        raise SystemExit(f"{path} isn't the title as the game draws it (a colour that isn't 5 bits per channel)")
    return original


def check_bytes(original):
    five = FIVE[original]
    colour = (five[..., 0] << 10) | (five[..., 1] << 5) | five[..., 2]
    return ((colour * 40503) >> 8).astype(np.uint8)


def scale_of(image, path):
    height, width = image.shape[:2]
    scale = width // WIDTH
    if not 2 <= scale <= 8 or (width, height) != (WIDTH * scale, HEIGHT * scale):
        raise SystemExit(f"{path} is {width}x{height}, not {WIDTH}x{HEIGHT} times 2 to 8")
    return scale


def main():
    if len(sys.argv) != 5 or sys.argv[1] not in ("make", "apply"):
        raise SystemExit(__doc__)
    mode, original_path, in_path, out_path = sys.argv[1:]
    original = load_original(original_path)
    if mode == "make":
        picture = np.asarray(Image.open(in_path).convert("RGB"))
        scale = scale_of(picture, in_path)
        under = original.repeat(scale, 0).repeat(scale, 1)
        patch = np.empty(picture.shape[:2] + (4,), dtype=np.uint8)
        patch[..., :3] = picture - under
        patch[..., 3] = check_bytes(original).repeat(scale, 0).repeat(scale, 1)
        Image.fromarray(patch, "RGBA").save(out_path, optimize=True)
    else:
        patch = np.asarray(Image.open(in_path).convert("RGBA"))
        scale = scale_of(patch, in_path)
        if not np.array_equal(patch[::scale, ::scale, 3], check_bytes(original)):
            raise SystemExit(f"{in_path} was made for a different title than {original_path}")
        under = original.repeat(scale, 0).repeat(scale, 1)
        Image.fromarray(patch[..., :3] + under, "RGB").save(out_path)


if __name__ == "__main__":
    main()

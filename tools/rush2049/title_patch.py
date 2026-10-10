"""Make or apply the hi-res title patch (src/hires_font.cpp, "Hi-res title screen").

Usage: .venv/Scripts/python -I tools/rush2049/title_patch.py make <original.ppm> <title.png> <title.patch>
       .venv/Scripts/python -I tools/rush2049/title_patch.py apply <original.ppm> <title.patch> <title.png>
Needs numpy, Pillow and zstandard in the venv.

original.ppm is the title as the game draws it, written to hires_title/ in the app folder by a run with title.png
there. The patch describes the picture in terms of it and is useless without it:

    "R49TITLE", then little-endian u32 version (1), u32 scale, u64 check, u32 payload size, then the payload.

The picture minus a bilinear upscale of the original, per channel and mod 256, goes through a reversible colour
transform (R-G, G, B-G, each +128) and the JPEG-LS median predictor, plane by plane, and is compressed with zstd.
The payload is that, XORed with a keystream seeded from the original; the check, from the same seed, tells the game
it has the right original before it unpacks anything.
"""
import struct
import sys

import numpy as np
import zstandard
from PIL import Image

WIDTH, HEIGHT = 320, 220
MAGIC, VERSION = b"R49TITLE", 1
MASK = (1 << 64) - 1
GOLDEN = 0x9E3779B97F4A7C15


def load_original(path):
    original = np.asarray(Image.open(path).convert("RGB"))
    if original.shape != (HEIGHT, WIDTH, 3):
        raise SystemExit(f"{path} is {original.shape[1]}x{original.shape[0]}, not {WIDTH}x{HEIGHT}")
    return original


def mix(z):
    """splitmix64's output function, on a uint64 array."""
    z = (z ^ (z >> np.uint64(30))) * np.uint64(0xBF58476D1CE4E5B9)
    z = (z ^ (z >> np.uint64(27))) * np.uint64(0x94D049BB133111EB)
    return z ^ (z >> np.uint64(31))


def seed_of(original):
    """FNV-1a 64 of the original's RGB bytes."""
    h = 0xCBF29CE484222325
    for byte in original.tobytes():
        h = ((h ^ byte) * 0x100000001B3) & MASK
    return h


def check_of(seed):
    with np.errstate(over="ignore"):
        return int(mix(np.array([seed ^ 0x5449544C45434B31], dtype=np.uint64))[0])


def keystream(seed, size):
    with np.errstate(over="ignore"):
        counter = np.arange(1, (size + 7) // 8 + 1, dtype=np.uint64) * np.uint64(GOLDEN) + np.uint64(seed)
        return mix(counter).astype("<u8").tobytes()[:size]


def bilinear(original, scale):
    """The original upscaled with integer weights, sampling at pixel centres and clamping at the edges."""
    n = 2 * scale
    def axis(size):
        f = 2 * np.arange(size * scale) + 1 - scale
        i0 = np.floor_divide(f, n)
        w = f - i0 * n
        return np.clip(i0, 0, size - 1), np.clip(i0 + 1, 0, size - 1), w
    x0, x1, wx = axis(WIDTH)
    y0, y1, wy = axis(HEIGHT)
    o = original.astype(np.int64)
    top = o[y0][:, x0] * (n - wx)[None, :, None] + o[y0][:, x1] * wx[None, :, None]
    bottom = o[y1][:, x0] * (n - wx)[None, :, None] + o[y1][:, x1] * wx[None, :, None]
    return ((top * (n - wy)[:, None, None] + bottom * wy[:, None, None] + n * n // 2) // (n * n)).astype(np.uint8)


def to_planes(picture, base):
    r = (picture.astype(np.int32) - base) & 255
    t = np.stack([r[..., 0] - r[..., 1] + 128, r[..., 1] + 128, r[..., 2] - r[..., 1] + 128]) & 255
    return t.astype(np.uint8)


def from_planes(t, base):
    t = t.astype(np.int32) - 128
    g = t[1]
    r = np.stack([t[0] + g, g, t[2] + g], axis=-1)
    return ((r + base) & 255).astype(np.uint8)


def median_predict(a, b, c):
    return np.where(c >= np.maximum(a, b), np.minimum(a, b), np.where(c <= np.minimum(a, b), np.maximum(a, b), a + b - c))


def encode(planes):
    v = planes.astype(np.int32)
    a = np.zeros_like(v); a[:, :, 1:] = v[:, :, :-1]
    b = np.zeros_like(v); b[:, 1:] = v[:, :-1]
    c = np.zeros_like(v); c[:, 1:, 1:] = v[:, :-1, :-1]
    return ((v - median_predict(a, b, c)) & 255).astype(np.uint8)


def decode(d):
    """The inverse of encode, a row at a time: each row's prediction needs the row above and its own left pixel."""
    planes, height, width = d.shape
    out = bytearray(planes * height * width)
    for p in range(planes):
        above = [0] * width
        for y in range(height):
            o = (p * height + y) * width
            row = d[p, y].tolist()
            left = left_above = 0
            for x in range(width):
                b = above[x]
                if left_above >= max(left, b):
                    guess = min(left, b)
                elif left_above <= min(left, b):
                    guess = max(left, b)
                else:
                    guess = left + b - left_above
                row[x] = left = (row[x] + guess) & 255
                left_above = b
            out[o:o + width] = bytes(row)
            above = row
    return np.frombuffer(bytes(out), dtype=np.uint8).reshape(d.shape)


def scale_of(width, height, what):
    scale = width // WIDTH
    if not 2 <= scale <= 8 or (width, height) != (WIDTH * scale, HEIGHT * scale):
        raise SystemExit(f"{what} is {width}x{height}, not {WIDTH}x{HEIGHT} times 2 to 8")
    return scale


def main():
    if len(sys.argv) != 5 or sys.argv[1] not in ("make", "apply"):
        raise SystemExit(__doc__)
    mode, original_path, in_path, out_path = sys.argv[1:]
    original = load_original(original_path)
    seed = seed_of(original)
    if mode == "make":
        picture = np.asarray(Image.open(in_path).convert("RGB"))
        scale = scale_of(picture.shape[1], picture.shape[0], in_path)
        planes = to_planes(picture, bilinear(original, scale))
        packed = zstandard.ZstdCompressor(level=22).compress(np.ascontiguousarray(encode(planes)).tobytes())
        payload = bytes(x ^ k for x, k in zip(packed, keystream(seed, len(packed))))
        with open(out_path, "wb") as f:
            f.write(MAGIC + struct.pack("<IIQI", VERSION, scale, check_of(seed), len(payload)) + payload)
        print(f"{out_path}: {len(payload) + 28} bytes, {scale}x")
    else:
        data = open(in_path, "rb").read()
        version, scale, check, size = struct.unpack_from("<IIQI", data, 8)
        if data[:8] != MAGIC or version != VERSION:
            raise SystemExit(f"{in_path} isn't a title patch")
        if check != check_of(seed):
            raise SystemExit(f"{in_path} was made for a different title than {original_path}")
        payload = data[28:28 + size]
        packed = bytes(x ^ k for x, k in zip(payload, keystream(seed, size)))
        d = np.frombuffer(zstandard.ZstdDecompressor().decompress(packed), dtype=np.uint8)
        planes = decode(d.reshape(3, HEIGHT * scale, WIDTH * scale))
        Image.fromarray(from_planes(planes, bilinear(original, scale)), "RGB").save(out_path)


if __name__ == "__main__":
    main()

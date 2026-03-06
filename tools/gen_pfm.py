#!/usr/bin/env python3
import argparse
import math
import struct
from dataclasses import dataclass


@dataclass(frozen=True)
class Vec3:
    r: float
    g: float
    b: float

    def clamp(self) -> "Vec3":
        return Vec3(
            max(0.0, self.r),
            max(0.0, self.g),
            max(0.0, self.b),
        )


def parse_vec3(s: str) -> Vec3:
    parts = [p.strip() for p in s.split(",")]
    if len(parts) != 3:
        raise argparse.ArgumentTypeError("expected r,g,b")
    try:
        return Vec3(float(parts[0]), float(parts[1]), float(parts[2]))
    except ValueError as e:
        raise argparse.ArgumentTypeError(str(e)) from e


def write_pfm_rgb(path: str, w: int, h: int, pixels):
    # PFM (PF) stores rows from bottom to top when scale is negative.
    with open(path, "wb") as f:
        f.write(b"PF\n")
        f.write(f"{w} {h}\n".encode("ascii"))
        f.write(b"-1.0\n")  # little-endian
        for y in range(h - 1, -1, -1):
            for x in range(w):
                r, g, b = pixels(y, x)
                f.write(struct.pack("<fff", float(r), float(g), float(b)))


def make_gradient(w: int, h: int, scale: float):
    def px(y, x):
        r = (x / max(1, w - 1)) * scale
        g = (y / max(1, h - 1)) * scale
        b = 0.4 * scale
        return r, g, b

    return px


def make_point(w: int, h: int, cx: float, cy: float, rgb: Vec3, bg: Vec3):
    ix = int(round(cx * (w - 1)))
    iy = int(round(cy * (h - 1)))

    def px(y, x):
        if x == ix and y == iy:
            return rgb.r, rgb.g, rgb.b
        return bg.r, bg.g, bg.b

    return px


def make_disc(w: int, h: int, cx: float, cy: float, radius_px: float, rgb: Vec3, bg: Vec3, soft: float):
    cxp = cx * (w - 1)
    cyp = cy * (h - 1)
    r2 = max(0.0, radius_px) ** 2
    soft = max(0.0, soft)

    def px(y, x):
        dx = x - cxp
        dy = y - cyp
        d2 = dx * dx + dy * dy
        if soft <= 0.0:
            a = 1.0 if d2 <= r2 else 0.0
        else:
            # Smooth edge over ~soft pixels.
            d = math.sqrt(d2)
            a = 1.0 - min(1.0, max(0.0, (d - radius_px) / soft))
        return (
            bg.r + (rgb.r - bg.r) * a,
            bg.g + (rgb.g - bg.g) * a,
            bg.b + (rgb.b - bg.b) * a,
        )

    return px


def make_stars(w: int, h: int, count: int, seed: int, rgb: Vec3, bg: Vec3, max_amp: float):
    # Deterministic RNG: xorshift32
    def rng32(state: int) -> int:
        x = state & 0xFFFFFFFF
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= (x >> 17) & 0xFFFFFFFF
        x ^= (x << 5) & 0xFFFFFFFF
        return x & 0xFFFFFFFF

    coords = []
    s = seed if seed != 0 else 1
    for _ in range(max(0, count)):
        s = rng32(s)
        x = s % w
        s = rng32(s)
        y = s % h
        s = rng32(s)
        amp = (s / 0xFFFFFFFF) * max_amp
        coords.append((int(x), int(y), float(amp)))

    def px(y, x):
        r, g, b = bg.r, bg.g, bg.b
        for sx, sy, a in coords:
            if x == sx and y == sy:
                r += rgb.r * a
                g += rgb.g * a
                b += rgb.b * a
        return r, g, b

    return px


def main():
    ap = argparse.ArgumentParser(description="Generate simple HDR RGB PFM scenes for Polynomial Optics sanity tests.")
    ap.add_argument("--out", required=True, help="Output .pfm path")
    ap.add_argument("--w", type=int, default=256)
    ap.add_argument("--h", type=int, default=144)
    ap.add_argument("--pattern", choices=["gradient", "point", "disc", "stars"], default="disc")
    ap.add_argument("--scale", type=float, default=0.05, help="Gradient scale (gradient pattern only)")

    ap.add_argument("--cx", type=float, default=0.5, help="Center X in [0,1]")
    ap.add_argument("--cy", type=float, default=0.5, help="Center Y in [0,1]")
    ap.add_argument("--radius_px", type=float, default=2.0, help="Disc radius in pixels (disc pattern only)")
    ap.add_argument("--soft_edge_px", type=float, default=0.0, help="Soft edge width in pixels (disc pattern only)")

    ap.add_argument("--rgb", type=parse_vec3, default=parse_vec3("1,1,1"), help="Foreground RGB (e.g. 20,20,20)")
    ap.add_argument("--bg", type=parse_vec3, default=parse_vec3("0,0,0"), help="Background RGB")

    ap.add_argument("--stars", type=int, default=200, help="Number of stars (stars pattern only)")
    ap.add_argument("--seed", type=int, default=1, help="Deterministic seed (stars pattern only)")
    ap.add_argument("--max_amp", type=float, default=1.0, help="Max star amplitude (stars pattern only)")

    args = ap.parse_args()

    w, h = int(args.w), int(args.h)
    rgb = args.rgb.clamp()
    bg = args.bg.clamp()

    if args.pattern == "gradient":
        pixels = make_gradient(w, h, args.scale)
    elif args.pattern == "point":
        pixels = make_point(w, h, args.cx, args.cy, rgb, bg)
    elif args.pattern == "disc":
        pixels = make_disc(w, h, args.cx, args.cy, args.radius_px, rgb, bg, args.soft_edge_px)
    else:
        pixels = make_stars(w, h, args.stars, args.seed, rgb, bg, args.max_amp)

    write_pfm_rgb(args.out, w, h, pixels)
    print(args.out)


if __name__ == "__main__":
    main()


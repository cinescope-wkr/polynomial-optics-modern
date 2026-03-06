#!/usr/bin/env python3
import argparse
import csv
import json
import os
import subprocess
from pathlib import Path


def run(cmd, quiet: bool):
    if not quiet:
        print(" ".join(cmd))
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL if quiet else None)


def run_capture(cmd):
    return subprocess.check_output(cmd, text=True)


def main():
    ap = argparse.ArgumentParser(
        description="Render a defocus sweep (sensor-plane shift) into an EXR sequence + per-frame stats."
    )
    ap.add_argument("--lens", default="systems/Edmund-Optics-achromat-NT32-921.lens")
    ap.add_argument("--in_pfm", default="", help="Input PFM. If empty, generates a disc PSF scene.")
    ap.add_argument("--outdir", default="OutputPFM/defocus_sweep")
    ap.add_argument("--frames", type=int, default=41)
    ap.add_argument("--d_from", type=float, default=-20.0)
    ap.add_argument("--d_to", type=float, default=20.0)

    ap.add_argument("--z", type=float, default=5000.0)
    ap.add_argument("--p", type=int, default=12)
    ap.add_argument("--s", type=float, default=80.0)
    ap.add_argument("--x", type=float, default=1.0)
    ap.add_argument("--e", type=float, default=19.5)
    ap.add_argument("--b", type=int, default=0)
    ap.add_argument("--a", type=int, default=1)
    ap.add_argument("--c", type=int, default=3)
    ap.add_argument("--quiet", action="store_true")

    # If generating scene
    ap.add_argument("--w", type=int, default=256)
    ap.add_argument("--h", type=int, default=144)
    ap.add_argument("--pattern", choices=["disc", "stars", "point", "gradient"], default="disc")
    ap.add_argument("--radius_px", type=float, default=1.5, help="Disc radius in pixels (disc only)")
    ap.add_argument("--soft_edge_px", type=float, default=1.0, help="Disc soft edge in pixels (disc only)")
    ap.add_argument("--rgb", default="50,50,50", help="Foreground RGB, e.g. 50,50,50")
    ap.add_argument("--bg", default="0,0,0", help="Background RGB, e.g. 0,0,0")
    ap.add_argument("--stars", type=int, default=400, help="Star count (stars only)")
    ap.add_argument("--seed", type=int, default=7, help="Deterministic seed (stars only)")
    ap.add_argument("--max_amp", type=float, default=1.0, help="Max star amplitude (stars only)")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    outdir = root / args.outdir
    outdir.mkdir(parents=True, exist_ok=True)

    run(["make", "-s"], quiet=True)

    if args.in_pfm:
        in_pfm = Path(args.in_pfm)
    else:
        scene_name = f"scene_{args.pattern}.pfm"
        in_pfm = outdir / scene_name
        gen_cmd = [
            "python3",
            str(root / "tools/gen_pfm.py"),
            "--out",
            str(in_pfm),
            "--w",
            str(args.w),
            "--h",
            str(args.h),
            "--pattern",
            args.pattern,
            "--rgb",
            args.rgb,
            "--bg",
            args.bg,
        ]

        if args.pattern in ("disc", "point"):
            gen_cmd += ["--cx", "0.5", "--cy", "0.5"]
        if args.pattern == "disc":
            gen_cmd += ["--radius_px", str(args.radius_px), "--soft_edge_px", str(args.soft_edge_px)]
        if args.pattern == "stars":
            gen_cmd += ["--stars", str(args.stars), "--seed", str(args.seed), "--max_amp", str(args.max_amp)]
        if args.pattern == "gradient":
            # Keep defaults in gen_pfm.py; can be extended later.
            pass

        run(gen_cmd, quiet=True)

    frames = max(2, int(args.frames))
    csv_path = outdir / "stats.csv"
    with open(csv_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "frame",
                "defocus",
                "file",
                "comX",
                "comY",
                "r50",
                "r90",
                "r99",
                "sigmaR",
                "maxY",
                "nonzeroCount",
                "totalCount",
            ]
        )

        for i in range(frames):
            t = i / (frames - 1)
            d = args.d_from * (1.0 - t) + args.d_to * t
            out_exr = outdir / f"frame_{i:04d}_d{d:+07.2f}.exr"

            cmd = [
                str(root / "bin/ex1-postprocess"),
                str(in_pfm),
                "-i",
                str(root / args.lens),
                "-z",
                str(args.z),
                "-p",
                str(args.p),
                "-s",
                str(args.s),
                "-x",
                str(args.x),
                "-e",
                str(args.e),
                "-d",
                str(d),
                "-b",
                str(args.b),
                "-a",
                str(args.a),
                "-c",
                str(args.c),
                "-o",
                str(out_exr),
            ]

            if not args.quiet:
                print(f"[{i+1}/{frames}] defocus={d:+.2f} -> {out_exr.name}")
            run(cmd, quiet=True)

            stats_json = run_capture([str(root / "bin/exr-sanity"), str(out_exr), "--json"])
            stats = json.loads(stats_json)
            w.writerow(
                [
                    i,
                    d,
                    out_exr.name,
                    stats["comX"],
                    stats["comY"],
                    stats["r50"],
                    stats["r90"],
                    stats["r99"],
                    stats["sigmaR"],
                    stats["maxY"],
                    stats["nonzeroCount"],
                    stats["totalCount"],
                ]
            )

    if not args.quiet:
        print(f"[done] {outdir}")
        print(f"[done] stats: {csv_path}")
        print("[note] Open the EXR sequence in your local viewer (DJV/tev/Nuke).")


if __name__ == "__main__":
    main()

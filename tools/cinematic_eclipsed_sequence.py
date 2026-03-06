#!/usr/bin/env python3
import argparse
import shutil
import subprocess
from pathlib import Path


def run(cmd, quiet: bool):
    if not quiet:
        print(" ".join(str(c) for c in cmd))
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL if quiet else None)


def main():
    ap = argparse.ArgumentParser(
        description="Render a cinematic eclipsed-bokeh sequence using ex2-eclipsed-bokeh (occluder motion + focus pull)."
    )
    ap.add_argument("--outdir", default="OutputEXR/cinematic_eclipsed_bokeh")
    ap.add_argument("--preset", choices=["preview", "cinematic"], default="preview")
    ap.add_argument("--case", choices=["A", "B", "C"], default="B")
    ap.add_argument("--frames", type=int, default=25)
    ap.add_argument("--fps", type=int, default=24)
    ap.add_argument("--quiet", action="store_true")

    ap.add_argument("--xres", type=int, default=1280)
    ap.add_argument("--yres", type=int, default=720)
    ap.add_argument("--sensor_width", type=float, default=80.0)

    ap.add_argument("--samples", type=int, default=500000)
    ap.add_argument(
        "--samples_per_light",
        type=int,
        default=0,
        help="If >0, overrides total samples and uses this many aperture samples per light (-G).",
    )
    ap.add_argument("--degree", type=int, default=3)
    ap.add_argument("--aperture_radius", type=float, default=19.5)

    # Start slightly defocused so the bokeh effect is visible from frame 0.
    ap.add_argument("--defocus_from", type=float, default=5.0)
    ap.add_argument("--defocus_to", type=float, default=35.0)
    ap.add_argument("--occ_y_from", type=float, default=60.0)
    ap.add_argument("--occ_y_to", type=float, default=-60.0)
    ap.add_argument("--occ_radius", type=float, default=15.0)

    # Star-like background: fewer sources, wider spread.
    ap.add_argument("--lights", type=int, default=40)
    ap.add_argument("--spread", type=float, default=700.0)
    ap.add_argument("--light_seed", type=int, default=1)
    ap.add_argument("--min_sep", type=float, default=120.0, help="Minimum separation between lights (mm).")
    ap.add_argument("--target_radius", type=float, default=0.0)

    ap.add_argument("--lambdas", type=int, default=5)
    ap.add_argument("--lambda_from", type=float, default=440.0)
    ap.add_argument("--lambda_to", type=float, default=660.0)
    ap.add_argument("--lambda_focus_ref", type=float, default=550.0)

    # Lower gain to avoid blowing out highlights; raise if your viewer shows black.
    ap.add_argument("--gain", type=float, default=3000.0)
    ap.add_argument("--png", action="store_true", help="Also write tonemapped PNGs.")
    ap.add_argument("--png_exposure", type=float, default=0.75, help="Only used with --png.")
    ap.add_argument("--baseline", action="store_true", help="Also write open/diff diagnostic frames.")
    ap.add_argument("--occ_outline", action="store_true", help="Overlay dashed red occluder outline (visual aid).")
    ap.add_argument(
        "--occ_outline_rel",
        type=float,
        default=0.06,
        help="Outline value = rel * frame max (only used with --occ_outline).",
    )
    ap.add_argument("--light_depth_labels", action="store_true", help="Overlay light depth labels at bokeh centers.")
    ap.add_argument(
        "--light_depth_labels_rel",
        type=float,
        default=0.06,
        help="Label value = rel * frame max (only used with --light_depth_labels).",
    )
    ap.add_argument("--lens", default="")
    ap.add_argument("--mp4", action="store_true", help="If ffmpeg is available, encode MP4 from PNGs.")
    args = ap.parse_args()

    root = Path(__file__).resolve().parent.parent
    outdir = root / args.outdir
    outdir.mkdir(parents=True, exist_ok=True)

    if args.preset == "cinematic":
        # Higher-quality defaults; still overrideable via CLI args.
        args.xres = max(args.xres, 1920)
        args.yres = max(args.yres, 1080)
        args.samples = max(args.samples, 120000)
        args.lights = max(args.lights, 80)
        args.lambdas = max(args.lambdas, 5)
        args.frames = max(args.frames, 100)
        args.gain = max(args.gain, 8000.0)

    run(["make", "-s", "bin/ex2-eclipsed-bokeh"], quiet=True)

    out_prefix = outdir / "frame"
    cmd = [
        str(root / "bin/ex2-eclipsed-bokeh"),
        "-t",
        args.case,
        "-n",
        str(args.frames),
        "-m",
        "0",
        "-c",
        str(args.degree),
        "-S",
        str(args.samples),
        "-G",
        str(args.samples_per_light),
        "-x",
        str(args.xres),
        "-y",
        str(args.yres),
        "-w",
        str(args.sensor_width),
        "-P",
        str(args.aperture_radius),
        "-T",
        str(args.target_radius),
        "-d",
        str(args.defocus_from),
        "-D",
        str(args.defocus_to),
        "-A",
        str(args.occ_y_from),
        "-B",
        str(args.occ_y_to),
        "-R",
        str(args.occ_radius),
        "-Q",
        str(args.lights),
        "-U",
        str(args.spread),
        "-V",
        str(args.light_seed),
        "-J",
        str(args.min_sep),
        "-W",
        str(args.lambdas),
        "-a",
        str(args.lambda_from),
        "-b",
        str(args.lambda_to),
        "-C",
        str(args.lambda_focus_ref),
        "-g",
        str(args.gain),
        "-o",
        str(out_prefix),
    ]
    if args.baseline:
        cmd += ["-u"]
    if args.png:
        cmd += ["-p", "-q", str(args.png_exposure)]
    if args.lens:
        cmd += ["-i", str(args.lens)]
    if args.occ_outline:
        cmd += ["-O", str(args.occ_outline_rel)]
    if args.light_depth_labels:
        cmd += ["-H", str(args.light_depth_labels_rel)]

    run(cmd, quiet=args.quiet)

    if args.mp4 and shutil.which("ffmpeg"):
        mp4_path = outdir / "sequence.mp4"
        run(
            [
                "ffmpeg",
                "-y",
                "-framerate",
                str(args.fps),
                "-i",
                str(outdir / "frame_%04d.png"),
                "-c:v",
                "libx264",
                "-pix_fmt",
                "yuv420p",
                str(mp4_path),
            ],
            quiet=args.quiet,
        )
        if not args.quiet:
            print(f"[done] mp4: {mp4_path}")

    if not args.quiet:
        print(f"[done] frames: {outdir}")
        if args.baseline:
            print("[note] For physical occlusion, view *_diff_####.* (open - occluded).")
        else:
            print("[note] This run saves one EXR per frame (plus PNG only if --png).")


if __name__ == "__main__":
    main()

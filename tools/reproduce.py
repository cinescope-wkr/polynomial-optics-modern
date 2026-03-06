#!/usr/bin/env python3
import argparse
import subprocess
from pathlib import Path
import sys

# --- 설정 (Configuration) ---
# 논문 퀄리티 재현을 위한 기본값들입니다.

# [품질 설정]
# Wavelengths: 1이면 단색광, 20 이상이면 고품질 스펙트럼 (색수차 구현)
DEFAULT_WAVELENGTHS = 31  # 440nm ~ 660nm 사이를 31단계로 촘촘하게 계산
DEFAULT_SAMPLES = 2000000 # 2백만 샘플 (노이즈 제거를 위한 고화질)

# [차폐물 시각화 옵션 (Occluder Visualization)]
# 0: 논문 Fig 1과 동일 (검은 배경, 보케가 잘려나감)
# 1: 차폐물 자체를 희미한 보케로 표시 (위치 확인용, 교육용)
# 2: 차폐물 보케를 별도 파일로 저장
DEFAULT_OCCLUDER_MODE = 0 

def run_command(cmd, quiet=False):
    """명령어를 실행하고 진행 상황을 출력합니다."""
    cmd_str = " ".join(str(c) for c in cmd)
    print(f"\n[Running] {cmd_str}")
    try:
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL if quiet else None)
    except subprocess.CalledProcessError as e:
        print(f"Error executing binary: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="Reproduce Figure 1 of the Eclipsed Bokeh paper with High Fidelity.")
    
    parser.add_argument("--outdir", default="Output_Fig1_HighQuality", help="Output directory")
    parser.add_argument("--cases", nargs="+", default=["A", "B", "C"], help="Cases to render (A, B, C)")
    parser.add_argument("--frames", type=int, default=5, help="Number of frames per case (Fig 1 uses about 5 steps)")
    
    # 품질 관련 옵션
    parser.add_argument("--samples", type=int, default=DEFAULT_SAMPLES, help="Ray samples per pixel")
    parser.add_argument("--wavelengths", type=int, default=DEFAULT_WAVELENGTHS, help="Number of spectral wavelengths")
    parser.add_argument("--vis_mode", type=int, default=DEFAULT_OCCLUDER_MODE, help="0: Physics-based (Invisible), 1: Overlay visual aid")
    
    # 해상도 (논문 그림 비율 유지)
    parser.add_argument("--xres", type=int, default=4096)
    parser.add_argument("--yres", type=int, default=3072)
    
    # 바이너리 경로
    parser.add_argument("--bin", default="bin/ex2-eclipsed-bokeh", help="Path to the compiled C++ binary")

    args = parser.parse_args()

    # 경로 설정
    root = Path(__file__).resolve().parent
    bin_path = root / args.bin
    out_root = root / args.outdir
    out_root.mkdir(parents=True, exist_ok=True)

    if not bin_path.exists():
        # 혹시 bin 폴더가 상위에 있을 경우를 대비
        if (root / "../bin/ex2-eclipsed-bokeh").exists():
            bin_path = root / "../bin/ex2-eclipsed-bokeh"   
        else:
            print(f"Error: Binary not found at {bin_path}. Please compile first.")
            return

    # 공통 파라미터 (논문 Figure 1 재현용 고정값)
    # 논문의 Achromatic lens는 코드 내에 하드코딩 되어 있어 별도 파일 불필요
    common_args = [
        str(bin_path),
        "-Q", "1",           # 단일 광원 (Single point light)
        "-U", "0.01",           # 광원 펴짐 (Almost Perfect point source)
        "-n", str(args.frames), # 프레임 수
        "-S", str(args.samples),
        "-W", str(args.wavelengths),
        "-a", "440",         # 파장 범위 시작 (Blue)
        "-b", "660",         # 파장 범위 끝 (Red)
        "-m", str(args.vis_mode),
        "-x", str(args.xres),
        "-y", str(args.yres),
        "-g", "20000",       # Gain (샘플이 많아지면 밝기 확보 필요, 필요시 조절)
        "-p"                # PNG 프리뷰도 함께 생성 (EXR은 기본 생성)
    ]

    print(f"=== Starting High-Quality Reproduction (Samples: {args.samples}, Wavelengths: {args.wavelengths}) ===")
    
    for case in args.cases:
        case = case.upper()
        if case not in ["A", "B", "C"]:
            continue
            
        print(f"\n>> Rendering Case {case}...")
        
        # Case별 폴더 생성
        case_dir = out_root / f"Case_{case}"
        case_dir.mkdir(exist_ok=True)
        out_prefix = case_dir / "frame"

        # Case별 명령어 실행
        # -t 옵션이 논문의 거리(Focus, Occluder, Light) 설정을 자동으로 불러옵니다.
        cmd = common_args + [
            "-t", case,
            "-o", str(out_prefix)
        ]
        
        run_command(cmd)
        print(f">> Case {case} finished. Output saved to {case_dir}")

    print("\n=== All tasks completed. ===")
    print(f"Check the results in: {out_root}")
    print("Note: .exr files contain high-dynamic range raw data.")

if __name__ == "__main__":
    main()
#!/usr/bin/env python3
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pdj_analyzer.pdjv import validate_package
from pdj_analyzer.pipeline import analyze_to_package


def main() -> None:
    parser = argparse.ArgumentParser(description="Offline PDJV analysis spike")
    parser.add_argument("--clip", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--max-frames", type=int, default=90)
    parser.add_argument("--backend", choices=("synthetic", "real"), default="synthetic")
    parser.add_argument("--real", action="store_true", help="Alias for --backend real")
    args = parser.parse_args()
    backend = "real" if args.real else args.backend
    if backend == "real":
        from pdj_analyzer.real_pipeline import analyze_real_clip
        dest = analyze_real_clip(Path(args.clip), Path(args.out), max_frames=args.max_frames)
    else:
        dest = analyze_to_package(Path(args.clip), Path(args.out), max_frames=args.max_frames)
    errors = validate_package(dest)
    if errors:
        print("INVALID")
        for item in errors:
            print(f"- {item}")
        sys.exit(1)
    print(f"VALID {dest}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pdj_analyzer.pdjv import validate_package


def main() -> None:
    if len(sys.argv) < 2:
        print("usage: validate_package.py <package.pdjv>")
        sys.exit(2)
    errors = validate_package(Path(sys.argv[1]))
    if errors:
        print("INVALID")
        for item in errors:
            print(f"- {item}")
        sys.exit(1)
    print("VALID")


if __name__ == "__main__":
    main()

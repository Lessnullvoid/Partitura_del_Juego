#!/usr/bin/env python3
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from pdj_analyzer.pdjv import export_package
from pdj_analyzer.pipeline import synthesize_package


def main() -> None:
    for name, version, pkg_id in (("golden_v0.pdjv", 0, "golden-v0"), ("golden_v1.pdjv", 1, "golden-v1")):
        dest = ROOT / "docs" / "pdjv" / "fixtures" / name
        package = synthesize_package(
            source_id="synthetic_golden",
            width=1920,
            height=1080,
            fps=30.0,
            frame_count=48,
            package_id=pkg_id,
            occlude_from=24,
        )
        package.manifest["version"] = version
        export_package(package, dest)
        print(dest)


if __name__ == "__main__":
    main()

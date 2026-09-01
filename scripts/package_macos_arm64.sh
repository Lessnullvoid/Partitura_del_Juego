#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="Partitura_del_Juego-macOS-arm64"
SOURCE_APP="$ROOT/bin/Partitura_del_Juego.app"
DIST_DIR="$ROOT/dist"
STAGE_DIR="$DIST_DIR/$NAME"
APP="$STAGE_DIR/Partitura_del_Juego.app"
DATA="$APP/Contents/Resources/data"
ZIP="$DIST_DIR/$NAME.zip"
CHECKSUM="$ZIP.sha256"

fail() {
    echo "error: $*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

require_command make
require_command ditto
require_command codesign
require_command otool
require_command plutil
require_command file
require_command python3
require_command shasum
require_command xattr

test -d "$ROOT/cortos" || fail "video folder not found: $ROOT/cortos"
test -f "$ROOT/bin/data/settings.json" || fail "settings not found"
test -d "$ROOT/bin/data/shaders" || fail "shaders not found"
test -f "$ROOT/distribution/README-macOS-test.md" || fail "distribution README not found"

echo "Building release app..."
make -C "$ROOT"
test -d "$SOURCE_APP" || fail "build did not produce $SOURCE_APP"

echo "Staging portable bundle..."
rm -rf "$STAGE_DIR" "$ZIP" "$CHECKSUM"
mkdir -p "$STAGE_DIR" "$DATA" "$STAGE_DIR/SuperCollider"
ditto "$SOURCE_APP" "$APP"

# Runtime assets belong in the openFrameworks app data directory. Copy the
# actual media files, never the absolute development symlinks in bin/data.
rm -rf "$DATA"
mkdir -p "$DATA"
ditto "$ROOT/bin/data/shaders" "$DATA/shaders"
ditto "$ROOT/cortos" "$DATA/cortos"
cp "$ROOT/bin/data/settings.json" "$DATA/settings.json"
plutil -replace clips.folder -string "cortos" "$DATA/settings.json"
plutil -replace outputMode -string "singleWindow" "$DATA/settings.json"
plutil -replace singleWindow.x -integer 0 "$DATA/settings.json"
plutil -replace singleWindow.y -integer 40 "$DATA/settings.json"
plutil -replace singleWindow.width -integer 1920 "$DATA/settings.json"
plutil -replace singleWindow.height -integer 1080 "$DATA/settings.json"

cp "$ROOT/supercollider/pdj_datamatics.scd" "$STAGE_DIR/SuperCollider/"
cp "$ROOT/supercollider/pdj_mode_voices.scd" "$STAGE_DIR/SuperCollider/"
cp "$ROOT/distribution/README-macOS-test.md" "$STAGE_DIR/README.md"

# Never ship mutable development state or inherited quarantine metadata.
rm -f "$APP/Contents/Resources/imgui.ini"
xattr -cr "$STAGE_DIR"

echo "Applying ad-hoc signature..."
if test -d "$APP/Contents/Frameworks/Syphon.framework"; then
    codesign --force --sign - --timestamp=none \
        "$APP/Contents/Frameworks/Syphon.framework"
fi
codesign --force --sign - --timestamp=none \
    "$APP/Contents/Frameworks/libfmod.dylib"
codesign --force --deep --sign - --timestamp=none "$APP"

validate_package() {
    local package_root="$1"
    local package_app="$package_root/Partitura_del_Juego.app"
    local executable="$package_app/Contents/MacOS/Partitura_del_Juego"
    local package_data="$package_app/Contents/Resources/data"

    test -x "$executable" || fail "missing executable in $package_root"
    file "$executable" | grep -q "arm64" || fail "executable is not arm64"
    file "$package_app/Contents/Frameworks/libfmod.dylib" \
        | grep -q "arm64" || fail "libfmod.dylib has no arm64 slice"

    while IFS= read -r dependency; do
        case "$dependency" in
            /System/*|/usr/lib/*|@executable_path/../Frameworks/libfmod.dylib)
                ;;
            *)
                fail "unresolved or non-portable dependency: $dependency"
                ;;
        esac
    done < <(otool -L "$executable" | awk 'NR > 1 { print $1 }')

    test -f "$package_data/settings.json" || fail "settings.json is missing"
    test -d "$package_data/shaders" || fail "shader folder is missing"
    plutil -extract performanceTest.durationSeconds raw \
        "$package_data/settings.json" >/dev/null \
        || fail "performance test configuration is missing"
    test -f "$package_root/SuperCollider/pdj_datamatics.scd" \
        || fail "SuperCollider engine is missing"
    plutil -lint "$package_app/Contents/Info.plist" >/dev/null

    PACKAGE_ROOT="$package_root" SOURCE_MEDIA="$ROOT/cortos" python3 <<'PY'
import os
from pathlib import Path

package = Path(os.environ["PACKAGE_ROOT"])
source_media = Path(os.environ["SOURCE_MEDIA"])
bundled_media = package / "Partitura_del_Juego.app/Contents/Resources/data/cortos"
extensions = {".mp4", ".mov", ".avi"}

source_count = sum(
    1 for path in source_media.iterdir()
    if path.is_file() and path.suffix.lower() in extensions
)
bundled_count = sum(
    1 for path in bundled_media.iterdir()
    if path.is_file() and path.suffix.lower() in extensions
)
if source_count == 0 or bundled_count != source_count:
    raise SystemExit(
        f"video count mismatch: source={source_count}, bundled={bundled_count}"
    )

broken = []
for root, dirs, files in os.walk(package):
    for name in dirs + files:
        path = Path(root) / name
        if path.is_symlink() and not path.exists():
            broken.append(str(path))
if broken:
    raise SystemExit("broken symlinks:\n" + "\n".join(broken))
PY

    codesign --verify --deep --strict --verbose=2 "$package_app"
}

echo "Validating staged package..."
validate_package "$STAGE_DIR"

echo "Creating ZIP..."
ditto -c -k --sequesterRsrc --keepParent "$STAGE_DIR" "$ZIP"
(
    cd "$DIST_DIR"
    shasum -a 256 "$(basename "$ZIP")" > "$(basename "$CHECKSUM")"
)

echo "Extracting ZIP for a second validation pass..."
VERIFY_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pdj-package.XXXXXX")"
trap 'rm -rf "$VERIFY_DIR"' EXIT
ditto -x -k "$ZIP" "$VERIFY_DIR"
validate_package "$VERIFY_DIR/$NAME"

echo
echo "Portable package created:"
echo "  $ZIP"
echo "  $CHECKSUM"

#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="Partitura_del_Juego-macOS-arm64"
SOURCE_APP="$ROOT/bin/Partitura_del_Juego.app"
DIST_DIR="$ROOT/dist"
STAGE_DIR="$DIST_DIR/$NAME"
APP="$STAGE_DIR/Partitura_del_Juego.app"
DATA="$APP/Contents/Resources/data"
VIDEOS="$STAGE_DIR/Videos"
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
test -d "$ROOT/bin/data/horizontal" \
    || fail "horizontal video folder not found: $ROOT/bin/data/horizontal"
test -f "$ROOT/bin/data/settings.json" || fail "settings not found"
test -d "$ROOT/bin/data/shaders" || fail "shaders not found"
test -f "$ROOT/distribution/README-macOS-test.md" || fail "distribution README not found"

echo "Building release app..."
make -C "$ROOT"
test -d "$SOURCE_APP" || fail "build did not produce $SOURCE_APP"

echo "Staging portable bundle..."
rm -rf "$STAGE_DIR" "$ZIP" "$CHECKSUM"
mkdir -p "$STAGE_DIR" "$DATA" "$VIDEOS" "$STAGE_DIR/SuperCollider"
ditto "$SOURCE_APP" "$APP"

# Mantener los medios mutables fuera de la aplicación firmada. Las rutas relativas de
# settings.json siguen funcionando cuando se mueve la carpeta completa del paquete.
rm -rf "$DATA"
mkdir -p "$DATA"
ditto "$ROOT/bin/data/shaders" "$DATA/shaders"
ditto "$ROOT/cortos" "$VIDEOS/Portrait"
ditto "$ROOT/bin/data/horizontal" "$VIDEOS/Horizontal"
cp "$ROOT/bin/data/settings.json" "$DATA/settings.json"
plutil -replace clips.folder -string "../../../../Videos/Portrait" \
    "$DATA/settings.json"
plutil -replace clips.horizontalFolder \
    -string "../../../../Videos/Horizontal" "$DATA/settings.json"
plutil -replace outputMode -string "singleWindow" "$DATA/settings.json"
plutil -replace presentationFullscreen -bool NO "$DATA/settings.json"
plutil -replace singleWindow.x -integer 0 "$DATA/settings.json"
plutil -replace singleWindow.y -integer 40 "$DATA/settings.json"
plutil -replace singleWindow.width -integer 1920 "$DATA/settings.json"
plutil -replace singleWindow.height -integer 1080 "$DATA/settings.json"
if test -d "$ROOT/volumetric/bin/data/real_001.pdjv"; then
    mkdir -p "$DATA/pdjv"
    ditto "$ROOT/volumetric/bin/data/real_001.pdjv" \
        "$DATA/pdjv/real_001.pdjv"
fi

cp "$ROOT/supercollider/pdj_audio_config.scd"  "$STAGE_DIR/SuperCollider/"
cp "$ROOT/supercollider/pdj_launcher.scd"      "$STAGE_DIR/SuperCollider/"
cp "$ROOT/supercollider/pdj_datamatics.scd"    "$STAGE_DIR/SuperCollider/"
cp "$ROOT/supercollider/pdj_mode_voices.scd"   "$STAGE_DIR/SuperCollider/"
if test -f "$ROOT/supercollider/pdj_volumetric_compat.scd"; then
    cp "$ROOT/supercollider/pdj_volumetric_compat.scd" \
        "$STAGE_DIR/SuperCollider/"
fi
cp "$ROOT/distribution/README-macOS-test.md" "$STAGE_DIR/README.md"
cat > "$STAGE_DIR/Start Audio.command" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

PACKAGE_DIR="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER="$PACKAGE_DIR/SuperCollider/pdj_launcher.scd"
SCLANG=""

for candidate in \
    "/Applications/SuperCollider.app/Contents/MacOS/sclang" \
    "$HOME/Applications/SuperCollider.app/Contents/MacOS/sclang"
do
    if test -x "$candidate"; then
        SCLANG="$candidate"
        break
    fi
done

if test -z "$SCLANG"; then
    echo ""
    echo "ERROR: SuperCollider was not found in /Applications or ~/Applications."
    echo "Install SuperCollider from https://supercollider.github.io, then run"
    echo "this file again."
    read -r -p "Press Return to close."
    exit 1
fi

# --------------------------------------------------------------------------
# Pre-flight: scan audio devices and report DANTE status before launching SC
# --------------------------------------------------------------------------
echo ""
echo "============================================================"
echo "  Partitura del Juego  |  Audio Launcher"
echo "============================================================"
echo ""
echo "Scanning system audio devices..."
if system_profiler SPAudioDataType 2>/dev/null \
        | grep -qi "dante\|virtual soundcard"; then
    DANTE_STATUS="YES — Dante Virtual Soundcard found in system audio"
else
    DANTE_STATUS="NO  — Dante Virtual Soundcard not found (stereo fallback)"
fi
echo "  DANTE device : $DANTE_STATUS"

# Show raw device list from system_profiler (device names only, trimmed)
echo "  Devices listed by macOS:"
system_profiler SPAudioDataType 2>/dev/null \
    | awk -F': ' '/^\s+[A-Z].*:$/ { gsub(/^[ \t]+|[ \t]+$/, "", $1); print "    " $1 }' \
    | grep -iv "Audio Devices\|coreaudio\|^$" \
    | head -12 || echo "    (could not read device list)"
echo ""

# Check Dante Virtual Soundcard app installation
if test -d "/Applications/Dante Virtual Soundcard.app" \
        || test -d "$HOME/Applications/Dante Virtual Soundcard.app"; then
    echo "  DVS app      : installed"
else
    echo "  DVS app      : NOT found in Applications"
    echo "                 Download from https://www.audinate.com/products/software/dante-virtual-soundcard"
fi
echo ""
echo "Starting the audio engine. Leave this window open."
echo "Press Ctrl+C here to stop audio."
echo ""

exec "$SCLANG" "$LAUNCHER"
SH
chmod +x "$STAGE_DIR/Start Audio.command"

# --------------------------------------------------------------------------
# Check Audio.command — standalone scanner, no engine, double-clickable
# --------------------------------------------------------------------------
cat > "$STAGE_DIR/Check Audio.command" <<'SH'
#!/usr/bin/env bash
set -euo pipefail

echo ""
echo "============================================================"
echo "  Partitura del Juego  |  Audio Status Check"
echo "============================================================"
echo ""

# macOS audio device list via system_profiler
echo "[ System audio devices ]"
system_profiler SPAudioDataType 2>/dev/null \
    | awk -F': ' '/^\s+[A-Z].*:$/ { gsub(/^[ \t]+|[ \t]+$/, "", $1); print "  " $1 }' \
    | grep -iv "Audio Devices\|coreaudio\|^$" \
    | head -20 || echo "  (could not read device list)"
echo ""

# DANTE / DVS detection
echo "[ DANTE detection ]"
if system_profiler SPAudioDataType 2>/dev/null \
        | grep -qi "dante\|virtual soundcard"; then
    echo "  DANTE Virtual Soundcard : DETECTED"
    echo "  Mode when SC starts     : 8 channels / 48 kHz"
else
    echo "  DANTE Virtual Soundcard : NOT FOUND"
    echo "  Mode when SC starts     : stereo fallback (2 ch)"
    echo ""
    echo "  To enable DANTE:"
    echo "  1. Install Dante Virtual Soundcard from audinate.com (license required)"
    echo "  2. Open the DVS menu-bar app, set TX=8 RX=2 48kHz, click Enable"
    echo "  3. Connect the Mac to the same Ethernet switch as the DANTE 5 unit"
    echo "  4. In Dante Controller, route DVS Out 1-8 to D3-1 through D3-8"
fi
echo ""

# DVS app installation
echo "[ DVS app ]"
if test -d "/Applications/Dante Virtual Soundcard.app" \
        || test -d "$HOME/Applications/Dante Virtual Soundcard.app"; then
    echo "  Installed : YES"
else
    echo "  Installed : NO — download from https://www.audinate.com"
fi
echo ""

# Dante Controller app
echo "[ Dante Controller app ]"
if test -d "/Applications/Dante Controller.app" \
        || test -d "$HOME/Applications/Dante Controller.app"; then
    echo "  Installed : YES"
else
    echo "  Installed : NO — download free from https://www.audinate.com"
fi
echo ""

# Network check: look for an Ethernet interface with an IP
echo "[ Network ]"
if networksetup -listallhardwareports 2>/dev/null \
        | grep -A2 "Ethernet\|Thunderbolt" \
        | grep -q "en[0-9]"; then
    # Try to find the IP of the first wired Ethernet interface
    ETH_IF=$(networksetup -listallhardwareports 2>/dev/null \
        | awk '/Ethernet|Thunderbolt/{getline; print $2; exit}')
    if test -n "$ETH_IF"; then
        ETH_IP=$(ipconfig getifaddr "$ETH_IF" 2>/dev/null || echo "")
        if test -n "$ETH_IP"; then
            echo "  Ethernet ($ETH_IF) : $ETH_IP"
        else
            echo "  Ethernet ($ETH_IF) : no IP address — cable connected?"
        fi
    fi
else
    echo "  No Ethernet interface found — DANTE requires a wired connection"
fi
echo ""

echo "============================================================"
echo "Run 'Start Audio.command' to start the audio engine."
echo "============================================================"
echo ""
read -r -p "Press Return to close."
SH
chmod +x "$STAGE_DIR/Check Audio.command"

# Nunca empaquetar estado de desarrollo mutable ni metadatos de cuarentena heredados.
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
    test -f "$package_root/SuperCollider/pdj_audio_config.scd" \
        || fail "SuperCollider audio config is missing (pdj_audio_config.scd)"
    test -f "$package_root/SuperCollider/pdj_launcher.scd" \
        || fail "SuperCollider launcher script is missing (pdj_launcher.scd)"
    test -f "$package_root/SuperCollider/pdj_datamatics.scd" \
        || fail "SuperCollider engine is missing"
    test -f "$package_root/SuperCollider/pdj_mode_voices.scd" \
        || fail "SuperCollider mode voices are missing"
    test -x "$package_root/Start Audio.command" \
        || fail "Start Audio.command is missing or not executable"
    test -x "$package_root/Check Audio.command" \
        || fail "Check Audio.command is missing or not executable"
    plutil -lint "$package_app/Contents/Info.plist" >/dev/null

    PACKAGE_ROOT="$package_root" \
    SOURCE_PORTRAIT="$ROOT/cortos" \
    SOURCE_HORIZONTAL="$ROOT/bin/data/horizontal" \
    python3 <<'PY'
import os
from pathlib import Path

package = Path(os.environ["PACKAGE_ROOT"])
sources = {
    "Portrait": Path(os.environ["SOURCE_PORTRAIT"]),
    "Horizontal": Path(os.environ["SOURCE_HORIZONTAL"]),
}
extensions = {".mp4", ".mov", ".avi"}

for name, source_media in sources.items():
    bundled_media = package / "Videos" / name
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
            f"{name} video count mismatch: "
            f"source={source_count}, bundled={bundled_count}"
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

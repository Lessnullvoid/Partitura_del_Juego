#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-${PDJ_VERSION:-$(date -u '+%Y.%m.%d-%H%M')}}"
if ! [[ "$VERSION" =~ ^[0-9A-Za-z][0-9A-Za-z._-]*$ ]]; then
    echo "error: invalid version '$VERSION'" >&2
    echo "use only letters, numbers, dots, underscores, and hyphens" >&2
    exit 1
fi
NAME="Partitura_del_Juego-v${VERSION}-macOS-arm64"
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
{
    echo "version=$VERSION"
    echo "built_utc=$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "git_commit=$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
} > "$STAGE_DIR/VERSION.txt"

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

echo ""
echo "============================================================"
echo "  Partitura del Juego  |  Audio Launcher"
echo "============================================================"
echo ""

# --------------------------------------------------------------------------
# Step 1: scan and classify audio devices
# Outputs shell variables: SCAN_MODE, SCAN_DANTE, SCAN_REAL_LIST, SCAN_BUILTIN
# --------------------------------------------------------------------------
echo "[ Scanning audio devices ]"

eval "$(python3 - <<'PY'
import subprocess, json, sys

BUILTIN_KW  = {"built-in","built in","macbook","imac","mac mini","mac pro",
               "mac studio","microphone","aggregate","multi-output","blackhole"}
DANTE_KW    = {"dante","dvs","virtual soundcard"}
VIRTUAL_KW  = {"ndi","iriun","zoom","webcam","loopback","soundflower",
               "teams","discord","airplay"}

def classify(name):
    lo = name.lower()
    if any(k in lo for k in DANTE_KW):    return "dante"
    if any(k in lo for k in BUILTIN_KW):  return "builtin"
    if any(k in lo for k in VIRTUAL_KW):  return "virtual"
    return "real"

try:
    out = subprocess.run(["system_profiler","SPAudioDataType","-json"],
                         capture_output=True, text=True, timeout=8).stdout
    items = []
    for section in json.loads(out).get("SPAudioDataType",[]):
        items.extend(section.get("_items",[]))
except Exception:
    try:
        out = subprocess.run(["system_profiler","SPAudioDataType"],
                             capture_output=True, text=True, timeout=8).stdout
        items = [{"_name": l.strip().rstrip(":")}
                 for l in out.splitlines()
                 if l.startswith("        ") and l.rstrip().endswith(":")]
    except Exception:
        print("SCAN_MODE=builtin"); print("SCAN_DANTE="); print("SCAN_REAL_LIST="); print("SCAN_BUILTIN=")
        sys.exit(0)

for d in items:
    n = d.get("_name","?")
    t = classify(n)
    if   t == "dante":   lab = "[DANTE]   "
    elif t == "builtin": lab = "[built-in]"
    elif t == "virtual": lab = "[virtual] "
    else:                lab = "[real]    "
    sys.stderr.write("  " + lab + "  " + n + "\n")

dante   = next((d["_name"] for d in items if classify(d["_name"])=="dante"),  None)
reals   = [d["_name"] for d in items if classify(d["_name"])=="real"]
builtin = next((d["_name"] for d in items if classify(d["_name"])=="builtin" and "speaker" in d["_name"].lower()), None)
if builtin is None:
    builtin = next((d["_name"] for d in items if classify(d["_name"])=="builtin"), None)

mode = "dante" if dante else ("real" if reals else "builtin")

def sq(s): return s.replace("'", "'\\''") if s else ""

real_list = "|".join(sq(r) for r in reals)

sys.stderr.write("\n")
print(f"SCAN_MODE='{mode}'")
print(f"SCAN_DANTE='{sq(dante or '')}'")
print(f"SCAN_REAL_LIST='{real_list}'")
print(f"SCAN_BUILTIN='{sq(builtin or '')}'")
PY
)"

echo ""

# --------------------------------------------------------------------------
# Step 2: device selection
# DANTE -> automatic.  Otherwise -> interactive picker.
# Result exported as PDJ_AUDIO_DEVICE (empty = built-in default)
# --------------------------------------------------------------------------
export PDJ_AUDIO_DEVICE=""
export PDJ_AUDIO_CHANNELS=2

if [ "$SCAN_MODE" = "dante" ]; then
    echo "  DANTE detected: $SCAN_DANTE"
    echo "  Routing: 8-channel @ 48 kHz -> AmpCrown"
    PDJ_AUDIO_DEVICE="$SCAN_DANTE"
    PDJ_AUDIO_CHANNELS=8
else
    IFS='|' read -ra REAL_DEVS <<< "$SCAN_REAL_LIST"
    MENU_ITEMS=()
    for dev in "${REAL_DEVS[@]}"; do
        [ -n "$dev" ] && MENU_ITEMS+=("$dev")
    done
    [ -n "$SCAN_BUILTIN" ] && MENU_ITEMS+=("${SCAN_BUILTIN} (built-in)")

    if [ ${#MENU_ITEMS[@]} -eq 0 ]; then
        echo "  No external audio device found."
        echo "  Using macOS default output (built-in speakers)."
    else
        echo "  No DANTE detected. Select audio output:"
        echo ""
        DEFAULT_IDX=0
        for i in "${!MENU_ITEMS[@]}"; do
            LABEL="${MENU_ITEMS[$i]}"
            MARKER=""
            [ "$i" -eq "$DEFAULT_IDX" ] && MARKER=" (default)"
            echo "    $((i+1)))  $LABEL$MARKER"
        done
        echo ""
        read -r -p "  Enter number [1-${#MENU_ITEMS[@]}] or Enter for default: " SELECTION
        echo ""

        if [ -z "$SELECTION" ]; then
            SELECTION=1
        fi

        if ! [[ "$SELECTION" =~ ^[0-9]+$ ]] || \
           [ "$SELECTION" -lt 1 ] || \
           [ "$SELECTION" -gt "${#MENU_ITEMS[@]}" ]; then
            echo "  Invalid choice -- using default."
            SELECTION=1
        fi

        CHOSEN="${MENU_ITEMS[$((SELECTION-1))]}"

        if [[ "$CHOSEN" == *" (built-in)" ]]; then
            PDJ_AUDIO_DEVICE=""
        else
            PDJ_AUDIO_DEVICE="$CHOSEN"
        fi

        if [ -z "$PDJ_AUDIO_DEVICE" ]; then
            echo "  Selected: built-in speakers (macOS default output)"
        else
            echo "  Selected: $PDJ_AUDIO_DEVICE"
        fi
    fi
    echo ""
fi

# --------------------------------------------------------------------------
# Step 3: OS-level audio confirmation
# afplay uses whatever macOS has as the system output device. When DVS is
# active that means the ping travels the full DANTE path to the amps, which
# is exactly what we want to verify. A background watchdog kills afplay
# after 6 seconds so it can never hang the launcher.
# --------------------------------------------------------------------------
afplay_safe() {
    afplay "$1" &
    local pid=$!
    ( sleep 6 && kill "$pid" 2>/dev/null ) &
    local dog=$!
    wait "$pid" 2>/dev/null || true
    kill "$dog" 2>/dev/null || true
    wait "$dog" 2>/dev/null || true
}

PING_SOUND=""
for candidate in \
    "/System/Library/Sounds/Ping.aiff" \
    "/System/Library/Sounds/Tink.aiff" \
    "/System/Library/Sounds/Pop.aiff"
do
    if test -f "$candidate"; then
        PING_SOUND="$candidate"
        break
    fi
done

if [ -n "$PING_SOUND" ]; then
    echo "[ OS audio test ]"
    if [ "$SCAN_MODE" = "dante" ]; then
        echo "  Playing a system ping through DANTE -> AmpCrown speakers..."
        echo "  (if DVS is the macOS output device you should hear it on the amps)"
    else
        echo "  Playing a system ping to confirm macOS audio routing..."
    fi
    echo ""
    afplay_safe "$PING_SOUND"
    sleep 0.3
    afplay_safe "$PING_SOUND"
    echo ""
    read -r -p "  Did you hear the ping? [y/n] then Enter: " HEARD
    echo ""
    if [[ "$HEARD" =~ ^[Nn] ]]; then
        if [ "$SCAN_MODE" = "dante" ]; then
            echo "  ** Ping not heard through DANTE. Check:"
            echo "     1. Dante Virtual Soundcard is set as macOS output"
            echo "        (System Settings -> Sound -> Output -> Dante Virtual Soundcard)"
            echo "     2. DVS is enabled (green icon in the menu bar)"
            echo "     3. Dante Controller shows green signal on AmpCrown receive channels"
            echo "     4. AmpCrown amplifiers are powered on and not muted"
        else
            echo "  ** Audio not confirmed. Troubleshooting steps:"
            echo "     1. Open System Settings -> Sound -> Output"
            echo "        and confirm the correct device is selected."
            echo "     2. Make sure the Mac volume is not muted (press F12)."
            echo "     3. If using an external interface, check it is powered"
            echo "        on and set as the output in System Settings."
        fi
        echo ""
        read -r -p "  Press Enter to continue anyway, or Ctrl+C to quit: "
        echo ""
    else
        echo "  Audio confirmed. Continuing..."
        echo ""
    fi
fi

# --------------------------------------------------------------------------
# Step 4: launch SC engine -- PDJ_AUDIO_DEVICE and PDJ_AUDIO_CHANNELS are
# exported so pdj_audio_config.scd can read them with .getenv
# --------------------------------------------------------------------------
echo "[ Starting SuperCollider engine ]"
echo "  Leave this window open.  Press Ctrl+C to stop."
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

python3 - <<'PY'
import subprocess, json, sys

BUILTIN_KEYWORDS = {
    "built-in", "built in", "macbook", "imac", "mac mini",
    "mac pro", "mac studio", "microphone", "aggregate",
    "multi-output", "blackhole"
}
DANTE_KEYWORDS = {"dante", "dvs", "virtual soundcard"}

def classify(name):
    lo = name.lower()
    if any(k in lo for k in DANTE_KEYWORDS):
        return "dante"
    if any(k in lo for k in BUILTIN_KEYWORDS):
        return "builtin"
    return "external"

try:
    out = subprocess.run(
        ["system_profiler", "SPAudioDataType", "-json"],
        capture_output=True, text=True, timeout=8
    ).stdout
    items = []
    for section in json.loads(out).get("SPAudioDataType", []):
        items.extend(section.get("_items", []))
except Exception:
    try:
        out = subprocess.run(
            ["system_profiler", "SPAudioDataType"],
            capture_output=True, text=True, timeout=8
        ).stdout
        items = [
            {"_name": line.strip().rstrip(":")}
            for line in out.splitlines()
            if line.startswith("        ") and line.rstrip().endswith(":")
            and not line.strip().startswith("#")
        ]
    except Exception as e:
        print(f"  [!] Could not read audio devices: {e}")
        items = []

dante    = next((d["_name"] for d in items if classify(d.get("_name","")) == "dante"),    None)
external = next((d["_name"] for d in items if classify(d.get("_name","")) == "external"), None)

print("[ Audio device scan ]")
for d in items:
    name = d.get("_name", "?")
    tag  = classify(name)
    marker = {"dante": "[DANTE]   ", "external": "[external]", "builtin": "[built-in]"}.get(tag, "[?]       ")
    print(f"  {marker}  {name}")

print()
print("[ Audio mode when SuperCollider starts ]")
if dante:
    print(f"  Mode    : 8-channel DANTE")
    print(f"  Device  : {dante}")
    print( "  Action  : none needed — DANTE is ready")
elif external:
    print(f"  Mode    : stereo — external device")
    print(f"  Device  : {external}")
    print( "  Note    : for 8-channel DANTE, install + enable DVS and connect Ethernet")
else:
    print( "  Mode    : stereo — built-in speakers")
    print( "  Device  : none connected  (Mac internal speakers will be used)")
    print()
    print( "  To enable DANTE (8ch / Sala Abierta):")
    print( "    1. Install + license Dante Virtual Soundcard (audinate.com, ~USD 30)")
    print( "    2. Open the DVS menu-bar app")
    print( "       Set TX=8  RX=2  48000 Hz  1ms latency — click Enable")
    print( "    3. Connect Ethernet to the same switch as the DANTE 5 unit")
    print( "    4. Open Dante Controller — route DVS Out 1-8 to D3-1 through D3-8")
    print( "       Set DANTE 5 as Clock Master — confirm green lock icon")
    print( "    5. Run this check again to confirm, then Start Audio.command")
    print()
    print( "  To use a stereo USB/Thunderbolt interface instead:")
    print( "    Connect the interface before launching Start Audio.command.")
    print( "    The engine selects any non-built-in device automatically.")
PY

echo ""

# DVS app
echo "[ Dante Virtual Soundcard app ]"
if test -d "/Applications/Dante Virtual Soundcard.app" \
        || test -d "$HOME/Applications/Dante Virtual Soundcard.app"; then
    echo "  Status : installed"
else
    echo "  Status : NOT installed"
    echo "  Get it : https://www.audinate.com/products/software/dante-virtual-soundcard"
fi
echo ""

# Dante Controller app
echo "[ Dante Controller app ]"
if test -d "/Applications/Dante Controller.app" \
        || test -d "$HOME/Applications/Dante Controller.app"; then
    echo "  Status : installed"
else
    echo "  Status : NOT installed (free)"
    echo "  Get it : https://www.audinate.com/products/software/dante-controller"
fi
echo ""

# Ethernet / network
echo "[ Network (Ethernet required for DANTE) ]"
ETH_FOUND=0
while IFS= read -r line; do
    if [[ "$line" =~ ^Hardware\ Port:.*[Ee]thernet|^Hardware\ Port:.*[Tt]hunderbolt ]]; then
        read -r devline
        IF=$(echo "$devline" | awk '{print $2}')
        IP=$(ipconfig getifaddr "$IF" 2>/dev/null || echo "")
        if [ -n "$IP" ]; then
            echo "  $IF : $IP  (connected)"
        else
            echo "  $IF : no IP address (cable plugged in?)"
        fi
        ETH_FOUND=1
    fi
done < <(networksetup -listallhardwareports 2>/dev/null)
if [ "$ETH_FOUND" -eq 0 ]; then
    echo "  No Ethernet adapter found — DANTE requires a wired connection"
fi
echo ""

echo "============================================================"
echo "Run 'Start Audio.command' to launch the audio engine."
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
    test -f "$package_root/VERSION.txt" \
        || fail "release version metadata is missing"
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

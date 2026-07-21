# Partitura del Juego

A generative audiovisual installation built with openFrameworks (C++) and SuperCollider. The piece processes pre-recorded sports footage through a real-time computer vision pipeline and renders the extracted data as a continuously evolving graphic score across eight vertical portrait screens spanning two networked computers. Sound is synthesised live in SuperCollider, driven by the combined CV stream from both machines over OSC.

> **Implementation status:** the two-machine, 8-channel architecture and the inter-machine network layer are currently in progress. The single-machine 4-channel build is fully functional and serves as the development baseline.

---

## Concept

The title translates literally as "Score of the Game." The work operates on a double meaning: *partitura* as musical score (a notation that prescribes what to play) and *juego* as both game and play. Sports video — already a document of collective movement and event — is re-read as raw data, and that data is turned into notation. The athletes become unwitting performers in a score they never see.

The installation does not attempt to analyse or interpret the game. It uses the game's motion, proximity, collisions and spatial density purely as input signals — the same way a composer might use a dice roll or a noise source — to drive a visual and sonic language that draws on Ryoji Ikeda's datamatic aesthetic: clinical, sparse, high-frequency, indifferent to narrative.

---

## System Architecture

The full installation runs across two computers connected on a local network. Each machine runs the same openFrameworks application managing four channels and four portrait monitors. A network sync layer (in progress) keeps the `GlobalDirector` state — slow-motion, fast-forward and screen-clear events — coherent across both machines so temporal gestures are felt simultaneously on all eight screens. The SuperCollider audio engine runs on one machine and receives OSC from both.

```
 ╔══════════════════════════════════════════════╗
 ║  MACHINE A  (channels 0–3)                   ║
 ║                                              ║
 ║  Video clips (cortos/)                       ║
 ║        |                                     ║
 ║   ClipPool  ──── random clip cycling         ║
 ║        |                                     ║
 ║   [4 x Channel]                              ║
 ║        |                                     ║
 ║   ofVideoPlayer ──── raw color frame         ║
 ║        |                                     ║
 ║   CVPipeline (OpenCV)                        ║
 ║        |  bg subtraction, optical flow,      ║
 ║        |  contour finding, blob tracking     ║
 ║        |                                     ║
 ║   EventDetector                              ║
 ║        |  collision, ball, crowd, leg dist   ║
 ║        |                                     ║
 ║   GraphicScore ──── 13 visual modes          ║
 ║        |                                     ║
 ║   4 x portrait window (1080×1920)            ║
 ║        |                                     ║
 ║   OSCSender ─────────────────────────────────╬──> SuperCollider
 ║        |     CV data, events, score mode     ║    pdj_datamatics.scd
 ║        |     UDP → SC host:9001              ║    (audio synthesis)
 ║        |                                     ║
 ║   GlobalDirector ◄── network sync (in prog.) ║
 ║        |  slow-mo / fast-fwd / clear         ║
 ║        |  broadcast to Machine B             ║
 ║        |                                     ║
 ║   ControlApp (ImGui) — live param editing    ║
 ╚══════════════════════╦═══════════════════════╝
                        ║  LAN — director sync
                        ║  (OSC or TCP, in progress)
 ╔══════════════════════╩═══════════════════════╗
 ║  MACHINE B  (channels 4–7)                   ║
 ║                                              ║
 ║  [same pipeline as Machine A]                ║
 ║                                              ║
 ║   4 x portrait window (1080×1920)            ║
 ║        |                                     ║
 ║   OSCSender ─────────────────────────────────╬──> SuperCollider
 ║        |     channels 4–7 stream             ║    (same SC instance
 ║        |     UDP → SC host:9001              ║     on Machine A)
 ║        |                                     ║
 ║   GlobalDirector ◄── network sync (in prog.) ║
 ║        |  receives director state from A     ║
 ║        |                                     ║
 ║   ControlApp (ImGui)                         ║
 ╚══════════════════════════════════════════════╝

  Physical layout (installation):
  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
  │ Ch 0 │ Ch 1 │ Ch 2 │ Ch 3 │ Ch 4 │ Ch 5 │ Ch 6 │ Ch 7 │
  │      │      │      │      │      │      │      │      │
  │  A   │  A   │  A   │  A   │  B   │  B   │  B   │  B   │
  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
       Machine A (4 outputs)        Machine B (4 outputs)
```

---

## Computer Vision Pipeline

Each channel runs an independent OpenCV pipeline on every frame. The video is optionally downscaled to half resolution for analysis, keeping the GPU display path at full resolution.

### Background Subtraction

`cv::BackgroundSubtractorMOG2` maintains a statistical model of the background over a rolling history window (default 120 frames). Every new frame is compared against this model to produce a binary foreground mask. The mask isolates moving bodies — players, ball, referees — from the static pitch.

### Optical Flow

`ofxCv::FlowFarneback` (dense Gunnar Farneback flow) runs on consecutive grayscale frames, producing a per-pixel 2D velocity field. Two aggregate values are extracted each frame:

- **Flow magnitude** — mean vector length across the field, encoding global motion intensity.
- **Flow angle** — mean vector direction, encoding dominant movement direction.

These feed directly into audio amplitude and stereo panning.

### Contour Finding and Blob Tracking

`ofxCv::ContourFinder` thresholds the foreground mask and finds contours. Each contour becomes a tracked blob with:

- normalised centroid position (x, y)
- frame-to-frame velocity (vx, vy)
- area and bounding box dimensions
- a persistent label for inter-frame tracking

The contour finder also accumulates **contour length** — the total perimeter of all detected shapes — which encodes the visual complexity of player arrangement.

### Edge Detection

Canny edge detection runs on the grayscale frame at tunable low/high thresholds. The resulting edge image is used directly by several visual modes (barcode, video lines).

### Event Detection

`EventDetector` interprets blob data into higher-level events:

| Event | Detection logic |
|---|---|
| **Collision** | IoU between bounding boxes exceeds threshold, or blob count drops suddenly |
| **Ball** | A blob smaller than `ballMaxArea` px² whose velocity exceeds `ballMinSpeed` |
| **Crowd** | At least `crowdMinBlobs` blobs within normalised radius `crowdMaxDist` |
| **Leg distance** | Spatial spread of blob centroids along the vertical axis |

---

## Visual Techniques

The graphic score cycles through 13 rendering modes, automatically sequenced by an internal director. Each mode holds for a random duration between `minModeDuration` and `maxModeDuration` seconds. A `BwClean` pause is inserted between every active mode, providing visual breathing room and framing each technique as a distinct episode.

### Base Film Look (B&W shader)

All modes that do not use raw color begin with the same GLSL shader pass (`bw.frag`). The shader converts the video to grayscale using Rec.709 luminance coefficients and applies a film processing chain:

- **Gamma curve** — adjustable exponent, default 0.95
- **Contrast and brightness** — linear adjustment around 0.5 midpoint
- **Film S-curve** — Hermite smooth-step blended with the linear signal, lifting shadows and compressing highlights to approximate chemical film tonal response
- **Elliptical vignette** — edge darkening wider horizontally, matching portrait aspect ratio
- **Animated grain** — hash noise sampled at a position jittered by elapsed time, producing a different grain pattern every frame
- **Optional posterize** — quantises luma to a fixed number of steps (disabled by default)
- **Optional hard threshold** — smooth-step edge for graphic stencil look (disabled by default)

### Score Modes

**BwClean** — base film-look only, no overlay. Functions as a rest state and visual punctuation between active modes.

**ScanLine** — horizontal lines whose length is modulated by `motionEnergy × sin(y, time)`. Line width breathes with the energy of the frame. When the ball is detected, a crosshair marks its position. The lines read as a scope or oscilloscope trace of collective movement.

**BBoxTracker** — bounding boxes drawn around each tracked blob, with corner tick marks (surveillance-camera UI aesthetic). Each box is labelled with its persistent blob ID and area. A velocity arrow from each centroid encodes the direction and magnitude of individual player motion. Ball detection adds a labelled circle.

**BinaryText** — blob positions, velocities and crowd density are encoded as 8-bit binary strings and printed as dense text. The frame counter runs in the header. The display reads as machine telemetry — data that is formally precise but humanly opaque.

**Waveform** — a scrolling history of `motionEnergy` drawn as a horizontal bar graph filling the screen from left to right, and simultaneously as a right-edge dot trail. The full history (up to 1920 samples) fills the height, so the image becomes a temporal cross-section of activity over the past ~64 seconds at 30 fps.

**GridData** — a 32×48 grid of crosshair marks whose arm length is modulated by `motionEnergy × sin(col, row, time)`. Active blob centroids are marked with larger crosshairs and coordinate readouts. The grid makes the screen into a measurement surface: density, rhythm and phase relationships across the frame become visible as a field.

**Barcode** — the foreground mask is compressed into 64 vertical bars. Each bar height represents the average foreground density of one vertical column slice of the mask. The result reads as a barcode whose dimensions are dictated by where the players are standing. Still phases produce narrow bars; dense play fills the screen.

**VideoNormal** — raw color video, cover-cropped to fill the frame. Used as a contrast moment — direct, unmediated image — so that the return to processed modes is felt as a shift in register.

**VideoSquares** — the B&W processed base layer with up to six color video patches cropped from blob centroid positions. Each patch shows the raw color image at the location of a detected player. Corner-tick borders mark each patch. The mode layers analytical distance (the B&W frame) against intimate proximity (the color close-up).

**VideoNumbers** — the video is expressed as a 36×64 grid of digits 0–9. Each digit's value corresponds to the brightness of the corresponding cell, scaled so 0 = black and 9 = white. Dark cells are skipped entirely. The screen becomes text that is also image, and image that is also measurement.

**VideoLines** — a three-layer line drawing:

1. *CLD (Coherent Line Drawing)* — bilateral filtering followed by `ofxCv::CLD` (Flow-based Difference of Gaussians) extracts interior body lines with the quality of pencil drawing. Upscaled bilinearly to add softness.
2. *FG silhouettes* — the foreground mask is morphologically closed to fill jersey-gap holes, contours are found, approximated with `approxPolyDP`, smoothed with `getSmoothed(9)` and resampled to even spacing. Three drawing passes (wide soft halo, mid-glow, crisp ink line) give each silhouette a hand-drawn depth.
3. *Optical flow strokes* — the Farneback flow field is sampled on a grid; velocity vectors above a minimum magnitude become short directed line segments. The layer makes motion direction legible as a field of marks.

**ThermalVision** — the Ironbow / FLIR thermal colormap is applied via `thermal.frag`. The shader maps luma through five color stops: black (cold) → deep violet → dark crimson → orange → yellow → white (hot). A subtle animated sensor-grid artifact (mod-2 scanline pattern, 2.5% modulation) simulates thermal camera digitisation noise. A vertical gradient bar on the right edge shows the full palette. Active blobs are marked with crosshairs and fake temperature readouts derived from blob area.

**SlitScan** — temporal slit-scan. Each frame, the center column of the analysis-resolution grayscale image is written into a ring buffer `kSlitW` (270) columns wide. The buffer is then unrolled left-to-right, oldest first, into a display image. The result compresses time into space: one second of play occupies a fixed horizontal span, and the Y axis remains spatial. A brightness gradient fades the past (left, 45% brightness) toward the present (right, 100%). Second-tick marks along the bottom label the time axis. When the mark color is not white, the assembled image is tinted with the current mark hue.

**Flash** — a full-frame white rectangle that fades from alpha 200 to 0 over `flashDuration` (default 0.25 s). Triggered automatically on collision detection events. The flash interrupts whatever mode is active and resumes it on completion.

### Mark Color Cycling

On every mode transition the mark color advances through a fixed palette: white → red → electric blue → repeat. This gives each mode episode a distinct chromatic identity and makes the sequence of modes readable as a rhythm even with the sound off.

### Strobe Overlay

On every clip change, a full-frame white or red rectangle fades out at 1100 alpha units per second (~0.23 s). This marks the editorial cut between clips as a visual event — a flashframe.

### Persistent Data HUD

Every mode carries a transparent layer on top:

- A 2 px wide vertical energy bar at the left edge, bottom-anchored, representing current `motionEnergy`
- Frame counter and blob count in the bottom-left corner
- Ball crosshair wherever a ball is detected

The HUD keeps the display in a constant state of monitoring, regardless of which visual mode is active.

---

## Compositional Elements

### The Sequence as Score

The mode sequence in `buildSequence()` functions as a written score. It prescribes an ordered rotation of techniques — each separated by a `BwClean` rest — that defines the arc of visual language over the piece's duration. Because each mode holds for a random time between `minModeDuration` and `maxModeDuration`, the sequence does not have a fixed clock: it is indeterminate in duration but determinate in order.

The sequence is not random. `ThermalVision` appears three times, `SlitScan` appears twice (labeled "temporal time-scroll #1" and "#2"), `VideoNormal` and `VideoLines` each appear twice. The repetitions function like refrains — recurring perspectives on the same material.

### The GlobalDirector

`GlobalDirector` applies temporal manipulation to all channels simultaneously. Two independent processes run on configurable random intervals:

- **Slow motion** (default 20% speed): a ramp-down phase, a hold, a ramp-up, each with tunable durations. Auto-triggered every 25–50 seconds.
- **Fast forward** (default 280% speed): much shorter ramp and hold phases. Auto-triggered every 15–35 seconds.

Both use a Hermite ease function so the speed changes are smooth rather than abrupt. The slow and fast phases can overlap or conflict; the director does not prevent double-application.

A third process triggers **screen-clear events**: full-frame colored fills (black or red) that fade in, hold briefly and fade out. These punctuate the flow of images and reset visual attention.

All three processes have manual triggers accessible from the ControlApp, and all three can be disabled independently.

**Cross-machine synchronisation (in progress):** in the two-machine setup, Machine A's `GlobalDirector` acts as the authority. When it fires a slow, fast or clear trigger — whether from the auto-scheduler or from a manual ControlApp button — it broadcasts the event and its full parameter state to Machine B over the network (planned transport: OSC over UDP). Machine B's director receives the message and executes the same transition from the same starting parameters, so both banks of four screens respond simultaneously. The internal frame-guard inside `GlobalDirector` prevents duplicate updates within a single real frame, making the receiver-side safe to call from any update loop.

### Eight Channels across Two Machines

Each machine runs four independent pipeline instances in parallel. Within a machine the clip pool is shared but each channel picks independently, so the four columns show different clips simultaneously — a polyphonic visual structure of four unsynchronised readings of the same footage. Across both machines the same principle extends to eight columns, doubling the panoramic width.

The `GlobalDirector` synchronises the temporal feel (slow/fast/clear) across all eight channels: within a machine this is handled in-process; across machines it is handled by the planned network sync layer described above. Visual mode sequencing and clip selection remain independent per channel regardless of machine.

The installation layout is eight 1080×1920 portrait monitors placed side by side:

- Machine A drives monitors 0–3 (columns 1–4, leftmost bank)
- Machine B drives monitors 4–7 (columns 5–8, rightmost bank)

Total display width: 8640 px (8 × 1080). Each monitor is a 9:16 portrait column.

### OSC Bridge and Sound

Each machine runs its own `OSCSender` instance, which transmits the full CV data stream at ~30 fps to SuperCollider over UDP. Both senders target the same SuperCollider host (running on Machine A by default). The OSC address namespace is channel-indexed (`/pdj/channel/0` through `/pdj/channel/7`), so the two streams merge into a single coherent 8-channel data space inside SuperCollider without collision.

The data per channel includes: flow magnitude and angle, motion energy, blob count, contour length, up to 8 individual blob positions and velocities, all event flags, and the current score mode (0–13) on every mode change.

The SuperCollider engine (`pdj_datamatics.scd`) maps this stream to an Ikeda-informed sound world:

- **Sustained pure tones** (`pdjTone`): one per channel, frequency modulated by blob Y position and flow magnitude, amplitude following motion energy with a quadratic response (prefers silence)
- **High clicks** (`pdjClick`): triggered on collision and ball-detection events; the click sweeps across the stereo field in the direction of the detected blob's velocity
- **Digital glitch bursts** (`pdjGlitch`): random-hold filtered noise triggered by high blob counts and mode changes; intensity indexed to the `~modeGlitch` table (Barcode and Flash modes have the highest glitch weight)
- **Motion-driven noise bed** (`pdjNoiseBed`): band-pass filtered noise whose center frequency tracks energy
- **Bit-reduction pulse** (`pdjBitPulse`): Dust-triggered Latch noise that stochastically fires proportional to the glitch bus
- **Sub pressure** (`pdjSub`): very low sine tone whose amplitude tracks crowd density
- **Rhythmic pulse** (`pdjBeat`): tempo derived from energy and flow, gated probabilistically by the macro beat level; transitions between sine body and hard data click based on macro density
- **Textural wash** (`pdjWash`) and **data crackle** (`pdjCrackle`): global noise layers whose amplitude tracks the macro noise level

A **macro state** (`noise`, `beat`, `space`, `density`) performs a slow random walk over 6–14 second steps, biased by the average motion energy across all channels. With eight channels feeding in, the macro drift is anchored to a wider picture of collective activity. The current engine is written for four channels (`~numChannels = 4`); expanding to eight channels requires updating that constant and the `~baseFreqs` and `~ch` arrays — this is part of the in-progress two-machine work.

A ping-pong delay (`pdjDelayFX`) with wandering delay time adds spatial depth; its wetness is controlled by the macro space level and by individual event send depths. Ball events furthest from the camera (low Y) are sent deeper into the delay than near events, simulating physical space.

---

## Dependencies

| Library | Role |
|---|---|
| openFrameworks | graphics, video playback, window management |
| ofxOpenCv | OpenCV wrapper for openFrameworks |
| ofxCv | higher-level CV utilities (CLD, Farneback flow, ContourFinder) |
| ofxOsc | OSC send/receive |
| ofxImGui | ImGui-based control panel |
| OpenCV | background subtraction (MOG2), edge detection, optical flow |
| SuperCollider | audio synthesis engine |
| Syphon | (bundled) GPU texture sharing, available for routing to external video software |
| FMOD | (bundled) |

---

## Configuration

All runtime parameters are in `bin/data/settings.json`. Window positions are specified in screen coordinates. The file includes commented layout presets:

| Preset key | Use |
|---|---|
| `channels` (active array) | current window positions |
| `_testLayout` | development — 4 small windows on a single display |
| `_installationLayout` | installation — 4 × 1080×1920 portrait monitors |
| `_installationControl` | control window position for installation |

For the two-machine setup, each machine uses its own copy of `settings.json` with window coordinates relative to its own display arrangement. The `osc` block on Machine B must point its `host` at the IP address of Machine A (where SuperCollider is running). The planned network sync adds a `networkSync` block to configure the director broadcast address and port.

The ControlApp (key `U` to show/hide) provides live access to all CV, image, event detection, score, and temporal director parameters without recompilation. In the two-machine setup, Machine A's ControlApp is the primary control surface; Machine B's ControlApp remains available for local adjustments but director triggers should be issued from Machine A to ensure both machines stay in sync.

---

## Running

### Development (single machine, 4 channels)

```bash
# Edit settings.json: swap _testLayout into "channels" for small test windows
make && bin/Partitura_del_Juego

# Audio — start SuperCollider after oF is running
# Open supercollider/pdj_datamatics.scd
# Cmd+Enter on the outer block
# OSC host/port must match settings.json (default localhost:9001)

# Synthetic audio feed without oF
~testOsc.play;

# Shutdown audio
~shutdown.();
```

### Installation (two machines, 8 channels)

Both machines must have identical copies of the `cortos/` clip folder and the compiled application.

**Machine A (primary):**

```bash
# settings.json: use _installationLayout (channels 0–3)
# osc.host: localhost (SC runs here)
# networkSync.role: "primary"  [planned field]
# networkSync.broadcastTo: "<Machine B IP>:<port>"  [planned field]
make && bin/Partitura_del_Juego

# Start SuperCollider
# Edit pdj_datamatics.scd: set ~numChannels = 8
# Update ~baseFreqs to cover 8 channels
# Cmd+Enter
```

**Machine B (secondary):**

```bash
# settings.json: use _installationLayout (channels 4–7, adjust x offsets for B's monitors)
# osc.host: <Machine A IP>  (SC is on A)
# networkSync.role: "secondary"  [planned field]
# networkSync.listenPort: <port>  [planned field]
make && bin/Partitura_del_Juego
# No SuperCollider needed on B
```

**Startup order:** start Machine A first (so SuperCollider and the director broadcast are ready), then Machine B. The ControlApp on Machine A is the primary control surface for the full installation.

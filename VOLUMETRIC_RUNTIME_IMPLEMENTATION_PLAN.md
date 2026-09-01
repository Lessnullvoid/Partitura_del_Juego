# Partitura del Juego — Volumetric Runtime

## Step-by-Step Development and Implementation Plan

**Audience:** Cursor and developers working on the next version of *Partitura del Juego*  
**Target runtime:** openFrameworks, C++, macOS, Apple Silicon, OpenGL 4.1  
**Target computer:** Mac mini M4 Pro  
**Target installation:** eight portrait displays driven through two ICUIXIAN 1-to-4 video-wall controllers  
**Document status:** implementation plan; no implementation is included in this document

---

## 1. Purpose of this document

This document defines the implementation plan for a new version of *Partitura del Juego* in which football players are always represented as volumetric generators.

The new application must not display the source video. Video is used only during an offline analysis stage. That analysis produces compact, time-indexed data packages. During the installation, the openFrameworks runtime loads those packages and reconstructs players as high-density point clouds, luminous splats, filaments, temporal trails, depth slices, particles, and related spatial structures.

The current application must be preserved as a fully recoverable legacy version before development begins.

This document is deliberately explicit. Cursor should treat the architectural boundaries, preservation rules, phase gates, and acceptance criteria as project requirements rather than optional suggestions.

---

## 2. Non-negotiable design decisions

The following decisions define the new version:

1. Players are always represented as volumetric entities.
2. The installation runtime never displays the source video.
3. The installation runtime does not perform heavy computer vision.
4. Detection, tracking, segmentation, pose estimation, depth estimation, and dense motion analysis happen offline.
5. The runtime receives numerical and image-derived analysis data, not source-video textures.
6. Player identities remain temporally stable through persistent tracking IDs.
7. Every volumetric generator begins with the same underlying player-volume representation.
8. The rendering system uses depth, high-precision color, emissive rendering, and controlled postprocessing.
9. The eight screens may behave independently or as regions of one shared spatial field.
10. Visual complexity must arise from spatial organization, temporal memory, transformation, and data relationships rather than arbitrary decoration.
11. Darkness and sparse states are active parts of the composition.
12. The legacy program must remain restorable, compilable, and independently usable.

---

## 3. Conceptual transformation

### Current system

```text
Source video
    -> runtime video decoder
    -> runtime OpenCV analysis
    -> processed video and 2D graphic generators
    -> channel FBO
    -> eight-screen presentation
```

### New system

```text
OFFLINE

Source video
    -> player detection
    -> persistent multi-object tracking
    -> per-player segmentation
    -> pose estimation
    -> monocular depth estimation
    -> optical flow and motion features
    -> temporal stabilization
    -> event and collective-feature extraction
    -> PDJV data package
```

```text
INSTALLATION RUNTIME

PDJV data package
    -> asynchronous reader and frame cache
    -> timeline interpolation
    -> player-volume reconstruction
    -> GPU particle and point simulation
    -> volumetric generator scenes
    -> HDR luminous renderer
    -> bloom, feedback, depth treatment, and tone mapping
    -> two presentation surfaces
    -> two ICUIXIAN controllers
    -> eight portrait screens
```

The source footage disappears as an image and remains as computational memory.

---

## 4. Current codebase assessment

The existing project already provides infrastructure that should be preserved or adapted:

- eight-channel architecture
- two presentation windows, each divided into four portrait regions
- shared OpenGL contexts
- ICUIXIAN output configuration
- `VisualComposer` timing and organization concepts
- `GlobalDirector`
- `PerformanceMonitor`
- `SettingsStore`
- `OSCSender`
- SuperCollider integration
- control-window foundations
- deterministic seeds and chapter revisions

Important existing files include:

- `src/main.cpp`
- `src/PresentationApp.*`
- `src/VisualComposer.*`
- `src/GlobalDirector.*`
- `src/PerformanceMonitor.*`
- `src/SettingsStore.h`
- `src/OSCSender.*`
- `src/Channel.*`
- `src/CVPipeline.*`
- `src/VisualGenerator.*`
- `src/GraphicScore.*`

### Current limitations relevant to the new direction

The existing `VisualGenerator` is primarily a collection of immediate-mode 2D renderers. It uses rectangles, lines, polylines, text, and low-complexity procedural fields.

The current analysis reduces player-like regions to:

- centroid
- velocity
- area
- bounding-box dimensions
- aggregate flow magnitude and direction

The current renderer does not yet provide:

- persistent player volumes
- depth maps
- pose data
- stable player IDs in the exported `CVData::BlobEntry`
- high-density point-cloud state
- 3D cameras
- a depth buffer in generator FBOs
- HDR generator rendering
- GPU particle feedback state
- splat rendering
- volumetric temporal trails

The new application should reuse the system-level infrastructure while replacing the video/CV channel core with a data-driven volumetric channel core.

---

## 5. Preservation procedure for the current version

### Important current condition

At the time this plan was written, the repository contained modified tracked files and untracked files. The existing behavior must be reviewed and committed before creating the volumetric development branch.

Cursor must not silently commit, discard, overwrite, reset, or reorganize these changes.

### Required preservation sequence

1. Review the current working tree.
2. Confirm which generated files and local machine settings should remain untracked.
3. Build the current application.
4. Run the current four- or eight-channel test configuration.
5. Confirm that video playback, control UI, OSC, and presentation windows function.
6. Commit the complete functional state as a legacy baseline.
7. Create an annotated tag such as:

   ```text
   pdj-legacy-visual-v1
   ```

8. Create an archival branch such as:

   ```text
   archive/legacy-visual-v1
   ```

9. Save a distributable legacy build.
10. Record the exact openFrameworks version, addon revisions, macOS version, and build instructions.
11. Create the new development branch only after the baseline is recoverable.

Suggested new development branch:

```text
feature/volumetric-runtime
```

### Preservation artifacts

Store the following outside the volatile build directories:

- legacy application build
- legacy `settings.json`
- legacy SuperCollider files
- legacy shaders
- addon revision list
- compilation instructions
- screenshot of control UI
- eight-output test record
- known limitations

### Preservation acceptance criterion

The preservation phase is complete when a developer can check out the legacy tag and compile or run the previous application without any volumetric-runtime files.

---

## 6. New product architecture

The volumetric version consists of two programs and one shared data specification.

```text
pdj-analyzer
    offline analysis and PDJV export

PDJV format
    versioned bridge between analysis and runtime

pdj-volumetric
    openFrameworks installation runtime
```

### 6.1 `pdj-analyzer`

Recommended initial implementation environment: Python.

Reasons:

- mature model ecosystem
- simpler experimentation with detection, segmentation, pose, and depth
- analysis does not need to run in real time
- processing can happen on another machine
- model dependencies remain outside the installation runtime

Responsibilities:

- inspect source clips
- detect players
- maintain persistent track IDs
- segment each player
- estimate pose
- estimate monocular depth
- compute dense or sampled motion
- temporally stabilize masks, depth, and pose
- estimate confidence
- derive collective relationships and events
- export PDJV packages
- produce validation previews and reports

### 6.2 PDJV data specification

PDJV is a versioned, deterministic, source-video-independent package.

Responsibilities:

- describe a clip timeline
- store per-frame player observations
- provide spatial masks and relative depth
- provide stable IDs
- support seeking and interpolation
- allow asynchronous frame loading
- remain extensible through versioning

### 6.3 `pdj-volumetric`

Implementation environment: openFrameworks and C++.

Responsibilities:

- load PDJV packages
- maintain synchronized timelines
- interpolate analysis frames
- create persistent `VolumetricPlayer` objects
- build or update point clouds
- run GPU simulations
- render point sprites, splats, lines, and slices
- direct camera and generator states
- render two four-segment output windows
- send OSC to SuperCollider
- expose installation controls and diagnostics

---

## 7. PDJV package design

### 7.1 Proposed directory structure

```text
clip_identifier.pdjv/
├── manifest.json
├── timeline.bin
├── frame_index.bin
├── observations.bin
├── masks.bin
├── depth.bin
├── motion.bin
├── events.json
├── validation.json
└── thumbnail.png
```

The exact filenames may change, but each responsibility should remain separate enough to support partial loading and version migration.

### 7.2 Manifest

Example:

```json
{
  "format": "PDJV",
  "version": 1,
  "sourceId": "match_001_clip_023",
  "analysisId": "2026-09-01_model-set-a",
  "fps": 30.0,
  "frameCount": 1842,
  "durationSeconds": 61.4,
  "sourceWidth": 1920,
  "sourceHeight": 1080,
  "coordinateSystem": "normalized_image",
  "depthConvention": "relative_camera_z",
  "jointSchema": "documented-schema-id",
  "features": {
    "masks": true,
    "depth": true,
    "pose": true,
    "motion": true,
    "events": true,
    "teamLabels": false
  },
  "compression": {
    "observations": "documented-codec",
    "masks": "rle-or-block-codec",
    "depth": "uint16-block-codec"
  }
}
```

### 7.3 Per-frame observation data

Each frame should contain:

```text
frame number
timestamp
global motion energy
collective centroid
collective direction
collective density
event references
player count

for each player:
    stable tracking ID
    optional team ID
    normalized bounding box
    normalized centroid
    velocity
    acceleration
    observation confidence
    mask offset and byte length
    depth offset and byte length
    motion offset and byte length
    pose joints
    joint confidence
```

### 7.4 Sparse per-player spatial storage

Do not store full-frame masks and depth images for every player. Store cropped data associated with each player's bounding box.

```text
full-frame normalized bounding box
    + cropped binary or confidence mask
    + cropped relative-depth image
    + optional cropped motion field
```

Recommended logical representation:

- mask: 8-bit confidence or binary RLE
- relative depth: 16-bit normalized values
- motion: signed 16-bit or half-float vectors on a reduced grid
- pose: half- or full-float joint positions and confidence

Do not prematurely optimize the first prototype. First create a readable reference format, validate it, and only then introduce compression and binary packing.

### 7.5 Coordinate conventions

Define conventions once and include them in the manifest:

- image X: 0 at left, 1 at right
- image Y: 0 at top, 1 at bottom
- relative depth Z: documented near/far convention
- time: seconds from clip start
- velocity: normalized image units per second
- acceleration: normalized image units per second squared
- pose joints: normalized within full source frame, not within the player crop

All runtime conversion into channel pixels, world coordinates, or wall coordinates must happen after loading.

### 7.6 IDs and continuity

Tracking IDs must remain stable for as long as the player can reasonably be associated across frames.

When tracking becomes uncertain:

- preserve the previous ID for a bounded gap if confidence remains acceptable
- record confidence decay
- avoid immediately reusing retired IDs
- distinguish a missing observation from player death
- allow the runtime to dissolve a player gracefully

Point-level stable identity should be derived deterministically:

```text
pointStableId = hash(clipId, playerTrackingId, pointSeed)
```

This enables temporal trails and reconstruction even when point positions change.

### 7.7 Versioning

Every package must contain:

- format version
- analysis-pipeline identifier
- source identifier
- feature flags
- coordinate conventions
- codec identifiers

The runtime must reject unsupported major versions with a clear error. Minor optional fields should be skipped safely.

---

## 8. Offline analysis pipeline

### Step 1 — Source inspection

For each source clip:

- record resolution, frame rate, duration, and rotation
- generate a unique source ID
- detect damaged or variable-frame-rate media
- normalize timestamps
- optionally transcode only for analysis consistency

The installation runtime will not use this normalized video.

### Step 2 — Player detection

Detect player candidates per frame.

Requirements:

- output bounding box
- class confidence
- frame timestamp
- preserve multiple simultaneous players
- avoid treating spectators or field markings as players where possible

The detector is a proposal source, not the final identity source.

### Step 3 — Persistent tracking

Associate detections across frames.

Use:

- bounding-box overlap
- motion prediction
- appearance features if needed
- pose consistency
- field position
- temporal gap tolerance

Output:

- stable tracking ID
- track confidence
- visibility state
- interpolated short gaps
- explicit termination

### Step 4 — Instance segmentation

Create a player-specific mask for each tracked detection.

Requirements:

- preserve arms and legs where possible
- avoid merging adjacent players
- return confidence, not only a hard binary mask
- crop to the player bounding box
- stabilize mask boundaries temporally

Store a hard mask only if disk limitations require it. A confidence mask provides more expressive point emission and more graceful uncertain edges.

### Step 5 — Pose estimation

Estimate a documented joint schema.

Minimum useful regions:

- head
- shoulders
- elbows
- wrists
- hips
- knees
- ankles

Requirements:

- joint confidence
- temporal smoothing
- no sudden teleportation when confidence is low
- explicit missing joints
- preservation of raw and smoothed data during validation, if practical

### Step 6 — Monocular depth estimation

Produce relative depth for the player crop or full frame, then retain only player regions.

Requirements:

- temporally stabilize scale and offset
- normalize depth consistently within a clip
- avoid frame-to-frame depth inversion
- preserve confidence or invalid regions
- document whether larger values mean nearer or farther

The goal is expressive, temporally coherent 2.5D reconstruction, not metric surveying.

### Step 7 — Motion analysis

Compute one or more of:

- dense optical flow
- sampled flow grid
- tracked centroid velocity
- pose-joint velocity
- acceleration
- silhouette deformation
- local motion energy

Avoid storing full-resolution flow unless tests prove it materially improves the result.

### Step 8 — Temporal stabilization

Stabilize:

- track IDs
- masks
- depth range
- pose joints
- centroid velocity
- acceleration
- player scale

Different quantities require different smoothing. Avoid applying one generic low-pass filter to everything.

Suggested behavior:

- positions: smooth lightly to preserve athletic motion
- depth: smooth more strongly to avoid surface flicker
- pose: confidence-aware filtering
- masks: edge-aware temporal consistency
- acceleration: derive after position smoothing

### Step 9 — Events and collective features

Extract:

- acceleration peaks
- direction changes
- proximity
- cluster formation
- cluster dissolution
- collision candidates
- possession or ball relationships when available
- collective centroid
- density
- spread
- dominant direction

Events should include timestamps, positions, confidence, and involved tracking IDs.

### Step 10 — PDJV export

Export:

- manifest
- frame index
- observation stream
- masks
- depth
- motion
- events
- validation summary

### Step 11 — Validation render

The analyzer must generate a diagnostic preview showing:

- track IDs
- bounding boxes
- masks
- pose
- depth
- confidence
- dropped or interpolated observations

This preview is for analysis validation only. It is not used by the installation runtime.

### Offline-analysis acceptance criteria

- IDs are temporally stable.
- Player masks remain coherent during motion.
- Depth does not flicker severely.
- Pose errors are confidence-tagged.
- Short occlusions do not create unnecessary new players.
- Every exported frame can be indexed.
- The package can be validated without the source video.

---

## 9. Runtime data layer

### 9.1 Proposed classes

```text
PdjvManifest
PdjvPackage
PdjvReader
PdjvValidator
FrameIndex
FrameCache
TimelinePlayer
VolumetricFrame
PlayerObservation
CollectiveObservation
DetectedEvent
```

### 9.2 `PdjvReader`

Responsibilities:

- open the package
- validate version and feature flags
- read the frame index
- request frames by timestamp or frame number
- decode observations and spatial blocks
- report errors without crashing the render loop

### 9.3 `FrameCache`

Use a bounded asynchronous cache.

Responsibilities:

- prefetch future frames
- retain adjacent frames for interpolation
- release old frames
- avoid allocation on the render thread
- report cache misses
- use the most recent valid state during bounded temporary stalls

Recommended conceptual states:

```text
Empty
Loading
Ready
Failed
Evictable
```

### 9.4 `TimelinePlayer`

Responsibilities:

- play
- pause
- stop
- seek
- change speed
- loop
- report normalized and absolute position
- select adjacent analysis frames
- calculate interpolation alpha
- trigger events once

Do not bind simulation time directly to render frame count. Use elapsed seconds and explicit timestamps.

### 9.5 Frame interpolation

Interpolate continuous quantities:

- centroid
- velocity
- acceleration
- pose positions
- global density
- collective direction

Do not naïvely interpolate:

- tracking IDs
- event presence
- missing observations
- mask topology

Mask and depth interpolation should use either nearest-frame selection for the first prototype or a dedicated confidence-aware method later.

---

## 10. Runtime volumetric data model

### 10.1 `VolumetricFrame`

```cpp
struct VolumetricFrame {
    double timestamp = 0.0;
    std::vector<PlayerObservation> players;
    CollectiveObservation collective;
    std::vector<DetectedEvent> events;
};
```

### 10.2 `PlayerObservation`

Conceptual fields:

```cpp
struct PlayerObservation {
    int trackingId = -1;
    int teamId = -1;

    glm::vec2 centroid{0.f};
    glm::vec2 velocity{0.f};
    glm::vec2 acceleration{0.f};
    glm::vec4 boundingBox{0.f};

    PoseObservation pose;
    SpatialBlockReference mask;
    SpatialBlockReference depth;
    SpatialBlockReference motion;

    float confidence = 0.f;
    bool observed = false;
};
```

### 10.3 `VolumetricPlayer`

This is a persistent runtime entity, not a temporary frame record.

Responsibilities:

- retain tracking ID
- retain deterministic point seeds
- maintain current and target observations
- maintain temporal history
- manage appearance and disappearance envelopes
- expose player-level forces
- preserve simulation state during short observation gaps

Conceptual structure:

```cpp
class VolumetricPlayer {
public:
    void setup(const VolumetricPlayerConfig& config);
    void ingest(const PlayerObservation& observation, double timestamp);
    void markUnobserved(double timestamp);
    void update(float dt);
    void reset();

    int trackingId() const;
    const PlayerVolumeState& state() const;
    const TemporalHistory& history() const;
};
```

### 10.4 Observation loss

When a player disappears temporarily:

1. stop introducing new observation-constrained points;
2. retain existing points for a bounded grace period;
3. reduce confidence;
4. increase dispersion or controlled dissolution;
5. reconnect if the same tracking ID returns;
6. retire the player only after the configured timeout.

Never snap missing players to `(0, 0, 0)`.

---

## 11. Translating advanced TouchDesigner techniques into openFrameworks

The goal is not to embed TouchDesigner. The goal is to reproduce useful GPU design patterns inside the openFrameworks runtime.

### 11.1 Point attributes stored in floating-point textures

TouchDesigner commonly represents point-cloud attributes in TOP textures, with one pixel representing one point or one attribute set. Its point-cloud documentation describes XYZ positions stored in floating-point image channels. See [TouchDesigner Point Clouds](https://docs.derivative.ca/Point_Clouds).

Equivalent openFrameworks design:

```text
positionState RGBA32F
    RGB = XYZ
    A = active / age

velocityState RGBA16F or RGBA32F
    RGB = velocity XYZ
    A = confidence or drag

attributeState RGBA16F
    R = normalized player ID
    G = body-region ID
    B = luminance or energy
    A = lifetime
```

Use `ofFbo` and `ofShader` to update these textures.

### 11.2 Depth-map projection

TouchDesigner's depth-projection component converts a 2D depth map into a 3D point cloud and stores the result in a floating-point texture suitable for instancing. See [depthProjection](https://docs.derivative.ca/Palette%3AdepthProjection).

Equivalent vertex reconstruction:

```text
u, v = normalized mask sample coordinate
z = decoded relative depth
x = remap(u, player bounding box, world scale)
y = remap(v, player bounding box, world scale)
worldPosition = cameraModel(x, y, z)
```

The first prototype can use artistic orthographic projection. Camera-intrinsic reconstruction can be added if source-camera information becomes available.

### 11.3 GPU feedback simulation

TouchDesigner's Feedback POP reuses the previous frame's point state and passes it through another GPU processing cycle. See [Feedback POP](https://docs.derivative.ca/Feedback_POP).

Equivalent openFrameworks pattern:

```text
previous position and velocity textures
    -> simulation shader
    -> next position and velocity textures
    -> swap references
```

Use this for:

- particle advection
- drag
- attraction to the observed body
- repulsion
- turbulence
- fragmentation
- reconstruction
- death and rebirth

### 11.4 Particle attributes and lifecycle

TouchDesigner's Particle POP uses attributes such as position, velocity, mass, drag, force, emission, and death. See [Particle POP](https://docs.derivative.ca/Particle_POP).

Equivalent runtime attributes:

- position
- velocity
- acceleration or force
- mass
- drag
- age
- lifetime
- birth state
- death state
- player ID
- stable point ID
- anatomical region
- observation confidence
- target body position

### 11.5 Stable-ID temporal trails

TouchDesigner's Trail POP can connect point history through a stable attribute ID. See [Trail POP](https://docs.derivative.ca/Trail_POP).

Equivalent design:

- retain several time slices of point positions;
- index them by stable point ID;
- render slices as points, line strips, ribbons, or depth layers;
- express trail length in seconds rather than frames;
- reset explicitly on clip or scene changes.

### 11.6 Pre-reading heavy point sequences

TouchDesigner's Point File In tools support pre-reading and buffering point-cloud sequences. Its documentation recommends binary formats such as OpenEXR for multi-channel point data and describes read-ahead behavior. See [Point File In TOP](https://docs.derivative.ca/Point_File_In_TOP).

Equivalent design:

- indexed binary PDJV files
- asynchronous read thread
- bounded decompression queue
- current/previous/next frame retention
- configurable prefetch seconds
- timeout and latest-valid-frame strategy

### 11.7 POP architecture as design reference

TouchDesigner's Point Operators operate on GPU-resident points and attributes, supporting point clouds, particles, lines, curves, and geometry. See [Learning About POPs](https://docs.derivative.ca/Learning_About_POPs).

Equivalent internal principle:

```text
one shared point representation
    -> transformations
    -> grouping
    -> history
    -> simulation
    -> multiple render interpretations
```

Avoid implementing each generator as a completely unrelated renderer.

### 11.8 Gaussian splat principles

TouchDesigner's current Gaussian-splat support uses position, color, rotation, scale, and higher-order appearance attributes. See [Gaussian Splats](https://docs.derivative.ca/Experimental%3AGaussian_Splats).

The project does not initially require full Gaussian Splatting. Implement an artistic approximation:

- camera-facing elliptical billboard
- position XYZ
- scale XY
- orientation derived from velocity, pose, or estimated normal
- opacity
- luminance
- depth test
- soft radial or anisotropic falloff

This will create a denser and more volumetric appearance than basic GL points.

### 11.9 Shader limitations on the target Mac

The application currently requests OpenGL 4.1. TouchDesigner documents that GLSL compute shaders require GLSL 4.30 or later. See [GLSL TOP](https://docs.derivative.ca/GLSL_TOP).

Therefore:

- do not require OpenGL compute shaders;
- do not build the runtime around SSBOs unavailable in the target OpenGL path;
- use fragment-shader ping-pong FBO simulation;
- use vertex-shader sampling and instanced geometry where compatible;
- verify every shader on the actual Mac mini M4 Pro.

---

## 12. GPU particle-state implementation

### 12.1 State texture layout

Begin with square or rectangular particle-state textures.

Example quality tiers:

```text
Preview:      128 x 128 state = 16,384 slots
Medium:       256 x 256 state = 65,536 slots
High:         512 x 512 state = 262,144 slots
```

These are global examples, not guaranteed per-player budgets. The final allocation must follow profiling.

### 12.2 Ping-pong container

Create a reusable abstraction:

```cpp
class PingPongFbo {
public:
    void allocate(const ofFbo::Settings& settings);
    ofFbo& source();
    ofFbo& destination();
    void swap();
    void clear();
};
```

Do not expose raw swap-index assumptions throughout generators.

### 12.3 Simulation passes

Recommended separation:

1. target-body generation;
2. force calculation;
3. velocity integration;
4. position integration;
5. lifecycle update;
6. optional trail/history update.

The first prototype may combine passes, but shader responsibilities should remain conceptually documented.

### 12.4 Forces

Support composable forces:

- attraction to target body position
- attraction to pose joint
- repulsion from neighboring body regions
- global field direction
- optical-flow direction
- curl-like procedural turbulence
- event impulse
- depth-slice force
- wall-space propagation force
- return-to-body force

Each generator sets force weights rather than replacing the simulation architecture.

### 12.5 Determinism

Randomness must derive from:

```text
clip seed
scene seed
player tracking ID
point stable ID
```

Avoid frame-dependent random calls that change after performance stalls.

---

## 13. Building player point clouds

### 13.1 Initial surface sampling

For every active particle slot assigned to a player:

1. generate deterministic UV coordinates;
2. sample the player mask;
3. reject or reposition samples outside the mask;
4. sample relative depth;
5. transform crop UV into full-frame normalized coordinates;
6. convert into world coordinates;
7. derive body-region attribution from nearest pose segment or UV region;
8. store target position and confidence.

### 13.2 Point redistribution

Uniform random image-space sampling may underrepresent narrow limbs. Improve later using:

- confidence-weighted sampling
- edge sampling
- pose-segment sampling
- stratified sampling
- separate quotas for torso, arms, and legs
- distance-transform weighting

### 13.3 Surface thickness

A single depth value creates a thin shell. Controlled thickness can be introduced through:

- depth-normal noise
- body-region thickness values
- distance-to-mask-edge
- velocity-dependent displacement
- deterministic signed offset

Thickness must remain subordinate to silhouette and pose coherence.

### 13.4 Player transitions

When a new player appears:

- initialize points near the first observed volume;
- use an appearance envelope;
- optionally gather points from a field rather than spawning instantly.

When a player retires:

- stop target updates;
- transition to dissolution or field behavior;
- recycle particle slots only after the exit envelope ends.

---

## 14. Renderer architecture

### 14.1 Required render targets

At minimum:

```text
geometryColorFbo: GL_RGBA16F + depth
emissiveFbo:      GL_RGBA16F
feedbackFbo A/B:  GL_RGBA16F
bloom levels:     reduced-resolution GL_RGBA16F
finalChannelFbo:  output-compatible format
```

### 14.2 Point renderer

Features:

- point sprites or instanced quads
- camera-aware size
- depth test
- optional soft circular falloff
- luminance control
- confidence-based opacity
- stable dithering

### 14.3 Splat renderer

Features:

- instanced billboard quad
- elliptical scale
- orientation from velocity or local body direction
- soft anisotropic falloff
- depth test
- additive or premultiplied-alpha modes

### 14.4 Segment renderer

Use for:

- pose filaments
- pass or relational links
- temporal trails
- slice contours
- inter-player topology

Avoid relying on wide OpenGL lines, which can vary by platform. Prefer camera-facing quad segments or instanced line geometry.

### 14.5 Depth behavior

Support:

- depth testing
- configurable depth write
- near/far fading
- fog
- point-size attenuation
- depth slicing
- silhouette preservation

### 14.6 Camera

Provide a controlled camera abstraction rather than exposing arbitrary free-camera movement in the final show.

Camera modes:

- frontal orthographic
- shallow perspective
- limited orbit
- velocity-aligned view
- tactical overhead
- macro point-cloud view
- shared wall camera
- per-channel detail camera

Every camera transition must be time-based and deterministic.

---

## 15. HDR luminous postprocessing

### 15.1 Bloom

Pipeline:

```text
HDR emissive source
    -> bright-pass threshold
    -> downsample
    -> horizontal blur
    -> vertical blur
    -> optional additional scales
    -> weighted recomposition
```

Use at least:

- narrow bloom for point definition
- medium bloom for body volume
- broad bloom for major events

Bloom must not turn the entire cloud into an undifferentiated white mass.

### 15.2 Visual feedback

Separate simulation feedback from image feedback.

Image-feedback pass:

```text
previous rendered image
    -> decay
    -> slight transform
    -> optional blur or threshold
    -> optional spectral displacement
    -> composite current geometry
```

Parameters:

- decay
- scale
- translation
- rotation
- directional smear
- threshold
- spectral separation
- dry/wet mix
- reset

### 15.3 Tone mapping

Final stage:

- exposure
- highlight compression
- black-level control
- gamma
- output clamp
- optional subtle color response

Calibration must be performed on the actual display chain.

### 15.4 Color system

Initial palette:

- black background
- neutral white observed volume
- restrained team hues when necessary
- one prediction or uncertainty accent

Luminance should carry more information than hue.

---

## 16. Volumetric generator library

Players remain volumetric in every mode. Generators alter their state, visibility, history, connectivity, and spatial organization.

### 16.1 Scan Volume

A plane, line, or curved field traverses the player volume.

Parameters:

- scan direction
- scan thickness
- scan speed
- revealed duration
- depth bias
- point displacement
- afterglow

Variants:

- vertical scan
- horizontal scan
- diagonal scan
- radial scan
- Z-plane scan
- multiple synchronized scans

### 16.2 Temporal Echo

Player history is distributed through depth or wall space.

Parameters:

- history seconds
- sample interval
- Z spacing
- opacity decay
- color separation
- connection mode

Render options:

- point slices
- filaments by stable point ID
- pose-joint trails
- shell sequence

### 16.3 Kinetic Fragmentation

Acceleration, collision, or a directed event releases part of the player cloud.

Parameters:

- event threshold
- fragment region
- impulse direction
- dispersion
- drag
- free duration
- reconstruction strength

The original body target remains available so particles can return.

### 16.4 Spectral Dislocation

Observed state, memory, and prediction occupy distinct temporal and depth layers.

Avoid applying a generic RGB split to the entire frame. The layers should represent different semantic states.

### 16.5 Skeleton Filaments

Pose joints create nodes and luminous segments.

Possible connections:

- anatomical bones
- joint to cloud region
- joint history
- player-to-player relation
- player-to-ball relation

### 16.6 Depth Slices

Quantize the volume into Z regions.

Operations:

- isolate one slice
- animate slice order
- offset slices spatially
- assign slices across screens
- convert slices into segments
- collapse slices into a plane

### 16.7 Voxel Quantization

Quantize point positions into a spatial grid.

States:

- organic cloud
- partial voxel lock
- full quantization
- voxel fragmentation
- recovery to body

### 16.8 Formation Topology

Players remain visible while collective relationships create geometry.

Operations:

- nearest-neighbor links
- triangulation
- team hull
- centroid axes
- pressure zones
- relation planes

### 16.9 Prediction Shell

The current player is a dense observed cloud. Possible future states appear as lower-density shells or branches.

Mappings:

- confidence -> shell coherence
- probability -> opacity
- prediction horizon -> spatial separation
- prediction error -> dissolution behavior

### 16.10 Data Storm

Body points temporarily leave anatomical targets and become a shared field while retaining player and body-region attributes.

The system can reconstruct one or multiple players from the same field.

---

## 17. Generator interface

Proposed interface:

```cpp
class IVolumetricGenerator {
public:
    virtual ~IVolumetricGenerator() = default;

    virtual void setup(const VolumetricGeneratorContext& context) = 0;
    virtual void activate(const ScenePreset& preset, uint32_t seed) = 0;
    virtual void ingest(const VolumetricFrame& frame) = 0;
    virtual void trigger(const DetectedEvent& event) = 0;
    virtual void update(float deltaTime) = 0;
    virtual void configureRender(VolumetricRenderState& state) = 0;
    virtual void deactivate() = 0;
    virtual void reset() = 0;
};
```

Generators should configure forces and rendering interpretation. They should not each own a completely separate particle engine unless a measured technical reason requires it.

---

## 18. Scene composition

### 18.1 Scene state

```cpp
struct VolumetricSceneState {
    std::string generatorId;
    std::string presetId;
    TemporalStage stage;
    OrganizationMode organization;
    CameraMode cameraMode;
    uint32_t seed;
    float elapsed;
    float duration;
    float envelope;
};
```

### 18.2 Temporal lifecycle

Every scene follows:

1. emergence;
2. development;
3. threshold;
4. transformation;
5. dissolution.

The system should support transitions where particle state continues while only the force field or render interpretation changes.

### 18.3 Black and sparse states

Support:

- complete black
- one player only
- one joint only
- one depth slice
- residual trails only
- field without current observations

Do not keep all players and effects at full intensity continuously.

---

## 19. Eight-screen spatial architecture

The physical system consists of two 1920 x 1080 presentation surfaces. Each ICUIXIAN controller divides one surface into four portrait outputs.

The software should maintain three coordinate systems:

```text
player-local coordinates
channel-local coordinates
global eight-screen wall coordinates
```

### 19.1 Organization modes

#### Unison

All screens share one scene state.

#### Distributed body

One player volume spans multiple screens.

#### Polyphonic

Different cameras or generator interpretations show the same volumetric data.

#### Propagation

An impulse or scan moves through physical screen order.

#### Four plus four

```text
Screens 1-4: observation, body, surface, present
Screens 5-8: memory, depth, relation, prediction
```

#### Decomposed anatomy

Different screens show:

- full cloud
- pose
- surface points
- motion
- depth
- memory
- prediction
- relations

### 19.2 Configurability

Do not hard-code physical order. Configure:

- window group
- segment index
- global screen index
- screen rotation
- screen reversal
- global-wall transform
- inter-screen gap or bezel

---

## 20. Proposed runtime source organization

```text
src/
├── app/
│   ├── VolumetricApp.h/.cpp
│   ├── VolumetricChannel.h/.cpp
│   ├── PresentationApp.h/.cpp
│   └── ControlApp.h/.cpp
├── data/
│   ├── PdjvManifest.h/.cpp
│   ├── PdjvPackage.h/.cpp
│   ├── PdjvReader.h/.cpp
│   ├── PdjvValidator.h/.cpp
│   ├── FrameCache.h/.cpp
│   └── TimelinePlayer.h/.cpp
├── volume/
│   ├── VolumetricFrame.h
│   ├── PlayerObservation.h
│   ├── VolumetricPlayer.h/.cpp
│   ├── PointCloudBuilder.h/.cpp
│   ├── PoseRig.h/.cpp
│   └── TemporalHistory.h/.cpp
├── gpu/
│   ├── PingPongFbo.h/.cpp
│   ├── ParticleState.h
│   ├── ParticleSimulation.h/.cpp
│   ├── PointRenderer.h/.cpp
│   ├── SplatRenderer.h/.cpp
│   └── SegmentRenderer.h/.cpp
├── generators/
│   ├── IVolumetricGenerator.h
│   ├── GeneratorRegistry.h/.cpp
│   ├── ScanVolume.h/.cpp
│   ├── TemporalEcho.h/.cpp
│   ├── KineticFragmentation.h/.cpp
│   ├── SpectralDislocation.h/.cpp
│   ├── SkeletonFilaments.h/.cpp
│   ├── DepthSlices.h/.cpp
│   ├── VoxelQuantization.h/.cpp
│   ├── FormationTopology.h/.cpp
│   ├── PredictionShell.h/.cpp
│   └── DataStorm.h/.cpp
├── render/
│   ├── VolumetricCamera.h/.cpp
│   ├── HdrPipeline.h/.cpp
│   ├── Bloom.h/.cpp
│   ├── ImageFeedback.h/.cpp
│   └── ToneMapping.h/.cpp
├── direction/
│   ├── VolumetricComposer.h/.cpp
│   ├── ScenePreset.h/.cpp
│   └── TransitionDirector.h/.cpp
└── output/
    ├── WallOutput.h/.cpp
    └── PerformanceMonitor.h/.cpp
```

Do not perform this reorganization as one large uncontrolled change. Introduce directories and classes phase by phase while preserving buildability.

---

## 21. Existing code reuse plan

### Reuse or adapt

- dual-window creation in `main.cpp`
- `PresentationApp`
- `PerformanceMonitor`
- `SettingsStore`
- `OSCSender`
- `GlobalDirector` concepts
- `VisualComposer` temporal-stage and organization concepts
- control-window patterns
- wall settings

### Replace in the volumetric runtime path

- `ofVideoPlayer`-driven channel lifecycle
- runtime `CVPipeline`
- runtime `EventDetector` based on live CV
- `GraphicScore` as the principal output renderer
- `VisualGenerator` as the principal generator model
- `ClipPool` as a video-path pool

### Keep only in the legacy branch or optional hybrid tooling

- B&W video shader path
- thermal video treatment
- video squares
- video numbers
- slit scan from video
- video edge modes
- source-texture access inside generators

Do not delete legacy files until the new runtime has its own executable and the archive baseline has been verified.

---

## 22. Step-by-step implementation phases

### Phase 0 — Preserve and document the legacy application

Tasks:

1. inspect the working tree;
2. build the current project;
3. run a functional test;
4. resolve intended versus accidental local changes;
5. commit the functional baseline;
6. create archive branch and tag;
7. save a runnable build and dependency record;
8. create the volumetric branch.

Deliverables:

- legacy commit
- archive branch
- annotated tag
- build artifact
- dependency manifest
- test notes

Gate:

> Stop if the legacy application cannot be restored independently.

### Phase 1 — One-clip offline analysis proof of concept

Tasks:

1. select a representative clip;
2. detect and track one or more players;
3. export stable track IDs;
4. generate masks;
5. generate pose;
6. generate relative depth;
7. generate motion features;
8. create diagnostic previews;
9. evaluate temporal stability.

Deliverables:

- analysis script or tool
- diagnostic output
- unoptimized reference dataset
- quality report

Gate:

> At least one player remains spatially and temporally coherent enough to support volumetric reconstruction.

### Phase 2 — PDJV reference specification

Tasks:

1. define manifest schema;
2. define timestamps and coordinate conventions;
3. define frame and player records;
4. define mask and depth block references;
5. define feature flags;
6. define version rules;
7. implement exporter;
8. implement a standalone validator;
9. generate a first PDJV package.

Deliverables:

- `PDJV_FORMAT.md`
- schema examples
- exporter
- validator
- reference package

Gate:

> The package can be inspected and validated without opening the original video.

### Phase 3 — Minimal openFrameworks data runtime

Tasks:

1. create a separate volumetric runtime entry point or target;
2. load manifest;
3. load frame index;
4. play, pause, seek, and loop analysis time;
5. implement adjacent-frame cache;
6. interpolate centroids and pose;
7. expose diagnostics without video;
8. verify no video decoder is created.

Deliverables:

- PDJV reader
- timeline player
- frame cache
- diagnostic data view

Gate:

> The runtime reproduces analysis time deterministically without loading source footage.

### Phase 4 — Static volumetric-player prototype

Tasks:

1. decode one mask and depth block;
2. generate deterministic UV point samples;
3. construct XYZ positions;
4. upload point state;
5. render with a 3D camera;
6. enable depth testing;
7. implement point-size and confidence controls;
8. test one player in one portrait output.

Deliverables:

- `VolumetricPlayer`
- `PointCloudBuilder`
- point renderer
- basic camera

Gate:

> The player is legible as a spatial point cloud without any source-video pixels.

### Phase 5 — High-quality luminous renderer

Tasks:

1. introduce `GL_RGBA16F` render targets;
2. add depth buffer;
3. implement point sprites;
4. implement elliptical splats;
5. implement stable segment geometry;
6. implement bright-pass extraction;
7. implement multiscale bloom;
8. implement tone mapping;
9. implement calibration bypass views;
10. profile on the target Mac.

Deliverables:

- HDR pipeline
- point and splat renderer
- segment renderer
- bloom
- tone mapping

Gate:

> The renderer achieves the intended luminous density while preserving point and body definition.

### Phase 6 — GPU simulation and reconstruction

Tasks:

1. create ping-pong state abstraction;
2. allocate position, velocity, and attribute textures;
3. initialize deterministic particles;
4. implement attraction to body targets;
5. implement drag and turbulence;
6. implement event impulses;
7. implement age, death, and rebirth;
8. implement return-to-body behavior;
9. implement reset and resize;
10. verify no per-frame GPU readback.

Deliverables:

- particle simulation
- deterministic state initialization
- target-body reconstruction
- lifecycle control

Gate:

> The player can fragment, remain dynamic, and reconstruct without losing persistent identity.

### Phase 7 — First five volumetric generators

Implement in this order:

1. Scan Volume
2. Temporal Echo
3. Kinetic Fragmentation
4. Skeleton Filaments
5. Depth Slices

For every generator:

- define semantic input mappings;
- define force changes;
- define render changes;
- define camera compatibility;
- define lifecycle behavior;
- define reset behavior;
- define quality scaling;
- create at least three presets;
- add diagnostic mode;

Gate:

> Five distinct scenes operate on the same persistent volumetric-player representation and transition without returning to video.

### Phase 8 — Visual feedback and temporal image memory

Tasks:

1. create ping-pong image-feedback FBOs;
2. implement decay;
3. implement minimal transform;
4. implement directional smear;
5. implement threshold feedback;
6. implement semantic spectral layers;
7. provide explicit clear/freeze/carry controls;
8. ensure feedback can be disabled for calibration.

Gate:

> Feedback enriches movement without obscuring player structure or causing uncontrolled brightness accumulation.

### Phase 9 — Volumetric composition director

Tasks:

1. adapt temporal stages;
2. replace numeric generator assumptions with a registry;
3. introduce scene presets;
4. support deterministic seeds;
5. support camera presets;
6. support sparse and black states;
7. route precomputed events;
8. preserve particle state across compatible transitions;
9. synchronize OSC state.

Gate:

> The work reads as a continuous visual language rather than a sequence of technical demonstrations.

### Phase 10 — Eight-screen distribution

Tasks:

1. retain the dual-window output architecture;
2. define global wall coordinates;
3. map global space to each channel;
4. support shared and local cameras;
5. allow clouds and segments to cross boundaries;
6. implement unison, polyphonic, propagation, and four-plus-four organizations;
7. test channel order and rotation;
8. verify ICUIXIAN output behavior;
9. add calibration scene.

Gate:

> The eight outputs behave as one configurable spatial instrument.

### Phase 11 — Performance and installation hardening

Tasks:

1. create preview, installation, and diagnostic quality tiers;
2. profile particle simulation;
3. profile point and splat rendering;
4. profile bloom and feedback;
5. profile data loading and decompression;
6. implement memory limits;
7. implement cache metrics;
8. implement shader-failure reporting;
9. run long-duration stability tests;
10. test package corruption and missing frames;
11. test display disconnect and resize behavior;
12. package a signed or reproducible build.

Initial performance target:

- stable 30 fps
- two 1920 x 1080 presentation windows
- eight portrait segments
- no runtime heavy CV
- no video decoding
- no render-thread file I/O
- no per-frame GPU readback
- bounded memory use
- deterministic recovery after reset

60 fps is a later stretch target after visual density and reliability are established.

---

## 23. First vertical slice

The first end-to-end prototype should be deliberately narrow.

### Prototype name

```text
Volumetric Player / Temporal Echo
```

### Input

- one PDJV package
- one tracked player
- one mask stream
- one relative-depth stream
- pose
- stable tracking ID

### Visual behavior

- dense neutral-white point cloud
- black background
- elliptical luminous splats mixed with fine points
- shallow 3D depth
- five temporal echoes distributed in Z
- one moving scan plane
- acceleration-based particle release
- return-to-body force
- controlled bloom
- limited camera orbit

### Required controls

- point count
- point size
- splat ratio
- depth scale
- history length
- history spacing
- scan speed
- fragmentation strength
- reconstruction strength
- bloom threshold
- bloom gain
- camera angle

### Prototype acceptance criteria

- no source-video texture is loaded or rendered;
- player movement remains recognizable;
- depth feels spatial rather than like random Z noise;
- echoes remain connected to the same player;
- scan reveals structure;
- bloom preserves detail;
- particle reconstruction is stable;
- the prototype runs at the initial frame-rate target on the target Mac.

Do not begin all ten generators before this prototype passes review.

---

## 24. Performance strategy

### 24.1 Remove runtime costs

The volumetric runtime should remove:

- eight video decoders
- eight CPU computer-vision pipelines
- frame readbacks for analysis
- MOG2
- runtime Canny
- runtime Farneback unless explicitly retained as a small optional feature
- runtime segmentation
- runtime depth inference
- runtime pose inference

### 24.2 Add scalable costs

New costs:

- PDJV decompression
- mask/depth upload
- GPU state simulation
- point/splat rendering
- HDR buffers
- bloom
- feedback
- temporal history

All new costs must have quality scaling.

### 24.3 Quality tiers

```text
Preview
    low particle-state resolution
    point rendering only
    reduced bloom resolution
    short history
    one feedback pass

Installation
    calibrated particle-state resolution
    points and splats
    multiscale bloom
    full temporal behavior
    stable depth and feedback

Diagnostic
    bloom disabled
    feedback disabled
    visible IDs and bounds
    state-texture views
    cache and timing metrics
```

### 24.4 Measure separately

Track:

- package read time
- decompression time
- cache misses
- upload time
- simulation time
- geometry render time
- bloom time
- feedback time
- final composition time
- total frame time
- memory consumption

---

## 25. Error handling

### Invalid package

- show a clear control-window error;
- do not crash presentation windows;
- refuse unsupported major versions;
- identify missing files and offsets.

### Missing frame

- retain latest valid observation for a bounded duration;
- continue particle dynamics;
- reduce observation confidence;
- dissolve gracefully if the gap persists.

### Corrupt mask or depth block

- skip the invalid spatial block;
- preserve pose/centroid behavior if available;
- mark diagnostic status;
- never upload unvalidated dimensions.

### Shader failure

- log complete shader error;
- show diagnostic fallback;
- avoid presenting uninitialized GPU state.

### Resize or context recreation

- reallocate all render targets;
- clear feedback buffers;
- preserve timeline state;
- rebuild GPU resources deterministically.

---

## 26. Testing plan

### Data-format tests

- manifest parsing
- version rejection
- frame-index bounds
- corrupted offset detection
- optional-feature handling
- deterministic decode

### Timeline tests

- play/pause
- seek
- loop
- speed changes
- event single-trigger behavior
- frame interpolation
- end-of-package behavior

### Volumetric-player tests

- new player appearance
- short observation gap
- long observation loss
- tracking-ID return
- player retirement
- deterministic point identity

### GPU tests

- allocation and clear
- ping-pong swap correctness
- resize
- reset
- NaN and infinity suppression
- zero active particles
- maximum configured particles

### Renderer tests

- depth ordering
- point size
- splat orientation
- bloom bypass
- feedback reset
- tone-map calibration

### Multi-screen tests

- physical order
- orientation
- global-coordinate continuation
- four-plus-four grouping
- cross-screen propagation
- controller restart

### Long-duration test

Run the complete installation configuration long enough to evaluate:

- memory growth
- cache stability
- dropped frames
- accumulated feedback error
- timeline drift
- OSC continuity
- window stability

---

## 27. Cursor implementation rules

Cursor should follow these rules during implementation:

1. Do not modify the legacy baseline until it has been committed and tagged.
2. Do not delete existing video/CV code merely because the volumetric path replaces it.
3. Keep the project buildable at the end of every phase.
4. Introduce one architectural layer at a time.
5. Do not add an external dependency without documenting version, license, purpose, and Apple Silicon compatibility.
6. Do not assume old openFrameworks addons compile on the current toolchain.
7. Prefer openFrameworks core classes for GPU rendering.
8. Do not use OpenGL compute shaders on the target macOS path.
9. Do not perform file I/O or decompression on the render thread.
10. Do not read particle state back from the GPU during normal playback.
11. Do not pass source-video textures into volumetric generators.
12. Use deterministic seeds.
13. Express time in seconds, not only frame numbers.
14. Validate all package dimensions and offsets.
15. Add diagnostics before optimizing.
16. Profile the complete eight-output system, not only a one-window prototype.
17. Preserve explicit reset, clear, resize, and failure paths.
18. Keep generator semantics tied to analyzed football behavior.

---

## 28. Completion criteria for Volumetric Runtime V1

V1 is complete when:

- the legacy version is independently recoverable;
- the analyzer can produce validated PDJV packages;
- the installation runtime does not load source videos;
- the runtime does not execute heavy computer vision;
- players are always represented volumetrically;
- stable IDs preserve player continuity;
- point clouds use depth and pose data;
- GPU particles can fragment and reconstruct;
- HDR, bloom, feedback, and tone mapping are calibrated;
- at least five volumetric generators are production-ready;
- scene transitions preserve visual continuity;
- eight-screen composition is configurable;
- OSC and SuperCollider remain synchronized;
- 30 fps is stable under the installation configuration;
- long-duration testing shows bounded memory and no critical stalls;
- missing or corrupt analysis data fails gracefully;
- the result functions without access to original media files.

---

## 29. Reference links

### TouchDesigner technical references

- [Point Clouds — TouchDesigner documentation](https://docs.derivative.ca/Point_Clouds)  
  Describes point-cloud data represented through floating-point TOP textures, commonly using RGB for XYZ positions.

- [Point File In TOP — TouchDesigner documentation](https://docs.derivative.ca/Point_File_In_TOP)  
  Describes reading point sequences, multi-channel attributes, binary formats, pre-reading, buffering, and timeout behavior.

- [Point File In POP — TouchDesigner documentation](https://docs.derivative.ca/Point_File_In_POP)  
  Describes importing point attributes such as position, normal, color, and specialized splat fields.

- [Learning About POPs — TouchDesigner documentation](https://docs.derivative.ca/Learning_About_POPs)  
  Describes GPU point operators and their use for point clouds, particles, lines, curves, and geometry.

- [Particle POP — TouchDesigner documentation](https://docs.derivative.ca/Particle_POP)  
  Describes GPU particle state, emission, velocity, mass, drag, force, lifecycle, and feedback customization.

- [Feedback POP — TouchDesigner documentation](https://docs.derivative.ca/Feedback_POP)  
  Describes previous-frame GPU state reinjection for incremental simulation.

- [Trail POP — TouchDesigner documentation](https://docs.derivative.ca/Trail_POP)  
  Describes time-history storage and stable-ID matching for temporal trails.

- [depthProjection — TouchDesigner documentation](https://docs.derivative.ca/Palette%3AdepthProjection)  
  Describes projecting a 2D depth image into a 3D point cloud stored in a floating-point texture.

- [GLSL TOP — TouchDesigner documentation](https://docs.derivative.ca/GLSL_TOP)  
  Documents GLSL execution and notes that compute shaders require GLSL 4.30 or later.

- [Gaussian Splats — TouchDesigner documentation](https://docs.derivative.ca/Experimental%3AGaussian_Splats)  
  Describes splat attributes including position, color, rotation, scale, and appearance coefficients.

### openFrameworks technical references

- [ofVboMesh — openFrameworks documentation](https://openframeworks.cc/documentation/gl/ofVboMesh/)  
  Documents GPU-backed mesh rendering and instanced drawing.

- [ofMesh — openFrameworks documentation](https://openframeworks.cc/documentation/3d/ofMesh/)  
  Documents vertices, indices, colors, normals, texture coordinates, and mesh modes.

- [Introducing OpenGL for openFrameworks](https://openframeworks.cc/ofBook/chapters/openGL.html)  
  Covers VBOs, shaders, GPU rendering, and instancing concepts.

- [Advanced Graphics — openFrameworks ofBook](https://openframeworks.cc/ofBook/chapters/advanced_graphics.html)  
  Discusses GPU mesh storage and performance considerations.

- [openFrameworks project repository](https://github.com/openframeworks/openFrameworks)  
  Authoritative source and platform build information.

### Existing project planning reference

- `LUMINOUS_GENERATORS_SPEC.md`  
  Earlier proposal for data-driven luminous particles and segments. Its architectural separation between analysis and rendering remains useful, but the new volumetric runtime supersedes its assumption that simple 2D generators are the principal abstraction layer.

---

## 30. Final implementation statement

The volumetric version of *Partitura del Juego* transforms each source clip into a time-indexed computational body. Detection, segmentation, pose, depth, and motion are resolved before the installation. The runtime receives only their structured traces.

Each player exists as a persistent volume of points and luminous splats. The volume can be scanned, fragmented, remembered, predicted, quantized, connected, and reconstructed. Its identity survives every transformation.

The new application should therefore be understood as a volumetric score player rather than a video player: a real-time spatial instrument that performs the hidden dynamics of archived football through depth, light, movement, and distributed computation.

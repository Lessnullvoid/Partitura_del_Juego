# Luminous Generators Library

## Technical and Visual Proposal for *Partitura del Juego*

### Document purpose

This document defines a proposed library of luminous particle and line-segment generators for the *Partitura del Juego* openFrameworks application. It is intended to serve as design and implementation context for Cursor.

This is a planning specification. It does not imply that every module must be implemented at once, and it does not prescribe changes to the existing video treatment pipeline.

---

## 1. Core concept

The library creates an autonomous synthetic visual layer derived from football-video analysis.

The source video acts as a sensor and analytical input. The generators receive only numerical data extracted from the footage. They render particles, luminous segments, trails, fields, pulses, and related abstract structures.

**The original video image must never be sampled, composited, textured, or displayed by this generator layer.**

The separation is intentional:

```text
Source video
    -> tracking / optical flow / event detection
    -> normalized analysis data
    -> luminous generator
    -> particles, segments, trails, and light
```

The bodies disappear as images and remain present as forces. Their movements activate particles, their relationships produce filaments, and their decisions organize fields of light.

---

## 2. Visual direction

The desired language shares some techniques commonly found in real-time TouchDesigner work:

- additive particles
- instanced luminous segments
- temporal persistence
- vector-field advection
- controlled bloom
- fluid-like displacement
- relational networks
- energy pulses
- accumulation and dissipation

These techniques should not produce a generic particle-system aesthetic. Every visible behavior must have a legible relationship to football data, even when that relationship remains perceptually subtle.

The visual system should feel:

- precise rather than decorative
- energetic rather than illustrative
- minimal rather than spectacular by default
- data-driven rather than randomly reactive
- rhythmic rather than continuously busy
- capable of silence and near-black states

---

## 3. Architectural boundary

The generator system should consume a normalized analysis interface rather than depend directly on video players, computer-vision implementations, or clip formats.

```text
Video and analysis subsystem
    |
    v
AnalysisFrame / Event stream
    |
    v
Generator director
    |
    +-- Flow Tracers
    +-- Pass Filaments
    +-- Relational Constellations
    +-- Event Bursts
    +-- Prediction Ghosts
    +-- additional generators
    |
    v
HDR luminous renderer
    |
    v
Bloom / exposure / tone mapping
    |
    v
Channel FBO
```

This boundary allows the analysis and rendering systems to evolve independently.

### Explicit non-goals

The first version should not:

- render the source video inside a generator
- use the video as a particle texture
- derive particle color directly from video pixels
- require a specific tracking model
- duplicate the existing video-effect pipeline
- introduce physically accurate fluid simulation unless a generator requires it
- depend on an old addon without testing Apple Silicon compatibility

---

## 4. Shared analysis model

The exact C++ types can follow the conventions already used by the project. The following structures describe the required semantic contract.

### PlayerState

```cpp
struct PlayerState {
    int id;
    int team;
    glm::vec2 position;       // Normalized field or frame coordinates
    glm::vec2 velocity;
    glm::vec2 acceleration;
    float confidence;
    float activity;
    bool hasPossession;
    bool active;
};
```

### BallState

```cpp
struct BallState {
    glm::vec2 position;
    glm::vec2 velocity;
    float confidence;
    bool active;
};
```

### EventState

```cpp
enum class AnalysisEventType {
    Pass,
    Acceleration,
    DirectionChange,
    ZoneEntry,
    ClusterFormation,
    FormationBreak,
    Shot,
    PossessionChange,
    StadiumPeak,
    Custom
};

struct EventState {
    AnalysisEventType type;
    glm::vec2 origin;
    glm::vec2 destination;
    float intensity;
    float confidence;
    double timestamp;
    int sourcePlayerId;
    int targetPlayerId;
};
```

### CollectiveState

```cpp
struct CollectiveState {
    glm::vec2 centroid;
    glm::vec2 dominantDirection;
    float density;
    float spread;
    float pressure;
    float alignment;
};
```

### PredictionState

```cpp
struct PredictedPath {
    int playerId;
    std::vector<glm::vec2> points;
    float probability;
};
```

### AnalysisFrame

```cpp
struct AnalysisFrame {
    double timestamp;
    std::vector<PlayerState> players;
    BallState ball;
    CollectiveState teams[2];
    std::vector<EventState> events;
    std::vector<PredictedPath> predictions;
    float stadiumEnergy;
    float globalActivity;
};
```

All positions should be normalized before entering the generator layer. Coordinate conversion into a channel or wall output belongs to the rendering context.

---

## 5. Common visual primitives

The library should be built from four primary elements:

| Primitive | Meaning |
|---|---|
| Point | Presence, datum, individual unit |
| Segment | Direction, relationship, transfer |
| Trail | Memory, duration, trajectory |
| Field | Collective pressure, influence, ecology |

Restricting the system to these primitives will help maintain a coherent identity across multiple generators.

---

## 6. Generator interface

All generators should follow a shared lifecycle. Names and ownership can be adjusted to the existing codebase.

```cpp
class ILuminousGenerator {
public:
    virtual ~ILuminousGenerator() = default;

    virtual void setup(const GeneratorContext& context) = 0;
    virtual void activate(const GeneratorPreset& preset, uint32_t seed) = 0;
    virtual void update(const AnalysisFrame& analysis, float deltaTime) = 0;
    virtual void trigger(const EventState& event) = 0;
    virtual void render(ofFbo& target) = 0;
    virtual void deactivate() = 0;
    virtual void reset() = 0;
};
```

The interface should support deterministic seeds so that a visual sequence can be repeated during rehearsal and installation testing.

### Shared parameters

Generators should use a common parameter vocabulary where applicable:

- emission rate
- particle lifetime
- initial velocity
- drag or viscosity
- turbulence
- field strength
- segment length
- trail persistence
- luminance
- bloom contribution
- opacity
- hue or palette index
- scale
- temporal rate
- random seed
- event sensitivity
- analysis smoothing

Parameters should be serializable as presets. Generator behavior should not depend on UI state.

---

## 7. Proposed generator library

### 7.1 Flow Tracers

Particles are emitted from analyzed player positions and transported through a velocity field derived from player motion.

Inputs:

- player position
- velocity
- acceleration
- direction changes
- optional precomputed optical-flow vectors

Mappings:

- speed -> trail length
- acceleration -> luminance
- direction change -> local vortex strength
- player activity -> emission rate
- confidence -> sharpness or dispersion

Visual forms:

- fine points
- short oriented dashes
- continuous trails
- local vortices
- progressive dissipation

The video itself is not used as a texture. If optical flow is available, only its vector field is passed to the generator.

### 7.2 Player Halos

Each active player becomes an invisible energetic center that emits or displaces particles.

Mappings:

- speed -> halo radius
- acceleration -> peak brightness
- local opponent proximity -> oscillation
- possession -> pulse frequency
- sustained activity -> density

The player body remains absent. Its influence is inferred through particle behavior.

### 7.3 Pass Filaments

A pass event creates a luminous segment between its origin and destination.

Behaviors:

- progressive line growth
- directional particle transfer
- temporary afterimage
- parallel filament splitting
- speed-dependent vibration
- distance-dependent thickness
- endpoint impact
- disintegration after completion

Consecutive passes should be able to construct a temporary network across channels.

### 7.4 Relational Constellations

Players are treated as nodes in a changing spatial network.

Possible connection rules:

- nearest neighbor
- nearest three neighbors
- team-based proximity threshold
- cluster membership
- triangulation
- formation polygon
- relation to the ball
- relation across opposing teams

Lines should be born, tensioned, attenuated, and removed according to changing distances. Stable relationships may accumulate more luminance than transient ones.

### 7.5 Pressure Field

Player and team states create invisible attraction, repulsion, and directional forces. Particles reveal the field by moving through it.

Inputs:

- local density
- opponent proximity
- collective velocity
- dominant team direction
- possession changes
- concentration around the ball

The field itself is not necessarily drawn. Its structure becomes visible through particle motion.

### 7.6 Luminous Swarm

A particle population follows local agent rules influenced by the match:

- alignment
- cohesion
- separation
- attraction to the ball
- attraction to team centroids
- avoidance of saturated zones
- adaptation to collective direction

This must not be a generic flocking preset. Football analysis should continuously alter the rules and their weights.

### 7.7 Event Bursts

Discrete detected events trigger short luminous signatures.

Possible signatures:

| Event | Signature |
|---|---|
| Acceleration | Directional spray |
| Direction change | Arc or local vortex |
| Pass | Linear impulse |
| Zone entry | Expanding boundary pulse |
| Cluster formation | Inward contraction |
| Formation break | Fragmentation burst |
| Shot | High-energy focused ray |
| Possession change | Polarity or direction reversal |
| Stadium peak | Wall-wide propagation |

Signatures should form a limited and repeatable visual vocabulary.

### 7.8 Prediction Ghosts

Predicted player trajectories appear as provisional luminous structures.

Mappings:

- high confidence -> narrow, stable filament
- medium confidence -> multiple close paths
- low confidence -> dispersed particle cloud
- prediction confirmed -> increased brightness
- prediction error -> lateral dissolution

The generator should visually distinguish observed state, predicted state, and prediction error without relying on explanatory text.

### 7.9 Density Nebula

Team occupation produces a luminous particle mass without exposing individual player identities.

Mappings:

- density -> particle count
- spread -> spatial diffusion
- pressure -> turbulence
- collective speed -> drift
- alignment -> anisotropy
- activity -> luminance

The result should remain graphic and informational rather than imitate realistic smoke.

### 7.10 Temporal Comets

Each player produces a luminous head and a tail assembled from historical positions.

Possible behaviors:

- interval-based segmentation
- acceleration-driven brightness
- gaps during low-confidence tracking
- retention of direction changes only
- interaction between crossing trajectories
- gradual conversion from segments into points

### 7.11 Formation Pulse

Team formation is reduced to a small geometric vocabulary:

- convex hull or formation polygon
- longitudinal axis
- transverse axis
- centroid
- distance between tactical lines
- width and depth

The geometry can be composed of moving particles or discontinuous luminous segments. Its pulse reflects changes in collective organization.

### 7.12 Stadium Resonance

Stadium audio analysis controls a wall-wide luminous field.

Mappings:

- amplitude -> global luminance
- spectral centroid -> vertical distribution
- transient -> expanding wave
- sustained energy -> particle population
- crowd peak -> propagation across channels

Only extracted audio features enter the generator. This module should not become a conventional spectrum visualizer.

---

## 8. Multi-screen behavior

The library must treat the eight displays both as individual surfaces and as one distributed field.

### Solo

One screen presents a single generator while the remaining channels hold video, another generator, or black.

### Unison

All screens render the same generator state with spatial variations.

### Choral field

The eight screens display regions of a shared coordinate system. Particles and segments can cross screen boundaries.

### Polyphonic

Different generators interpret the same event simultaneously.

### Propagation

An impulse moves physically through the display sequence:

```text
01 -> 02 -> 03 -> 04 -> 05 -> 06 -> 07 -> 08
```

### Four plus four

The two video-wall groups can form complementary systems:

```text
Screens 1-4: observed movement / present state
Screens 5-8: relationships / memory / prediction
```

The design should not hard-code a single screen order. Wall grouping, channel mapping, physical order, and orientation must remain configurable.

---

## 9. GPU strategy for the Mac mini M4 Pro

The implementation should prioritize openFrameworks core components:

- `ofFbo`
- `ofShader`
- `ofTexture`
- `ofVboMesh`
- instanced rendering
- additive blending
- GLSL shaders

### Why not depend on compute shaders

The target platform is macOS. Its OpenGL implementation does not provide the same modern compute-shader path commonly used on Windows and Linux. The primary particle engine should therefore not require OpenGL 4.3 compute shaders or SSBO-based simulation.

### Recommended simulation model: ping-pong FBOs

Particle state can be stored in floating-point textures:

```text
State FBO A
    RG: position XY
    B: age or normalized lifetime
    A: active state or auxiliary value

State FBO B
    next-frame position state

Velocity FBO A
    RG: velocity XY
    B: seed / variation
    A: generator-specific value

Velocity FBO B
    next-frame velocity state
```

Per frame:

1. Read the previous position and velocity textures.
2. Sample normalized analysis forces.
3. Integrate the new particle state in a fragment shader.
4. Write into the alternate FBOs.
5. Swap current and previous buffers.
6. Render particles or segments using the resulting state textures.

This produces a GPU-resident simulation using features that are safer for the target Mac.

### CPU/GPU separation

CPU responsibilities:

- receive and normalize analysis
- detect or route events
- smooth low-frequency control signals
- update generator presets and state
- prepare small data arrays or data textures

GPU responsibilities:

- integrate particle motion
- sample vector fields
- render large particle populations
- generate segment geometry where practical
- accumulate trails
- apply bloom and tone mapping

Avoid reading particle state back from the GPU during normal rendering.

---

## 10. Luminous rendering pipeline

The particle library should render into an HDR or high-precision off-screen target.

```text
Particles and segments
    -> luminous HDR FBO
    -> bright-pass extraction
    -> horizontal blur
    -> vertical blur
    -> additive recomposition
    -> exposure control
    -> tone mapping / output clamp
    -> channel output FBO
```

Bloom should amplify hierarchy, not erase structure. The original particle or segment should remain visibly defined beneath its halo.

Recommended controls:

- luminance threshold
- bloom radius
- bloom gain
- source sharpness
- exposure
- output gamma
- maximum white level
- trail decay

The system should support a bloom bypass for calibration and performance testing.

---

## 11. Trails and temporal persistence

Trails can use a separate feedback FBO:

```text
previous trail frame
    -> decay
    -> optional directional displacement
    -> composite current particles
    -> next trail frame
```

Trail persistence should be generator-specific and resettable. Scene changes must be able to:

- clear immediately
- decay naturally
- freeze the current state
- hand the residual image to the following generator

The last option allows transitions where one generator emerges from the memory of another.

---

## 12. Addon evaluation

### ofxFlowTools

`ofxFlowTools` combines GLSL-based optical flow, particles, and 2D fluid simulation. It is useful as:

- an architectural reference
- a rapid visual prototype
- a source for studying advection and velocity fields
- a benchmark against a custom implementation

It should not become a required dependency until the following have been verified:

- compatibility with the project's openFrameworks version
- successful Apple Silicon compilation
- shader compatibility on macOS
- performance across eight channels
- overlap with the analysis already present in the project

### ofxPostProcessing

`ofxPostProcessing` includes bloom and shader-chain concepts. It can be evaluated for prototyping or used as a reference. A small project-specific bloom implementation may be safer for the final installation because it reduces dependencies and provides direct control over resolution, precision, and performance.

### ofxGpuParticles and similar legacy addons

Legacy GPU particle addons can provide useful examples but should not be assumed to compile unchanged with the current openFrameworks and Apple Silicon environment. Reimplementing the required ping-pong pattern with OF core classes is likely to produce a smaller and more maintainable system.

---

## 13. Color and light system

The initial library should use a restrained global palette:

- black background
- neutral white
- one color per team, when semantically necessary
- one accent color for prediction or uncertainty

Color should communicate state rather than decorate the image.

Possible color roles:

| Role | Suggested treatment |
|---|---|
| Observed data | Neutral white |
| Team A / Team B | Two restrained hues |
| Prediction | Cool or distinct accent |
| Error / divergence | Desaturated or unstable accent |
| Low confidence | Reduced saturation and opacity |
| High-energy event | Increased luminance before hue change |

Luminance should carry more information than hue because the installation depends on luminous contrast and may include screens with differing color response.

---

## 14. Rhythm and lifecycle

Generators should not remain at maximum activity continuously. Each visual state should have a compositional lifecycle:

1. **Emergence** — a datum or event introduces the system.
2. **Development** — information accumulates and relationships appear.
3. **Threshold** — density or energy reaches a critical state.
4. **Transformation** — the visual structure becomes another generator or returns to video.
5. **Dissolution** — matter decays into darkness or residual memory.

Useful duration families:

- quarter pulse
- half pulse
- one pulse
- two pulses
- four pulses
- eight pulses

The visual director should support sparse states, controlled pauses, and complete black. Darkness is an active compositional state.

---

## 15. Presets and deterministic behavior

Every generator should expose presets rather than require live adjustment of individual parameters.

Example preset model:

```cpp
struct GeneratorPreset {
    std::string id;
    std::string generatorType;
    uint32_t seed;
    float duration;
    float fadeIn;
    float fadeOut;
    float intensity;
    float particleScale;
    float timeScale;
    float trailDecay;
    float bloomGain;
    int paletteIndex;
    // Generator-specific values may be stored in a typed or validated block.
};
```

Requirements:

- presets must be serializable
- seeds must be repeatable
- transitions must not depend on frame rate
- missing analysis data must produce a stable fallback state
- generators must recover cleanly after reset or channel reconfiguration

---

## 16. Performance and quality scaling

The final particle count should be established by profiling the complete eight-screen installation, not by testing a generator in isolation.

Each generator should support quality levels such as:

```text
Preview
    reduced particle texture
    reduced bloom resolution
    short trails

Installation
    target particle texture
    calibrated bloom
    full trail behavior

Diagnostic
    no bloom
    visible field grid
    particle IDs or state colors
```

Scalable parameters:

- particle-state texture resolution
- active particle percentage
- trail FBO resolution
- bloom resolution
- blur pass count
- number of relational connections
- maximum prediction branches
- field-grid resolution

The generator system should report approximate GPU timings or, at minimum, per-frame CPU update and render timings to the existing control interface.

---

## 17. Failure and fallback behavior

The installation should remain visually stable when data is incomplete.

Cases to handle:

- no active players
- missing ball position
- low tracking confidence
- discontinuous timestamps
- clip change
- analysis stream interruption
- output resize
- generator reset

Recommended responses:

- decay gracefully instead of freezing unintentionally
- suppress emission when confidence is below threshold
- preserve existing particles only for a bounded time
- avoid spawning at coordinate origin when data is invalid
- clear buffers explicitly after resolution changes
- use a deterministic idle state when required

---

## 18. Initial implementation scope

The first production library should contain five generators:

1. **Flow Tracers**
2. **Pass Filaments**
3. **Relational Constellations**
4. **Event Bursts**
5. **Prediction Ghosts**

Shared infrastructure:

- normalized `AnalysisFrame`
- common generator interface
- event routing
- ping-pong particle-state FBOs
- instanced particle rendering
- instanced or batched segment rendering
- trail feedback FBO
- HDR luminous target
- bloom and exposure stage
- preset serialization
- deterministic random seeds
- debug visualization and performance metrics

This selection covers five distinct system behaviors:

| Generator | Function |
|---|---|
| Flow Tracers | Movement and flow |
| Pass Filaments | Transfer |
| Relational Constellations | Collective relationship |
| Event Bursts | Discrete event and rhythm |
| Prediction Ghosts | Anticipation and uncertainty |

---

## 19. Suggested development sequence

### Phase 1 — Data contract

- identify which required analysis values already exist
- define normalized coordinate and timing conventions
- build `AnalysisFrame` without changing current visual behavior
- create recorded test data for deterministic development

### Phase 2 — Rendering foundation

- create one luminous render target
- implement additive point and segment rendering
- implement calibrated bloom and exposure
- add explicit reset and resize handling

### Phase 3 — First event generators

- implement Event Bursts
- implement Pass Filaments
- verify deterministic triggering
- test transitions to and from black

### Phase 4 — Persistent particle simulation

- implement ping-pong particle state
- implement Flow Tracers
- implement trail feedback
- profile on the target Mac

### Phase 5 — Relational and predictive systems

- implement Relational Constellations
- implement Prediction Ghosts
- add uncertainty mappings

### Phase 6 — Eight-screen composition

- support local and shared wall coordinate spaces
- test continuation across screen boundaries
- implement unison, polyphonic, propagated, and four-plus-four modes
- profile the complete output configuration

### Phase 7 — Expansion

- Pressure Field
- Density Nebula
- Luminous Swarm
- Player Halos
- Formation Pulse
- Stadium Resonance

---

## 20. Acceptance criteria for the first version

The first library version is successful when:

- no source-video pixels enter the luminous render pipeline
- all visible behavior can be traced to analysis data or a deterministic compositional rule
- the same seed and analysis sequence produce a repeatable result
- generators can start, stop, reset, and resize cleanly
- at least five generator types share one interface
- particle and segment brightness remains controlled after bloom
- low-confidence or missing data does not create visual errors
- the system can render across the planned eight-channel installation
- quality can be reduced without changing the conceptual behavior
- generator activity and transitions can be directed from the existing control system

---

## 21. Final design statement

The luminous generator library translates football analysis into an ecology of energetic traces. Position becomes presence, velocity becomes direction, acceleration becomes intensity, proximity becomes connection, density becomes matter, and prediction becomes a provisional field.

The resulting images do not reproduce the match. They expose its relational dynamics through light.

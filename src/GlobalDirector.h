#pragma once
#include "ofMain.h"

// ---- Temporal phase ---------------------------------------------------------
// Slow: long ramp-down → long hold → ramp back
// Fast: short ramp-up  → short hold → ramp back
enum class TemporalPhase {
    Idle,
    SlowRampDown,
    SlowHold,
    SlowRampUp,
    FastRampUp,
    FastHold,
    FastRampDown,
};

// ---- Screen-clear phase -----------------------------------------------------
enum class ClearPhase {
    Idle,
    FadeIn,
    Hold,
    FadeOut,
};

// ---- Parameters (editable at runtime via ControlApp) -----------------------
struct GlobalDirectorParams {
    // Target speeds (multipliers applied to each channel's base speed)
    float slowSpeed       = 0.30f;   // 30 % — clearly slow, not frozen
    float fastSpeed       = 3.50f;   // 350 % — unmistakably fast

    // Slow-motion durations (seconds)
    // Short ramps give an immediate, snappy feel from the button press.
    float slowRampDownDur = 0.40f;   // snap in quickly
    float slowHoldMin     = 2.0f;
    float slowHoldMax     = 4.0f;
    float slowRampUpDur   = 0.60f;   // snap out quickly

    // Fast-forward durations (seconds)
    float fastRampUpDur   = 0.15f;   // almost instant
    float fastHoldMin     = 2.0f;    // sustain long enough to register
    float fastHoldMax     = 4.0f;
    float fastRampDownDur = 0.20f;   // snap back

    // Auto-trigger intervals (seconds)
    float slowIntervalMin  = 25.f;
    float slowIntervalMax  = 50.f;
    float fastIntervalMin  = 15.f;
    float fastIntervalMax  = 35.f;

    // Screen clear durations (seconds)
    float clearFadeInDur   = 0.20f;
    float clearHoldMin     = 0.8f;
    float clearHoldMax     = 2.0f;
    float clearFadeOutDur  = 0.45f;
    float clearIntervalMin = 30.f;
    float clearIntervalMax = 70.f;

    // Auto-triggers disabled by default: the performer decides when effects fire.
    // Enable in the Global Director panel for autonomous mode.
    bool autoSlow  = false;
    bool autoFast  = false;
    bool autoClear = false;
};

// ---- GlobalDirector ---------------------------------------------------------
// Shared by all four Channel objects.  Update() is safe to call from every
// app's update loop — internally it skips duplicate calls within the same
// real frame (dt < 1 ms guard).
class GlobalDirector {
public:
    void setup();
    void update();   // call from any/all channel apps each frame

    // Speed multiplier — Channel applies this on top of its base speed
    float   getSpeedMultiplier() const;
    bool    isSlowMo()           const { return tPhase_ == TemporalPhase::SlowRampDown
                                             || tPhase_ == TemporalPhase::SlowHold
                                             || tPhase_ == TemporalPhase::SlowRampUp; }
    bool    isFastMo()           const { return tPhase_ == TemporalPhase::FastRampUp
                                             || tPhase_ == TemporalPhase::FastHold
                                             || tPhase_ == TemporalPhase::FastRampDown; }

    // Screen-clear overlay — Channel draws this rectangle over its window
    bool    isClearActive()      const { return cPhase_ != ClearPhase::Idle; }
    float   getClearAlpha()      const { return cAlpha_; }   // 0..255
    ofColor getClearColor()      const { return cColor_; }

    // Manual triggers (e.g. from ControlApp buttons)
    void triggerSlow();
    void triggerFast();
    // Pass an explicit color to use it directly; call with no argument to cycle the palette.
    void triggerClear(ofColor color = ofColor(0, 0, 0, 0));

    GlobalDirectorParams& params()       { return p_; }
    const GlobalDirectorParams& params() const { return p_; }

    TemporalPhase temporalPhase() const { return tPhase_; }
    ClearPhase    clearPhase()    const { return cPhase_; }

private:
    // Smooth ease-in-out (Hermite)
    float ease(float t) const { return t * t * (3.f - 2.f * t); }

    GlobalDirectorParams p_;

    // Temporal state
    TemporalPhase tPhase_  = TemporalPhase::Idle;
    double        tTimer_  = 0.0;
    double        tDur_    = 0.0;
    float         tFrom_   = 1.0f;
    float         tTo_     = 1.0f;
    double        tNextSlow_ = 0.0;
    double        tNextFast_ = 0.0;

    // Clear state
    ClearPhase    cPhase_  = ClearPhase::Idle;
    double        cTimer_  = 0.0;
    double        cDur_    = 0.0;
    float         cAlpha_  = 0.f;
    ofColor       cColor_  = ofColor(0);
    int           cColorIdx_ = 0;   // cycles through clear color palette
    double        cNextClear_ = 0.0;

    // Frame-guard to prevent double-updating in multi-window setups.
    // -1 = uninitialized: setup() is called before the OF timer is ready,
    // so we defer initialization to the first update() call.
    double lastTime_ = -1.0;
};

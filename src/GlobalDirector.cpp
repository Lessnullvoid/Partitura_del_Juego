#include "GlobalDirector.h"
#include <fstream>

// Clear color palette — vibrant, high-contrast colors so the effect is unmissable
static const ofColor kClearPalette[] = {
    ofColor(220,  20,  20),   // vivid red
    ofColor(  0,   0,   0),   // black
    ofColor(255, 255, 255),   // white flash
    ofColor(  0,   0,   0),   // black
    ofColor(200,  15,  15),   // deep crimson
    ofColor(  0,   0,   0),   // black
};
static constexpr int kNumClearColors = 6;

// ---------------------------------------------------------------------------
void GlobalDirector::setup() {
    lastTime_ = ofGetElapsedTimef();

    // First triggers fire quickly so the effect is immediately testable.
    // Subsequent triggers use the longer intervals from GlobalDirectorParams.
    tNextFast_   = lastTime_ + ofRandom(4.f,  8.f);   // fast fwd in ~4-8 s
    tNextSlow_   = lastTime_ + ofRandom(12.f, 20.f);  // slow-mo after first fast
    cNextClear_  = lastTime_ + ofRandom(8.f,  16.f);  // first clear in ~8-16 s
}

// ---------------------------------------------------------------------------
void GlobalDirector::update() {
    double now = ofGetElapsedTimef();
    double dt  = now - lastTime_;
    // #region agent log
    { static long long _lastUpd = 0; long long _ms = (long long)(now*1000); if (_ms - _lastUpd > 2000) { _lastUpd = _ms; std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"E\",\"location\":\"GlobalDirector.cpp:update\",\"message\":\"update called\",\"data\":{\"now\":" << now << ",\"dt\":" << dt << ",\"dtMs\":" << (dt*1000) << ",\"dtPasses\":" << (dt>=0.0005?1:0) << ",\"tPhase\":" << (int)tPhase_ << ",\"cPhase\":" << (int)cPhase_ << ",\"tTimer\":" << tTimer_ << ",\"cTimer\":" << cTimer_ << ",\"cAlpha\":" << cAlpha_ << "},\"timestamp\":" << _ms << "}\n"; } }
    // #endregion
    if (dt < 0.0005) return;   // skip duplicate calls within same rendering frame
    lastTime_ = now;

    // ---- Auto triggers ----
    if (p_.autoSlow && tPhase_ == TemporalPhase::Idle && now >= tNextSlow_)
        triggerSlow();

    if (p_.autoFast && tPhase_ == TemporalPhase::Idle && now >= tNextFast_)
        triggerFast();

    if (p_.autoClear && cPhase_ == ClearPhase::Idle && now >= cNextClear_)
        triggerClear();

    // ---- Temporal state machine ----
    switch (tPhase_) {

        case TemporalPhase::Idle:
            break;

        case TemporalPhase::SlowRampDown:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_  = TemporalPhase::SlowHold;
                tTimer_  = 0.0;
                tDur_    = ofRandom(p_.slowHoldMin, p_.slowHoldMax);
            }
            break;

        case TemporalPhase::SlowHold:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_  = TemporalPhase::SlowRampUp;
                tTimer_  = 0.0;
                tDur_    = p_.slowRampUpDur;
                tFrom_   = p_.slowSpeed;
                tTo_     = 1.0f;
            }
            break;

        case TemporalPhase::SlowRampUp:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_      = TemporalPhase::Idle;
                tTimer_      = 0.0;
                tNextSlow_   = now + ofRandom(p_.slowIntervalMin, p_.slowIntervalMax);
                // Do not auto-reschedule fast if it already has a future time
                if (tNextFast_ < now)
                    tNextFast_ = now + ofRandom(p_.fastIntervalMin, p_.fastIntervalMax);
            }
            break;

        case TemporalPhase::FastRampUp:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_  = TemporalPhase::FastHold;
                tTimer_  = 0.0;
                tDur_    = ofRandom(p_.fastHoldMin, p_.fastHoldMax);
            }
            break;

        case TemporalPhase::FastHold:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_  = TemporalPhase::FastRampDown;
                tTimer_  = 0.0;
                tDur_    = p_.fastRampDownDur;
                tFrom_   = p_.fastSpeed;
                tTo_     = 1.0f;
            }
            break;

        case TemporalPhase::FastRampDown:
            tTimer_ += dt;
            if (tTimer_ >= tDur_) {
                tPhase_      = TemporalPhase::Idle;
                tTimer_      = 0.0;
                tNextFast_   = now + ofRandom(p_.fastIntervalMin, p_.fastIntervalMax);
                if (tNextSlow_ < now)
                    tNextSlow_ = now + ofRandom(p_.slowIntervalMin, p_.slowIntervalMax);
            }
            break;
    }

    // ---- Screen-clear state machine ----
    switch (cPhase_) {

        case ClearPhase::Idle:
            break;

        case ClearPhase::FadeIn:
            cTimer_ += dt;
            cAlpha_  = ease((float)(cTimer_ / cDur_)) * 255.f;
            if (cTimer_ >= cDur_) {
                cAlpha_  = 255.f;
                cPhase_  = ClearPhase::Hold;
                cTimer_  = 0.0;
                cDur_    = ofRandom(p_.clearHoldMin, p_.clearHoldMax);
            }
            break;

        case ClearPhase::Hold:
            cTimer_ += dt;
            if (cTimer_ >= cDur_) {
                cPhase_  = ClearPhase::FadeOut;
                cTimer_  = 0.0;
                cDur_    = p_.clearFadeOutDur;
            }
            break;

        case ClearPhase::FadeOut:
            cTimer_ += dt;
            cAlpha_  = (1.0f - ease((float)(cTimer_ / cDur_))) * 255.f;
            if (cTimer_ >= cDur_) {
                cAlpha_       = 0.f;
                cPhase_       = ClearPhase::Idle;
                cNextClear_   = now + ofRandom(p_.clearIntervalMin, p_.clearIntervalMax);
            }
            break;
    }
}

// ---------------------------------------------------------------------------
float GlobalDirector::getSpeedMultiplier() const {
    float t = (tDur_ > 0.0) ? ofClamp((float)(tTimer_ / tDur_), 0.f, 1.f) : 1.f;
    switch (tPhase_) {
        case TemporalPhase::Idle:         return 1.0f;
        case TemporalPhase::SlowRampDown: return ofLerp(1.0f,         p_.slowSpeed, ease(t));
        case TemporalPhase::SlowHold:     return p_.slowSpeed;
        case TemporalPhase::SlowRampUp:   return ofLerp(p_.slowSpeed, 1.0f,         ease(t));
        case TemporalPhase::FastRampUp:   return ofLerp(1.0f,         p_.fastSpeed, ease(t));
        case TemporalPhase::FastHold:     return p_.fastSpeed;
        case TemporalPhase::FastRampDown: return ofLerp(p_.fastSpeed, 1.0f,         ease(t));
    }
    return 1.0f;
}

// ---------------------------------------------------------------------------
void GlobalDirector::triggerSlow() {
    // #region agent log
    { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"GlobalDirector.cpp:triggerSlow\",\"message\":\"triggerSlow entered\",\"data\":{\"tPhase\":" << (int)tPhase_ << ",\"accepted\":" << (tPhase_==TemporalPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
    // #endregion
    if (tPhase_ != TemporalPhase::Idle) return;
    tPhase_ = TemporalPhase::SlowRampDown;
    tTimer_ = 0.0;
    tDur_   = p_.slowRampDownDur;
    tFrom_  = 1.0f;
    tTo_    = p_.slowSpeed;
    ofLogNotice("GlobalDirector") << "SLOW triggered";
}

void GlobalDirector::triggerFast() {
    // #region agent log
    { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"GlobalDirector.cpp:triggerFast\",\"message\":\"triggerFast entered\",\"data\":{\"tPhase\":" << (int)tPhase_ << ",\"accepted\":" << (tPhase_==TemporalPhase::Idle?1:0) << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
    // #endregion
    if (tPhase_ != TemporalPhase::Idle) return;
    tPhase_ = TemporalPhase::FastRampUp;
    tTimer_ = 0.0;
    tDur_   = p_.fastRampUpDur;
    tFrom_  = 1.0f;
    tTo_    = p_.fastSpeed;
    ofLogNotice("GlobalDirector") << "FAST triggered";
}

void GlobalDirector::triggerClear(ofColor color) {
    // #region agent log
    { std::ofstream _f("/Users/microhm/Desktop/01_Proyectos/Partitura_del_Juego/.cursor/debug-b4e03e.log", std::ios::app); _f << "{\"sessionId\":\"b4e03e\",\"hypothesisId\":\"B\",\"location\":\"GlobalDirector.cpp:triggerClear\",\"message\":\"triggerClear entered\",\"data\":{\"cPhase\":" << (int)cPhase_ << ",\"accepted\":" << (cPhase_==ClearPhase::Idle?1:0) << ",\"r\":" << (int)color.r << ",\"g\":" << (int)color.g << ",\"b\":" << (int)color.b << "},\"timestamp\":" << (long long)(ofGetElapsedTimef()*1000) << "}\n"; }
    // #endregion
    if (cPhase_ != ClearPhase::Idle) return;

    // If caller didn't specify a color, cycle through the palette
    if (color == ofColor(0) && color.a == 255) {
        cColor_ = kClearPalette[cColorIdx_ % kNumClearColors];
        cColorIdx_++;
    } else {
        cColor_ = color;
    }

    cPhase_  = ClearPhase::FadeIn;
    cTimer_  = 0.0;
    cDur_    = p_.clearFadeInDur;
    cAlpha_  = 0.f;
    ofLogNotice("GlobalDirector") << "CLEAR triggered ("
        << (int)cColor_.r << "," << (int)cColor_.g << "," << (int)cColor_.b << ")";
}

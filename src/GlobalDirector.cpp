#include "GlobalDirector.h"

// Paleta de clear — instalación monocroma, así que el efecto opera por
// escalones de exposición, no por tono.
static const ofColor kClearPalette[] = {
    ofColor(255, 255, 255),   // destello blanco
    ofColor(  0,   0,   0),   // negro
    ofColor(168, 168, 168),   // media exposición
    ofColor(  0,   0,   0),   // negro
    ofColor(255, 255, 255),   // destello blanco
    ofColor(  0,   0,   0),   // negro
};
static constexpr int kNumClearColors = 6;

// ---------------------------------------------------------------------------
void GlobalDirector::setup() {
    // Vacío a propósito: ofGetElapsedTimef() aún no es fiable cuando main()
    // llama a setup() antes de ofRunMainLoop().  El tiempo se inicializa en la
    // primera llamada a update(), cuando el temporizador OF ya está en marcha.
}

// ---------------------------------------------------------------------------
void GlobalDirector::update() {
    double now = ofGetElapsedTimef();

    // Primera llamada: inicializar el tiempo cuando el temporizador OF es estable.
    if (lastTime_ < 0.0) {
        lastTime_   = now;
        tNextFast_  = now + ofRandom(4.f,  8.f);
        tNextSlow_  = now + ofRandom(12.f, 20.f);
        cNextClear_ = now + ofRandom(8.f,  16.f);
        return;
    }

    double dt = now - lastTime_;
    if (dt < 0.0005) return;   // omitir llamadas duplicadas en el mismo fotograma de render
    lastTime_ = now;

    // ---- Auto-disparos ----
    if (p_.autoSlow && tPhase_ == TemporalPhase::Idle && now >= tNextSlow_)
        triggerSlow();

    if (p_.autoFast && tPhase_ == TemporalPhase::Idle && now >= tNextFast_)
        triggerFast();

    if (p_.autoClear && cPhase_ == ClearPhase::Idle && now >= cNextClear_)
        triggerClear();

    // ---- Máquina de estados temporal ----
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
                // No reprogramar fast automáticamente si ya tiene un tiempo futuro
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

    // ---- Máquina de estados de clear de pantalla ----
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
    if (tPhase_ != TemporalPhase::Idle) return;
    tPhase_ = TemporalPhase::SlowRampDown;
    tTimer_ = 0.0;
    tDur_   = p_.slowRampDownDur;
    tFrom_  = 1.0f;
    tTo_    = p_.slowSpeed;
    ofLogNotice("GlobalDirector") << "SLOW triggered";
}

void GlobalDirector::triggerFast() {
    if (tPhase_ != TemporalPhase::Idle) return;
    tPhase_ = TemporalPhase::FastRampUp;
    tTimer_ = 0.0;
    tDur_   = p_.fastRampUpDur;
    tFrom_  = 1.0f;
    tTo_    = p_.fastSpeed;
    ofLogNotice("GlobalDirector") << "FAST triggered";
}

void GlobalDirector::triggerClear(ofColor color) {
    if (cPhase_ != ClearPhase::Idle) return;

    // alpha == 0 es el centinela de "sin color indicado — recorrer la paleta"
    if (color.a == 0) {
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

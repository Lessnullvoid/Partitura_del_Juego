#pragma once
#include "ofMain.h"

// ---- Fase temporal ----------------------------------------------------------
// Slow: rampa descendente larga → hold largo → rampa de retorno
// Fast: rampa ascendente corta → hold corto → rampa de retorno
enum class TemporalPhase {
    Idle,
    SlowRampDown,
    SlowHold,
    SlowRampUp,
    FastRampUp,
    FastHold,
    FastRampDown,
};

// ---- Fase de clear de pantalla ----------------------------------------------
enum class ClearPhase {
    Idle,
    FadeIn,
    Hold,
    FadeOut,
};

// ---- Parámetros (editables en runtime vía ControlApp) ----------------------
struct GlobalDirectorParams {
    // Velocidades objetivo (multiplicadores sobre la velocidad base de cada canal)
    float slowSpeed       = 0.30f;   // 30 % — claramente lento, no congelado
    float fastSpeed       = 3.50f;   // 350 % — inequívocamente rápido

    // Duraciones de cámara lenta (segundos)
    // Rampas cortas dan una sensación inmediata y seca desde el botón.
    float slowRampDownDur = 0.40f;   // entrada rápida
    float slowHoldMin     = 2.0f;
    float slowHoldMax     = 4.0f;
    float slowRampUpDur   = 0.60f;   // salida rápida

    // Duraciones de avance rápido (segundos)
    float fastRampUpDur   = 0.15f;   // casi instantáneo
    float fastHoldMin     = 2.0f;    // sostener lo bastante para registrarse
    float fastHoldMax     = 4.0f;
    float fastRampDownDur = 0.20f;   // retorno seco

    // Intervalos de auto-disparo (segundos)
    float slowIntervalMin  = 25.f;
    float slowIntervalMax  = 50.f;
    float fastIntervalMin  = 15.f;
    float fastIntervalMax  = 35.f;

    // Duraciones de clear de pantalla (segundos)
    float clearFadeInDur   = 0.20f;
    float clearHoldMin     = 0.8f;
    float clearHoldMax     = 2.0f;
    float clearFadeOutDur  = 0.45f;
    float clearIntervalMin = 30.f;
    float clearIntervalMax = 70.f;

    // Auto-disparos desactivados por defecto: el intérprete decide cuándo saltan los efectos.
    // Activar en el panel Global Director para modo autónomo.
    bool autoSlow  = false;
    bool autoFast  = false;
    bool autoClear = false;
};

// ---- GlobalDirector ---------------------------------------------------------
// Compartido por los cuatro objetos Channel.  Update() es seguro de llamar
// desde el bucle update de cada app — internamente omite llamadas duplicadas
// en el mismo fotograma real (guarda dt < 1 ms).
class GlobalDirector {
public:
    void setup();
    void update();   // llamar desde cualquiera/todas las apps de canal cada fotograma

    // Multiplicador de velocidad — Channel lo aplica encima de su velocidad base
    float   getSpeedMultiplier() const;
    bool    isSlowMo()           const { return tPhase_ == TemporalPhase::SlowRampDown
                                             || tPhase_ == TemporalPhase::SlowHold
                                             || tPhase_ == TemporalPhase::SlowRampUp; }
    bool    isFastMo()           const { return tPhase_ == TemporalPhase::FastRampUp
                                             || tPhase_ == TemporalPhase::FastHold
                                             || tPhase_ == TemporalPhase::FastRampDown; }

    // Overlay de clear de pantalla — Channel dibuja este rectángulo sobre su ventana
    bool    isClearActive()      const { return cPhase_ != ClearPhase::Idle; }
    float   getClearAlpha()      const { return cAlpha_; }   // 0..255
    ofColor getClearColor()      const { return cColor_; }

    // Disparos manuales (p. ej. desde botones de ControlApp)
    void triggerSlow();
    void triggerFast();
    // Pasar un color explícito para usarlo directo; sin argumento recorre la paleta.
    void triggerClear(ofColor color = ofColor(0, 0, 0, 0));

    GlobalDirectorParams& params()       { return p_; }
    const GlobalDirectorParams& params() const { return p_; }

    TemporalPhase temporalPhase() const { return tPhase_; }
    ClearPhase    clearPhase()    const { return cPhase_; }

private:
    // Ease-in-out suave (Hermite)
    float ease(float t) const { return t * t * (3.f - 2.f * t); }

    GlobalDirectorParams p_;

    // Estado temporal
    TemporalPhase tPhase_  = TemporalPhase::Idle;
    double        tTimer_  = 0.0;
    double        tDur_    = 0.0;
    float         tFrom_   = 1.0f;
    float         tTo_     = 1.0f;
    double        tNextSlow_ = 0.0;
    double        tNextFast_ = 0.0;

    // Estado de clear
    ClearPhase    cPhase_  = ClearPhase::Idle;
    double        cTimer_  = 0.0;
    double        cDur_    = 0.0;
    float         cAlpha_  = 0.f;
    ofColor       cColor_  = ofColor(0);
    int           cColorIdx_ = 0;   // recorre la paleta de color de clear
    double        cNextClear_ = 0.0;

    // Guarda de fotograma para evitar doble actualización en setups multi-ventana.
    // -1 = sin inicializar: setup() se llama antes de que el temporizador OF esté listo,
    // así que se aplaza la inicialización a la primera llamada a update().
    double lastTime_ = -1.0;
};

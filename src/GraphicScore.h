#pragma once
#include "ofMain.h"
#include "CVPipeline.h"
#include <deque>

enum class ScoreMode {
    BwClean    = 0,   // Solo base B&W — respiro visual
    ScanLine,
    BBoxTracker,
    BinaryText,
    Waveform,
    GridData,
    Barcode,
    VideoNormal,    // vídeo en color original, fotograma completo
    VideoSquares,   // base B&W + cuadrados en color en las posiciones de blob
    VideoNumbers,   // vídeo expresado como una cuadrícula de dígitos de brillo (0-9)
    VideoLines,     // vídeo expresado como dibujo de líneas de contorno/borde
    ThermalVision,  // mapa de color térmico Ironbow / FLIR sobre la luma B&W
    SlitScan,       // slit-scan temporal: el tiempo se desplaza en horizontal, y = espacial
    Flash,
    COUNT
};

struct DatamaticsParams {
    // Imagen base B&W (pasada al shader)
    float bwThreshold  = 0.45f;
    float posterize    = 6.f;
    float brightness   = 0.0f;
    float contrast     = 1.2f;
    float gamma        = 0.85f;

    // Temporización de ScoreDirector
    float minModeDuration = 4.f;
    float maxModeDuration = 9.f;
    float flashDuration   = 0.25f;

    // Opacidad de capa
    float markOpacity  = 0.9f;

    // Color de marca — cicla blanco / rojo / azul eléctrico en cada cambio de modo
    ofColor markColor = ofColor(255);

    // Parámetros de aspecto fílmico
    float grain    = 0.035f;   // grano analógico sutil
    float vignette = 0.32f;    // oscurecimiento de bordes
    float sCurve   = 0.55f;    // intensidad de la curva S tonal fílmica

    // Forma de onda
    int   waveformHistory = 1920;

    // Rejilla
    int   gridCols = 32;
    int   gridRows = 48;

    // Scanline
    int   scanStep = 4;

    // Ajuste de VideoSquares
    float videoSquareSize  = 220.f;  // lado de cada cuadrado de vídeo en color, en px
    int   videoSquareCount = 6;      // máximo de cuadrados dibujados a la vez

    // Slit-scan
    int   slitInterval = 20;  // fotogramas entre capturas (~0.67 s a 30 fps)

    // Forzado manual (-1 = auto)
    int   forcedMode = -1;
};

class GraphicScore {
public:
    void setup(int w, int h);
    // bwTex  = vídeo procesado en B&W (desde el FBO de visualización + shader)
    // rawTex = fotograma de vídeo en color original (desde el player)
    void update(const ofTexture& bwTex, const ofTexture& rawTex, CVPipeline& cv);
    // Presenta un generador independiente o un fotograma de respiración
    // por el mismo FBO de salida.
    void updateExternal(const ofTexture* texture, float opacity = 1.f);
    // blackLevelCrush > 0 activa un paso de niveles en mono.frag que mapea
    // [blackLevelCrush, 1] -> [0, 1], aplastando el brillo de fondo a negro puro.
    // Usar ~0.07 antes de un blend-invert (applyPolarity) para obtener blanco verdadero.
    void draw(int x, int y, int w, int h, float blackLevelCrush = 0.f);

    void onCollision();
    void onClipChange();  // dispara un estroboscopio breve blanco/rojo superpuesto

    ScoreMode         currentMode() const { return (preFlashMode_ >= 0) ? ScoreMode::Flash : director_; }
    DatamaticsParams& params()            { return params_; }
    ofFbo&            getFbo()            { return fbo_; }

private:
    void advanceDirector(double dt);
    void buildSequence();

    // Renderizadores de modo (todos dibujan en el FBO actualmente vinculado)
    void renderBase(const ofTexture& videoTex);
    void renderScanLine(CVPipeline& cv);
    void renderBBoxTracker(CVPipeline& cv);
    void renderBinaryText(CVPipeline& cv);
    void renderWaveform();
    void renderGridData(CVPipeline& cv);
    void renderBarcode(CVPipeline& cv);
    void renderVideoNormal();
    void renderVideoSquares(CVPipeline& cv);
    void renderVideoNumbers(CVPipeline& cv);
    void renderVideoLines(CVPipeline& cv);
    void renderThermal(CVPipeline& cv);   // mapa de color térmico Ironbow
    void renderSlitScan();                // slit-scan temporal ping-pong
    void updateSlitFbo(CVPipeline& cv);   // se llama antes de fbo_.begin() de la partitura
    void renderFlash();

    // HUD de datos persistente dibujado encima de todos los modos
    void renderDataHUD(const CVData& data);

    std::string toBinary8(int v);

    // Ayudas de recorte cover para la textura de vídeo original
    struct CoverLayout { float ox, oy, dw, dh, scale; };
    CoverLayout coverLayout() const;

    // Elige el siguiente color de marca (ciclo blanco → rojo → azul)
    static ofColor nextMarkColor(const ofColor& cur);

    ofFbo    fbo_;
    ofShader bwShader_;
    ofShader thermalShader_;   // mapa de color térmico Ironbow
    ofShader monoShader_;      // puerta de escala de grises aplicada a la salida
    ofTrueTypeFont monoFont_;  // texto denso pequeño (filas binarias)
    ofTrueTypeFont labelFont_; // etiquetas grandes, IDs de blob, números
    ofImage        edgesImg_;  // se reutiliza cada fotograma para VideoLines

    // Slit-scan: cinta continua de columnas (slit-scan clásico Form+Code)
    cv::Mat  slitRibbonMat_;   // buffer circular, una columna por fotograma, aW de ancho
    int      slitWriteX_ = 0;
    float    slitSrcXNorm_   = 0.5f;  // posición suavizada de la columna de muestreo (0..1), sigue el movimiento
    float    slitStepAccum_  = 0.f;   // acumulador fraccionario para escrituras a velocidad variable

    // Slit-scan: superposición estroboscópica de fantasmas (instantes congelados)
    static constexpr int kSlitLayers = 7;    // instantes congelados mostrados a la vez
    cv::Mat  slitFrames_[kSlitLayers];       // buffer circular de fotogramas grises completos
    int      slitLayerWrite_   = 0;          // siguiente hueco de escritura
    int      slitLayersFilled_ = 0;          // huecos con datos válidos (0..kSlitLayers)
    int      slitFrameCount_   = 0;          // fotogramas transcurridos desde la última captura
    cv::Mat  slitGhostMat_;   // capa fantasma compuesta en caché (se recalcula al capturar)

    bool     slitReady_ = false;
    ofImage  slitImage_;      // compuesto final cinta+fantasma subido cada fotograma

    DatamaticsParams params_;
    int  w_ = 0, h_ = 0;

    // Punteros de textura cacheados por fotograma (se fijan en update, los usan los renderizadores)
    const ofTexture* curBwTex_  = nullptr;
    const ofTexture* curRawTex_ = nullptr;

    // Superposición estroboscópica al cambio de clip (se desvanece rápido sobre el modo actual)
    float   strobeAlpha_ = 0.f;
    ofColor strobeColor_ = ofColor(255);

    // Estado de ScoreDirector
    ScoreMode director_     = ScoreMode::ScanLine;
    int       seqIdx_       = 0;
    double    modeTimer_    = 0.0;
    double    modeDuration_ = 5.0;
    int       preFlashMode_ = -1;  // -1 = no está en flash
    double    flashTimer_   = 0.0;
    std::vector<ScoreMode> sequence_;

    // Historial de forma de onda
    std::deque<float> energyHistory_;

    double lastTime_ = 0.0;
};

#pragma once
#include "ofMain.h"
#include "Channel.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "GlobalDirector.h"
#include "VideoDirector.h"
#include "PerformanceMonitor.h"
#include <array>

// App de presentación para una salida de controlador: renderiza cuatro canales
// en segmentos iguales. Dos instancias cubren los canales 0-3 y 4-7 en dualWindow8.
// Strips4x1 divide la ventana en cuatro tiras verticales (muro vertical, rotación
// ICUIXIAN 90 deg). Grid2x2 divide la ventana en una rejilla 2x2 (muro apaisado,
// sin rotación ICUIXIAN).
class PresentationApp : public ofBaseApp {
public:
    static constexpr int kSegments = 4;

    enum class WallLayout {
        Strips4x1,  // cuatro tiras verticales, una por panel vertical (por defecto)
        Grid2x2     // rejilla 2x2, un cuadrante por panel apaisado
    };

    PresentationApp(const std::array<Channel*, kSegments>& channels,
                    int channelOffset, int totalW, int totalH,
                    ClipPool* pool, OSCSender* osc, const CVParams& cvp,
                    GlobalDirector* dir = nullptr, VideoDirector* videoDir = nullptr,
                    VisualComposer* composer = nullptr,
                    PerformanceMonitor* performance = nullptr,
                    int performanceWindow = 0, int targetFps = 30,
                    WallLayout layout = WallLayout::Strips4x1);

    void setup()  override;
    void update() override;
    void draw()   override;

private:
    std::array<Channel*, kSegments> channels_;
    int             channelOffset_;
    int             totalW_, totalH_, segW_, segH_;
    WallLayout      layout_;
    ClipPool*       pool_;
    OSCSender*      osc_;
    CVParams        cvp_;
    GlobalDirector* dir_;
    VideoDirector*  videoDir_;
    VisualComposer* composer_;
    PerformanceMonitor* performance_;
    int performanceWindow_;
    int targetFps_;
};

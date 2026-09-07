#include "ofMain.h"
#include "ofApp.h"
#include "ControlApp.h"
#include "PresentationApp.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "Channel.h"
#include "GlobalDirector.h"
#include "SettingsStore.h"
#include "VideoDirector.h"
#include "VisualComposer.h"
#include "VideoPointCloudGenerator.h"
#include <CoreGraphics/CoreGraphics.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits.h>

namespace {
void configureBundledDataPath() {
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (!bundle) return;

    CFURLRef resourcesUrl = CFBundleCopyResourcesDirectoryURL(bundle);
    if (!resourcesUrl) return;

    UInt8 resourcesPath[PATH_MAX];
    bool resolved = CFURLGetFileSystemRepresentation(
        resourcesUrl, true, resourcesPath, sizeof(resourcesPath));
    CFRelease(resourcesUrl);
    if (!resolved) return;

    std::string dataRoot =
        std::string(reinterpret_cast<const char*>(resourcesPath)) + "/data/";
    if (ofDirectory::doesDirectoryExist(dataRoot))
        ofSetDataPathRoot(dataRoot);
}
}

int main() {
    // Antes de que exista ninguna ventana, openFrameworks aún no ha seleccionado
    // el directorio de datos del bundle. Se resuelve explícitamente para lanzamientos desde Finder.
    configureBundledDataPath();

    std::string settingsError;
    ofJson cfg = SettingsStore::load(&settingsError);
    if (!cfg.is_object()) {
        ofLogError("main") << "settings.json is missing or invalid; using defaults";
        cfg = ofJson::object();
    } else if (!settingsError.empty()) {
        ofLogWarning("main") << settingsError;
    }
    // OSC
    std::string oscHost = cfg.value("osc/host", "localhost");
    if (cfg.contains("osc") && cfg["osc"].contains("host"))
        oscHost = cfg["osc"]["host"].get<std::string>();
    int oscPort = 9001;
    if (cfg.contains("osc") && cfg["osc"].contains("port"))
        oscPort = cfg["osc"]["port"].get<int>();
    if (const char* port = std::getenv("PDJ_OSC_PORT"))
        oscPort = std::max(1, std::atoi(port));
    int oscListenPort = 9002;
    if (cfg.contains("osc") && cfg["osc"].contains("listenPort"))
        oscListenPort = cfg["osc"]["listenPort"].get<int>();

    // Parámetros CV
    CVParams cvp;
    if (cfg.contains("cv")) {
        const auto& c = cfg["cv"];
        cvp.halfRes        = c.value("halfRes",        cvp.halfRes);
        cvp.analysisEveryNFrames =
            c.value("analysisEveryNFrames", cvp.analysisEveryNFrames);
        cvp.flowWindowSize = c.value("flowWindowSize", cvp.flowWindowSize);
        cvp.blobMinArea    = c.value("blobMinArea",    cvp.blobMinArea);
        cvp.blobMaxArea    = c.value("blobMaxArea",    cvp.blobMaxArea);
        cvp.bgSubHistory   = c.value("bgSubHistory",   cvp.bgSubHistory);
        cvp.bgSubThreshold = c.value("bgSubThreshold", cvp.bgSubThreshold);
        cvp.useCLAHE       = c.value("useCLAHE",       cvp.useCLAHE);
        cvp.claheClipLimit = c.value("claheClipLimit", cvp.claheClipLimit);
        cvp.bwContrast     = c.value("bwContrast",     cvp.bwContrast);
        cvp.bwBrightness   = c.value("bwBrightness",   cvp.bwBrightness);
        cvp.bwGamma        = c.value("bwGamma",        cvp.bwGamma);
        cvp.bwSCurve       = c.value("bwSCurve",       cvp.bwSCurve);
        cvp.bwGrain        = c.value("bwGrain",        cvp.bwGrain);
        cvp.bwVignette     = c.value("bwVignette",     cvp.bwVignette);
        cvp.bwThreshold    = c.value("bwThreshold",    cvp.bwThreshold);
        cvp.bwPosterize    = c.value("bwPosterize",    cvp.bwPosterize);
    }

    VideoDirectorParams videoParams;
    if (cfg.contains("videoDirector")) {
        const auto& v = cfg["videoDirector"];
        videoParams.enabled           = v.value("enabled", videoParams.enabled);
        videoParams.sharedEventsEnabled =
            v.value("sharedEventsEnabled", videoParams.sharedEventsEnabled);
        videoParams.shortWeight       = v.value("shortWeight", videoParams.shortWeight);
        videoParams.longWeight        = v.value("longWeight", videoParams.longWeight);
        videoParams.fullWeight        = v.value("fullWeight", videoParams.fullWeight);
        videoParams.shortMin          = v.value("shortMin", videoParams.shortMin);
        videoParams.shortMax          = v.value("shortMax", videoParams.shortMax);
        videoParams.longMin           = v.value("longMin", videoParams.longMin);
        videoParams.longMax           = v.value("longMax", videoParams.longMax);
        videoParams.sharedIntervalMin = v.value("sharedIntervalMin", videoParams.sharedIntervalMin);
        videoParams.sharedIntervalMax = v.value("sharedIntervalMax", videoParams.sharedIntervalMax);
        videoParams.sharedStartDelay  = v.value("sharedStartDelay", videoParams.sharedStartDelay);
        videoParams.driftTolerance    = v.value("driftTolerance", videoParams.driftTolerance);
    }

    VisualComposerParams composerParams;
    if (cfg.contains("visualComposer")) {
        const auto& c = cfg["visualComposer"];
        composerParams.enabled = c.value("enabled", composerParams.enabled);
        composerParams.programEnabled =
            c.value("programEnabled", composerParams.programEnabled);
        composerParams.pulseMomentDuration =
            c.value("pulseMomentDuration", composerParams.pulseMomentDuration);
        composerParams.barScanMomentDuration =
            c.value("barScanMomentDuration", composerParams.barScanMomentDuration);
        composerParams.maxVideoPerGroup =
            ofClamp(c.value("maxVideoPerGroup",
                            composerParams.maxVideoPerGroup), 1, 2);
        composerParams.dualVideoProbability =
            c.value("dualVideoProbability",
                    composerParams.dualVideoProbability);
        composerParams.takeoverIntervalMin =
            c.value("takeoverIntervalMin",
                    composerParams.takeoverIntervalMin);
        composerParams.takeoverIntervalMax =
            c.value("takeoverIntervalMax",
                    composerParams.takeoverIntervalMax);
        composerParams.takeoverDurationMin =
            c.value("takeoverDurationMin",
                    composerParams.takeoverDurationMin);
        composerParams.takeoverDurationMax =
            c.value("takeoverDurationMax",
                    composerParams.takeoverDurationMax);
        composerParams.videoProbability =
            c.value("videoProbability", composerParams.videoProbability);
        composerParams.generatorProbability =
            c.value("generatorProbability", composerParams.generatorProbability);
        composerParams.breathProbability =
            c.value("breathProbability", composerParams.breathProbability);
        composerParams.transitionProbability =
            c.value("transitionProbability", composerParams.transitionProbability);
        composerParams.videoMinDuration =
            c.value("videoMinDuration", composerParams.videoMinDuration);
        composerParams.videoMaxDuration =
            c.value("videoMaxDuration", composerParams.videoMaxDuration);
        composerParams.generatorMinDuration =
            c.value("generatorMinDuration", composerParams.generatorMinDuration);
        composerParams.generatorMaxDuration =
            c.value("generatorMaxDuration", composerParams.generatorMaxDuration);
        composerParams.breathMinDuration =
            c.value("breathMinDuration", composerParams.breathMinDuration);
        composerParams.breathMaxDuration =
            c.value("breathMaxDuration", composerParams.breathMaxDuration);
        composerParams.transitionMinDuration =
            c.value("transitionMinDuration", composerParams.transitionMinDuration);
        composerParams.transitionMaxDuration =
            c.value("transitionMaxDuration", composerParams.transitionMaxDuration);
        composerParams.noiseEventEnabled =
            c.value("noiseEventEnabled",  composerParams.noiseEventEnabled);
        composerParams.noiseEventInterval =
            c.value("noiseEventInterval", composerParams.noiseEventInterval);
        composerParams.noiseEventDuration =
            c.value("noiseEventDuration", composerParams.noiseEventDuration);
        composerParams.generatorInvertEnabled =
            c.value("generatorInvertEnabled",  composerParams.generatorInvertEnabled);
        composerParams.generatorInvertInterval =
            c.value("generatorInvertInterval", composerParams.generatorInvertInterval);
        composerParams.generatorInvertDuration =
            c.value("generatorInvertDuration", composerParams.generatorInvertDuration);
        composerParams.polarityInvertEnabled =
            c.value("polarityInvertEnabled",  composerParams.polarityInvertEnabled);
        composerParams.polarityInvertInterval =
            c.value("polarityInvertInterval", composerParams.polarityInvertInterval);
        composerParams.polarityInvertDuration =
            c.value("polarityInvertDuration", composerParams.polarityInvertDuration);
        composerParams.collectiveLogicEnabled =
            c.value("collectiveLogicEnabled",
                    composerParams.collectiveLogicEnabled);
        composerParams.collectiveDecisionHold =
            c.value("collectiveDecisionHold",
                    composerParams.collectiveDecisionHold);
        composerParams.collectiveMinimumDwell =
            c.value("collectiveMinimumDwell",
                    composerParams.collectiveMinimumDwell);
        composerParams.collectiveEventCooldown =
            c.value("collectiveEventCooldown",
                    composerParams.collectiveEventCooldown);
        composerParams.collectiveResponse =
            c.value("collectiveResponse",
                    composerParams.collectiveResponse);
        composerParams.bpm = c.value("bpm", composerParams.bpm);
        composerParams.beatSubdivision =
            c.value("beatSubdivision", composerParams.beatSubdivision);
        composerParams.recentHistorySize = static_cast<std::size_t>(
            std::max(0, c.value("recentHistorySize",
                                static_cast<int>(composerParams.recentHistorySize))));
        composerParams.seed =
            c.value("seed", composerParams.seed);
        if (c.contains("disabledGenerators") &&
            c["disabledGenerators"].is_array()) {
            composerParams.disabledGenerators.clear();
            for (const auto& entry : c["disabledGenerators"])
                composerParams.disabledGenerators.push_back(entry.get<int>());
        }
        if (c.contains("organizationWeights") &&
            c["organizationWeights"].is_array()) {
            for (std::size_t i = 0;
                 i < composerParams.organizationWeights.size() &&
                 i < c["organizationWeights"].size(); ++i) {
                composerParams.organizationWeights[i] =
                    c["organizationWeights"][i].get<float>();
            }
        }
    }
    if (const char* enabled = std::getenv("PDJ_COMPOSER_ENABLED"))
        composerParams.enabled = std::atoi(enabled) != 0;
    if (const char* seed = std::getenv("PDJ_COMPOSER_SEED"))
        composerParams.seed = static_cast<std::uint32_t>(std::strtoul(seed, nullptr, 10));
    if (const char* seconds = std::getenv("PDJ_PULSE_MOMENT_SECONDS"))
        composerParams.pulseMomentDuration =
            std::max(0.1f, static_cast<float>(std::atof(seconds)));
    if (const char* seconds = std::getenv("PDJ_BARSCAN_MOMENT_SECONDS"))
        composerParams.barScanMomentDuration =
            std::max(0.1f, static_cast<float>(std::atof(seconds)));
    if (const char* seconds = std::getenv("PDJ_TAKEOVER_INTERVAL_SECONDS")) {
        const float interval =
            std::max(1.f, static_cast<float>(std::atof(seconds)));
        composerParams.takeoverIntervalMin = interval;
        composerParams.takeoverIntervalMax = interval;
    }

    const int targetFPS = std::max(1, cfg.value("targetFPS", 30));
    PerformanceTestConfig performanceConfig;
    if (cfg.contains("performanceTest")) {
        const auto& p = cfg["performanceTest"];
        performanceConfig.durationSeconds =
            p.value("durationSeconds", performanceConfig.durationSeconds);
        performanceConfig.warmupSeconds =
            p.value("warmupSeconds", performanceConfig.warmupSeconds);
        performanceConfig.reportIntervalSeconds =
            p.value("reportIntervalSeconds",
                    performanceConfig.reportIntervalSeconds);
        if (p.contains("thresholds")) {
            const auto& t = p["thresholds"];
            performanceConfig.thresholds.minimumFps =
                t.value("minimumFps",
                        performanceConfig.thresholds.minimumFps);
            performanceConfig.thresholds.p95FrameMs =
                t.value("p95FrameMs",
                        performanceConfig.thresholds.p95FrameMs);
            performanceConfig.thresholds.lateFrameMs =
                t.value("lateFrameMs",
                        performanceConfig.thresholds.lateFrameMs);
            performanceConfig.thresholds.maximumLatePercent =
                t.value("maximumLatePercent",
                        performanceConfig.thresholds.maximumLatePercent);
            performanceConfig.thresholds.stallFrameMs =
                t.value("stallFrameMs",
                        performanceConfig.thresholds.stallFrameMs);
            performanceConfig.thresholds.maximumMemoryGrowthMB =
                t.value("maximumMemoryGrowthMB",
                        performanceConfig.thresholds.maximumMemoryGrowthMB);
        }
    }
    if (const char* duration = std::getenv("PDJ_PERF_DURATION_SECONDS"))
        performanceConfig.durationSeconds =
            std::max(1.f, static_cast<float>(std::atof(duration)));
    if (const char* warmup = std::getenv("PDJ_PERF_WARMUP_SECONDS"))
        performanceConfig.warmupSeconds =
            std::max(0.f, static_cast<float>(std::atof(warmup)));

    // Sistemas compartidos no GL
    auto pool = std::make_shared<ClipPool>();
    std::string clipFolder = "cortos";
    std::string horizontalClipFolder = "";
    if (cfg.contains("clips")) {
        if (cfg["clips"].contains("folder"))
            clipFolder = cfg["clips"]["folder"].get<std::string>();
        if (cfg["clips"].contains("horizontalFolder"))
            horizontalClipFolder = cfg["clips"]["horizontalFolder"].get<std::string>();
    }
    // Los paquetes portátiles guardan el material mutable junto al .app firmado.
    // Se prefieren esas carpetas si existen para que un settings de una
    // instalación anterior no redirija el paquete a una ruta solo de desarrollo.
    const std::string portablePortrait = "../../../../Videos/Portrait";
    const std::string portableHorizontal = "../../../../Videos/Horizontal";
    if (ofDirectory::doesDirectoryExist(
            ofToDataPath(portablePortrait, true))) {
        clipFolder = portablePortrait;
    }
    if (ofDirectory::doesDirectoryExist(
            ofToDataPath(portableHorizontal, true))) {
        horizontalClipFolder = portableHorizontal;
    }
    pool->scan(clipFolder);
    if (!horizontalClipFolder.empty())
        pool->scanHorizontal(horizontalClipFolder);

    auto oscSender = std::make_shared<OSCSender>();
    oscSender->setup(oscHost, oscPort);

    // Director de interpretación global (compartido por todos los canales)
    auto globalDir = std::make_shared<GlobalDirector>();
    globalDir->setup();

    auto videoDir = std::make_shared<VideoDirector>();
    auto visualComposer = std::make_shared<VisualComposer>();

    // Canales (recursos GL asignados en setup de la app; viven durante el bucle principal).
    static constexpr int kChannelCount = VideoDirector::kChannelCount;
    std::array<Channel, kChannelCount> channels;

    // Geometría de ventanas — leída de settings, valores por defecto razonables para 8 monitores verticales.
    struct WinConfig { int x, y, w, h; };
    std::array<WinConfig, kChannelCount> chCfg = {{
        {0,    0, 1080, 1920},
        {1080, 0, 1080, 1920},
        {2160, 0, 1080, 1920},
        {3240, 0, 1080, 1920},
        {0,    1920, 1080, 1920},
        {1080, 1920, 1080, 1920},
        {2160, 1920, 1080, 1920},
        {3240, 1920, 1080, 1920}
    }};
    WinConfig ctrlCfg = {0, 0, 1400, 900};

    if (cfg.contains("channels") && cfg["channels"].is_array()) {
        for (int i = 0; i < kChannelCount && i < (int)cfg["channels"].size(); i++) {
            const auto& cj = cfg["channels"][i];
            chCfg[i].x = cj.value("x", chCfg[i].x);
            chCfg[i].y = cj.value("y", chCfg[i].y);
            chCfg[i].w = cj.value("width",  chCfg[i].w);
            chCfg[i].h = cj.value("height", chCfg[i].h);
        }
    }
    if (cfg.contains("controlWindow")) {
        const auto& cj = cfg["controlWindow"];
        ctrlCfg.x = cj.value("x",      ctrlCfg.x);
        ctrlCfg.y = cj.value("y",      ctrlCfg.y);
        ctrlCfg.w = cj.value("width",  ctrlCfg.w);
        ctrlCfg.h = cj.value("height", ctrlCfg.h);
    }

    // --- Modo de salida --------------------------------------------------------
    // "auto"        : detecta una pantalla 4K+ al arrancar; usa singleWindow
    //                 si la encuentra, multiWindow en caso contrario.
    // "singleWindow": una salida con canales 0-3.
    // "dualWindow8": dos salidas, cada una dividida en cuatro segmentos verticales.
    // "multiWindow": legado de cuatro ventanas separadas.
    // "multiWindow8": ocho ventanas de diagnóstico.
    std::string outputMode = cfg.value("outputMode", "multiWindow");

    // Si es true, las ventanas de presentación se abren a pantalla completa nativa
    // (sin barra de título; el contenido llena toda la pantalla desde el arranque).
    // Solo aplica a dualWindow8 y singleWindow.
    const bool presentationFullscreen =
        cfg.value("presentationFullscreen", false);

    // Geometría de presentación. La configuración legado de singleWindow sigue soportada.
    int swX = 0, swY = 0, swW = 3840, swH = 2160;
    int sw2X = 3840, sw2Y = 0, sw2W = 3840, sw2H = 2160;
    if (cfg.contains("singleWindow")) {
        const auto& sw = cfg["singleWindow"];
        swX = sw.value("x",      swX);
        swY = sw.value("y",      swY);
        swW = sw.value("width",  swW);
        swH = sw.value("height", swH);
    }
    // Disposición por ventana: "4x1" = cuatro franjas verticales (por defecto), "2x2" = retícula 2x2 horizontal.
    std::string layoutA = "4x1";
    std::string layoutB = "4x1";
    if (cfg.contains("presentationWindows") && cfg["presentationWindows"].is_array()) {
        const auto& windows = cfg["presentationWindows"];
        if (!windows.empty()) {
            const auto& a = windows[0];
            swX = a.value("x", swX);
            swY = a.value("y", swY);
            swW = a.value("width", swW);
            swH = a.value("height", swH);
            layoutA = a.value("layout", layoutA);
        }
        if (windows.size() > 1) {
            const auto& b = windows[1];
            sw2X = b.value("x", sw2X);
            sw2Y = b.value("y", sw2Y);
            sw2W = b.value("width", sw2W);
            sw2H = b.value("height", sw2H);
            layoutB = b.value("layout", layoutB);
        }
    }
    const auto parseWallLayout = [](const std::string& s) {
        return s == "2x2" ? PresentationApp::WallLayout::Grid2x2
                          : PresentationApp::WallLayout::Strips4x1;
    };
    ofLogNotice("main") << "Wall layout A: " << layoutA
                        << "  Wall layout B: " << layoutB;

    if (outputMode == "auto") {
        uint32_t numDisplays = 0;
        CGGetActiveDisplayList(0, nullptr, &numDisplays);
        std::vector<CGDirectDisplayID> dispIDs(numDisplays);
        CGGetActiveDisplayList(numDisplays, dispIDs.data(), &numDisplays);

        struct DisplayChoice {
            CGDirectDisplayID id;
            size_t area;
            bool is4K;
        };
        std::vector<DisplayChoice> external4K;
        std::vector<DisplayChoice> externalChoices;
        std::vector<DisplayChoice> allChoices;
        CGDirectDisplayID bestID = kCGNullDirectDisplay;
        bool bestIs4K = false;

        for (uint32_t i = 0; i < numDisplays; ++i) {
            CGDirectDisplayID dID = dispIDs[i];
            const size_t physW = CGDisplayPixelsWide(dID);
            const size_t physH = CGDisplayPixelsHigh(dID);
            const size_t area = physW * physH;
            const bool is4K = physW >= 3840;
            const DisplayChoice choice{dID, area, is4K};
            allChoices.push_back(choice);
            if (!CGDisplayIsMain(dID)) {
                externalChoices.push_back(choice);
                if (is4K) external4K.push_back(choice);
            }
        }

        const auto byTierAndArea = [](const DisplayChoice& a,
                                      const DisplayChoice& b) {
            if (a.is4K != b.is4K) return a.is4K > b.is4K;
            return a.area > b.area;
        };
        std::sort(external4K.begin(), external4K.end(), byTierAndArea);
        std::sort(externalChoices.begin(), externalChoices.end(), byTierAndArea);
        std::sort(allChoices.begin(), allChoices.end(), byTierAndArea);

        if (external4K.size() >= 2) {
            const CGRect a = CGDisplayBounds(external4K[0].id);
            const CGRect b = CGDisplayBounds(external4K[1].id);
            swX = (int)a.origin.x; swY = (int)a.origin.y;
            swW = (int)a.size.width; swH = (int)a.size.height;
            sw2X = (int)b.origin.x; sw2Y = (int)b.origin.y;
            sw2W = (int)b.size.width; sw2H = (int)b.size.height;
            outputMode = "dualWindow8";
            ofLogNotice("main") << "Auto-selected two 4K outputs for 8 screens";
        } else {
            // Autodetección del par ICUIXIAN: dos salidas externas 1920x1080.
            // La más a la izquierda es Wall A (4x1 vertical, rotación ICUIXIAN 90 deg).
            // La más a la derecha es Wall B (2x2 horizontal, rotación ICUIXIAN 0 deg).
            std::vector<DisplayChoice> icuixian;
            for (const auto& d : externalChoices) {
                const CGRect r = CGDisplayBounds(d.id);
                if (static_cast<int>(r.size.width)  == 1920 &&
                    static_cast<int>(r.size.height) == 1080)
                    icuixian.push_back(d);
            }
            if (icuixian.size() >= 2) {
                std::sort(icuixian.begin(), icuixian.end(),
                          [](const DisplayChoice& a, const DisplayChoice& b) {
                              const CGRect ra = CGDisplayBounds(a.id);
                              const CGRect rb = CGDisplayBounds(b.id);
                              if (ra.origin.x != rb.origin.x)
                                  return ra.origin.x < rb.origin.x;
                              return ra.origin.y < rb.origin.y;
                          });
                const CGRect rA = CGDisplayBounds(icuixian[0].id);
                const CGRect rB = CGDisplayBounds(icuixian[1].id);
                swX  = (int)rA.origin.x; swY  = (int)rA.origin.y;
                swW  = (int)rA.size.width;  swH  = (int)rA.size.height;
                sw2X = (int)rB.origin.x; sw2Y = (int)rB.origin.y;
                sw2W = (int)rB.size.width;  sw2H = (int)rB.size.height;
                layoutA = "4x1";
                layoutB = "2x2";
                outputMode = "dualWindow8";
                ofLogNotice("main")
                    << "Auto-detected ICUIXIAN pair:"
                    << " Wall A (4x1 portrait, rot 90) @ "
                    << swX << "," << swY << " " << swW << "x" << swH
                    << "  Wall B (2x2 landscape, rot 0) @ "
                    << sw2X << "," << sw2Y << " " << sw2W << "x" << sw2H;
            } else {
                const auto& fallback =
                    externalChoices.empty() ? allChoices : externalChoices;
                if (!fallback.empty()) {
                    bestID = fallback.front().id;
                    bestIs4K = fallback.front().is4K;
                }
            }
        }

        if (outputMode == "auto" && bestID != kCGNullDirectDisplay) {
            CGRect bounds = CGDisplayBounds(bestID);
            swX = (int)bounds.origin.x;
            swY = (int)bounds.origin.y;
            swW = (int)bounds.size.width;
            swH = (int)bounds.size.height;
            outputMode = "singleWindow";
            ofLogNotice("main") << "Auto-selected display: "
                << swW << "x" << swH
                << " at (" << swX << ", " << swY << ")"
                << (bestIs4K ? " [4K+]" : " [largest available]")
                << (CGDisplayIsMain(bestID) ? " [primary]" : " [external]");
        } else if (outputMode == "auto") {
            ofLogWarning("main") << "No displays enumerated - falling back to multiWindow";
            outputMode = "multiWindow";
        }
    }

    const bool eightChannelOutput =
        outputMode == "dualWindow8" || outputMode == "multiWindow8";
    if (eightChannelOutput && cfg.contains("cv")) {
        cvp.analysisEveryNFrames = cfg["cv"].value(
            "eightChannelEveryNFrames",
            std::max(1, cvp.analysisEveryNFrames));
    }
    videoDir->setup(pool.get(), videoParams, eightChannelOutput ? 8 : 4);
    visualComposer->setup(composerParams, eightChannelOutput ? 8 : 4);
    auto performanceMonitor = std::make_shared<PerformanceMonitor>(
        eightChannelOutput ? 8 : 4);
    performanceMonitor->configure(performanceConfig, targetFPS, outputMode);

    GeneratorRuntimeParams generatorRuntime;
    if (cfg.contains("generators")) {
        const auto& g = cfg["generators"];
        generatorRuntime.intensity =
            g.value("intensity", generatorRuntime.intensity);
        generatorRuntime.density =
            g.value("density", generatorRuntime.density);
        generatorRuntime.glowGain =
            g.value("glowGain", generatorRuntime.glowGain);
        generatorRuntime.glowRadius =
            g.value("glowRadius", generatorRuntime.glowRadius);
        generatorRuntime.feedbackDecay =
            g.value("feedbackDecay", generatorRuntime.feedbackDecay);
        const auto readColor = [&g](const char* key, ofColor fallback) {
            if (!g.contains(key) || !g[key].is_array() ||
                g[key].size() < 3)
                return fallback;
            return ofColor(g[key][0].get<int>(), g[key][1].get<int>(),
                           g[key][2].get<int>());
        };
        generatorRuntime.cyan = g.contains("primaryColor")
            ? readColor("primaryColor", generatorRuntime.cyan)
            : readColor("cyan", generatorRuntime.cyan);
        generatorRuntime.red = g.contains("secondaryColor")
            ? readColor("secondaryColor", generatorRuntime.red)
            : readColor("red", generatorRuntime.red);
        generatorRuntime.showHud =
            g.value("showHud", generatorRuntime.showHud);
        generatorRuntime.detailTier =
            g.value("detailTier", generatorRuntime.detailTier);
    }
    for (int i = 0; i < videoDir->channelCount(); ++i)
        channels[i].generatorParams() = generatorRuntime;

    bool invertPolarity = false;
    if (cfg.contains("visuals"))
        invertPolarity = cfg["visuals"].value("invertPolarity", invertPolarity);
    if (const char* invert = std::getenv("PDJ_INVERT_POLARITY"))
        invertPolarity = std::atoi(invert) != 0;
    for (int i = 0; i < videoDir->channelCount(); ++i)
        channels[i].setInvertPolarity(invertPolarity);

    VideoPointCloudSettings vpcGlobal;
    if (cfg.contains("videoPointCloud"))
        vpcGlobal = VideoPointCloudSettings::fromJson(cfg["videoPointCloud"]);
    if (const char* gridSize = std::getenv("PDJ_VPC_GRID_SIZE"))
        vpcGlobal.applyQualityTier(std::atoi(gridSize));
    for (int i = 0; i < videoDir->channelCount(); ++i) {
        VideoPointCloudSettings channelVpc = vpcGlobal;
        if (cfg.contains("channels") && cfg["channels"].is_array() &&
            i < static_cast<int>(cfg["channels"].size()) &&
            cfg["channels"][i].contains("videoPointCloud")) {
            channelVpc.applyJson(cfg["channels"][i]["videoPointCloud"]);
        }
        channels[i].configureVideoPointCloud(channelVpc);
    }

    auto makeControlApp = [&](const std::shared_ptr<ofAppBaseWindow>& sharedWindow) {
        ofGLFWWindowSettings cs;
        cs.setGLVersion(4, 1);
        cs.setSize(ctrlCfg.w, ctrlCfg.h);
        cs.setPosition(glm::vec2(ctrlCfg.x, ctrlCfg.y));
        cs.title = "PDJ Control";
        cs.windowMode = OF_WINDOW;
        cs.shareContextWith = sharedWindow;

        auto ctrlWindow = ofCreateWindow(cs);
        std::vector<Channel*> chVec;
        chVec.reserve(videoDir->channelCount());
        for (int i = 0; i < videoDir->channelCount(); ++i)
            chVec.push_back(&channels[i]);
        auto ctrlApp = std::make_shared<ControlApp>(
            chVec, pool.get(), oscSender.get(), globalDir.get(), videoDir.get(),
            visualComposer.get(), performanceMonitor.get(), oscListenPort);
        ofRunApp(ctrlWindow, ctrlApp);
    };

    if (outputMode == "dualWindow8") {
        const ofWindowMode presMode =
            presentationFullscreen ? OF_FULLSCREEN : OF_WINDOW;

        ofGLFWWindowSettings firstSettings;
        firstSettings.setGLVersion(4, 1);
        firstSettings.setSize(swW, swH);
        firstSettings.setPosition(glm::vec2(swX, swY));
        firstSettings.title = "PDJ Presentation A";
        firstSettings.windowMode = presMode;
        auto firstWindow = ofCreateWindow(firstSettings);

        std::array<Channel*, PresentationApp::kSegments> groupA = {
            &channels[0], &channels[1], &channels[2], &channels[3]
        };
        auto firstApp = std::make_shared<PresentationApp>(
            groupA, 0, swW, swH, pool.get(), oscSender.get(), cvp,
            globalDir.get(), videoDir.get(), visualComposer.get(),
            performanceMonitor.get(), 0, targetFPS, parseWallLayout(layoutA));
        ofRunApp(firstWindow, firstApp);

        ofGLFWWindowSettings secondSettings;
        secondSettings.setGLVersion(4, 1);
        secondSettings.setSize(sw2W, sw2H);
        secondSettings.setPosition(glm::vec2(sw2X, sw2Y));
        secondSettings.title = "PDJ Presentation B";
        secondSettings.windowMode = presMode;
        secondSettings.shareContextWith = firstWindow;
        auto secondWindow = ofCreateWindow(secondSettings);

        std::array<Channel*, PresentationApp::kSegments> groupB = {
            &channels[4], &channels[5], &channels[6], &channels[7]
        };
        auto secondApp = std::make_shared<PresentationApp>(
            groupB, 4, sw2W, sw2H, pool.get(), oscSender.get(), cvp,
            globalDir.get(), videoDir.get(), visualComposer.get(),
            performanceMonitor.get(), 1, targetFPS, parseWallLayout(layoutB));
        ofRunApp(secondWindow, secondApp);
        makeControlApp(firstWindow);
    } else if (outputMode == "singleWindow") {
        ofGLFWWindowSettings ws;
        ws.setGLVersion(4, 1);
        ws.setSize(swW, swH);
        ws.setPosition(glm::vec2(swX, swY));
        ws.title      = "PDJ Presentation";
        ws.windowMode = presentationFullscreen ? OF_FULLSCREEN : OF_WINDOW;

        auto presWindow = ofCreateWindow(ws);
        std::array<Channel*, PresentationApp::kSegments> chPtrs = {
            &channels[0], &channels[1], &channels[2], &channels[3]
        };
        auto presApp = std::make_shared<PresentationApp>(
            chPtrs, 0, swW, swH, pool.get(), oscSender.get(), cvp,
            globalDir.get(), videoDir.get(), visualComposer.get(),
            performanceMonitor.get(), 0, targetFPS);
        ofRunApp(presWindow, presApp);
        makeControlApp(presWindow);
    } else {
        // Modo diagnóstico multi-ventana: una ventana por canal.
        ofGLFWWindowSettings ws;
        ws.setGLVersion(4, 1);
        ws.setSize(chCfg[0].w, chCfg[0].h);
        ws.setPosition(glm::vec2(chCfg[0].x, chCfg[0].y));
        ws.title      = "PDJ Ch0";
        ws.windowMode = OF_WINDOW;

        auto mainWindow = ofCreateWindow(ws);
        auto chApp0 = std::make_shared<ChannelApp>(&channels[0], 0,
                      chCfg[0].w, chCfg[0].h, pool.get(), oscSender.get(), cvp,
                      globalDir.get(), videoDir.get(), visualComposer.get(),
                      performanceMonitor.get(), targetFPS);
        ofRunApp(mainWindow, chApp0);

        for (int i = 1; i < videoDir->channelCount(); i++) {
            ofGLFWWindowSettings ws2;
            ws2.setGLVersion(4, 1);
            ws2.setSize(chCfg[i].w, chCfg[i].h);
            ws2.setPosition(glm::vec2(chCfg[i].x, chCfg[i].y));
            ws2.title            = "PDJ Ch" + ofToString(i);
            ws2.windowMode       = OF_WINDOW;
            ws2.shareContextWith = mainWindow;

            auto win = ofCreateWindow(ws2);
            auto app = std::make_shared<ChannelApp>(&channels[i], i,
                       chCfg[i].w, chCfg[i].h, pool.get(), oscSender.get(), cvp,
                       globalDir.get(), videoDir.get(), visualComposer.get(),
                       performanceMonitor.get(), targetFPS);
            ofRunApp(win, app);
        }
        makeControlApp(mainWindow);
    }

    ofRunMainLoop();
    return 0;
}

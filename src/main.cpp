#include "ofMain.h"
#include "ofApp.h"
#include "ControlApp.h"
#include "ClipPool.h"
#include "OSCSender.h"
#include "Channel.h"
#include "GlobalDirector.h"

int main() {
    // Load settings from bin/data/settings.json
    ofJson cfg = ofLoadJson("settings.json");

    // OSC
    std::string oscHost = cfg.value("osc/host", "localhost");
    if (cfg.contains("osc") && cfg["osc"].contains("host"))
        oscHost = cfg["osc"]["host"].get<std::string>();
    int oscPort = 9001;
    if (cfg.contains("osc") && cfg["osc"].contains("port"))
        oscPort = cfg["osc"]["port"].get<int>();

    // CV params
    CVParams cvp;
    if (cfg.contains("cv")) {
        const auto& c = cfg["cv"];
        cvp.halfRes        = c.value("halfRes",        cvp.halfRes);
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

    (void)cfg.value("targetFPS", 30); // applied per-window via ofSetFrameRate in ChannelApp::setup()

    // Shared non-GL systems
    auto pool = std::make_shared<ClipPool>();
    pool->scan("cortos");

    auto oscSender = std::make_shared<OSCSender>();
    oscSender->setup(oscHost, oscPort);

    // Global performance director (shared across all channels)
    auto globalDir = std::make_shared<GlobalDirector>();
    globalDir->setup();

    // Channels (GL resources allocated in ChannelApp::setup; lives for duration of ofRunMainLoop)
    Channel channels[4];

    // Window geometry — read from settings, sensible defaults for 4 vertical monitors
    struct WinConfig { int x, y, w, h; };
    WinConfig chCfg[4] = {
        {0,    0, 1080, 1920},
        {1080, 0, 1080, 1920},
        {2160, 0, 1080, 1920},
        {3240, 0, 1080, 1920}
    };
    WinConfig ctrlCfg = {4320, 0, 1280, 800};

    if (cfg.contains("channels") && cfg["channels"].is_array()) {
        for (int i = 0; i < 4 && i < (int)cfg["channels"].size(); i++) {
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

    // Channel 0 = primary window (no shareContextWith)
    ofGLFWWindowSettings ws;
    ws.setGLVersion(4, 1);
    ws.setSize(chCfg[0].w, chCfg[0].h);
    ws.setPosition(glm::vec2(chCfg[0].x, chCfg[0].y));
    ws.title      = "PDJ Ch0";
    ws.windowMode = OF_WINDOW;

    auto mainWindow = ofCreateWindow(ws);
    auto chApp0 = std::make_shared<ChannelApp>(&channels[0], 0,
                  chCfg[0].w, chCfg[0].h, pool.get(), oscSender.get(), cvp,
                  globalDir.get());
    ofRunApp(mainWindow, chApp0);

    // Channels 1-3
    for (int i = 1; i < 4; i++) {
        ofGLFWWindowSettings ws2;
        ws2.setGLVersion(4, 1);
        ws2.setSize(chCfg[i].w, chCfg[i].h);
        ws2.setPosition(glm::vec2(chCfg[i].x, chCfg[i].y));
        ws2.title           = "PDJ Ch" + ofToString(i);
        ws2.windowMode      = OF_WINDOW;
        ws2.shareContextWith = mainWindow;

        auto win = ofCreateWindow(ws2);
        auto app = std::make_shared<ChannelApp>(&channels[i], i,
                   chCfg[i].w, chCfg[i].h, pool.get(), oscSender.get(), cvp,
                   globalDir.get());
        ofRunApp(win, app);
    }

    // Control window
    {
        ofGLFWWindowSettings cs;
        cs.setGLVersion(4, 1);
        cs.setSize(ctrlCfg.w, ctrlCfg.h);
        cs.setPosition(glm::vec2(ctrlCfg.x, ctrlCfg.y));
        cs.title            = "PDJ Control";
        cs.windowMode       = OF_WINDOW;
        cs.shareContextWith = mainWindow;

        auto ctrlWindow = ofCreateWindow(cs);

        std::vector<Channel*> chPtrs = { &channels[0], &channels[1],
                                         &channels[2], &channels[3] };
        auto ctrlApp = std::make_shared<ControlApp>(chPtrs, pool.get(), oscSender.get(),
                                                     globalDir.get());
        ofRunApp(ctrlWindow, ctrlApp);
    }

    ofRunMainLoop();
    return 0;
}

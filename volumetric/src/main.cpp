#include "ofMain.h"
#include "VolumetricEngine.h"
#include <memory>
#include <limits.h>
#include <CoreFoundation/CoreFoundation.h>

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
    if (ofDirectory::doesDirectoryExist(dataRoot) &&
        ofFile::doesFileExist(dataRoot + "settings.json"))
        ofSetDataPathRoot(dataRoot);
}
}

int main() {
    configureBundledDataPath();
    ofJson cfg = ofLoadJson("settings.json");
    if (!cfg.is_object())
        cfg = ofJson::object();

    ofGLFWWindowSettings settings;
    settings.setGLVersion(4, 1);
    settings.numSamples = 0;
    settings.setPosition({cfg.value("/controlWindow/x"_json_pointer, 40),
                          cfg.value("/controlWindow/y"_json_pointer, 40)});
    settings.setSize(cfg.value("/controlWindow/width"_json_pointer, 720),
                     cfg.value("/controlWindow/height"_json_pointer, 640));

    auto engine = std::make_shared<VolumetricEngine>();
    const std::string mode = cfg.value("outputMode", std::string("singleWindow"));
    int presW = cfg.value("/singleWindow/width"_json_pointer, 1920);
    int presH = cfg.value("/singleWindow/height"_json_pointer, 1080);
    int presX = cfg.value("/singleWindow/x"_json_pointer, 40);
    int presY = cfg.value("/singleWindow/y"_json_pointer, 40);
    if (mode == "dualWindow8" && cfg.contains("presentationWindows")) {
        presW = cfg["presentationWindows"][0].value("width", 1920);
        presH = cfg["presentationWindows"][0].value("height", 1080);
        presX = cfg["presentationWindows"][0].value("x", 0);
        presY = cfg["presentationWindows"][0].value("y", 0);
    }

    ofGLFWWindowSettings pres;
    pres.setGLVersion(4, 1);
    pres.setSize(presW, presH);
    pres.setPosition({presX, presY});
    auto windowA = ofCreateWindow(pres);
    auto appA = std::make_shared<VolumetricPresentationApp>(engine.get(), 0, presW, presH, 0);
    ofRunApp(windowA, appA);

    std::shared_ptr<ofAppBaseWindow> windowB;
    if (mode == "dualWindow8") {
        ofGLFWWindowSettings presB;
        presB.setGLVersion(4, 1);
        presB.shareContextWith = windowA;
        int x = presX + presW;
        int y = presY;
        int w = presW;
        int h = presH;
        if (cfg.contains("presentationWindows") && cfg["presentationWindows"].size() > 1) {
            x = cfg["presentationWindows"][1].value("x", x);
            y = cfg["presentationWindows"][1].value("y", y);
            w = cfg["presentationWindows"][1].value("width", w);
            h = cfg["presentationWindows"][1].value("height", h);
        }
        presB.setSize(w, h);
        presB.setPosition({x, y});
        windowB = ofCreateWindow(presB);
        auto appB = std::make_shared<VolumetricPresentationApp>(engine.get(), 4, w, h, 1);
        ofRunApp(windowB, appB);
    }

    settings.shareContextWith = windowA;
    auto controlWindow = ofCreateWindow(settings);
    auto control = std::make_shared<VolumetricControlApp>(engine.get());
    ofRunApp(controlWindow, control);
    ofRunMainLoop();
    return 0;
}

#pragma once
#include "ofMain.h"
#include "ofxOsc.h"
#include <string>

struct EventData {
    bool      collision    = false;
    bool      ballDetected = false;
    glm::vec2 ballPos      = {0.f, 0.f};
    float     crowdDensity = 0.f;
    float     legDistance  = 0.f;
};

struct CVData {
    int    channelIdx    = 0;
    float  flowMagnitude = 0.f;
    float  flowAngle     = 0.f;
    float  motionEnergy  = 0.f;
    int    blobCount     = 0;

    struct BlobEntry {
        float x, y, vx, vy, area;
        float bbW = 0.f, bbH = 0.f;
    };
    std::vector<BlobEntry> blobs;
    float contourLength = 0.f;

    EventData events;
    int       scoreMode = 0;
};

class OSCSender {
public:
    void setup(const std::string& host, int port);
    void send(const CVData& data);

    void setHost(const std::string& host) { host_ = host; reconnect(); }
    void setPort(int port)               { port_ = port; reconnect(); }
    const std::string& getHost() const   { return host_; }
    int  getPort()               const   { return port_; }

private:
    void reconnect();

    ofxOscSender sender_;
    std::string  host_;
    int          port_ = 9001;
};

#pragma once
#include "ofMain.h"
#include <vector>
#include <string>
#include <map>

class ClipPool {
public:
    void scan(const std::string& folder);

    // Returns a random clip for channelIdx, avoiding clips active on other channels.
    std::string getRandomClip(int channelIdx);

    // Called by each Channel after it loads a clip.
    void setActiveClip(int channelIdx, const std::string& path);

    int  totalClips() const { return (int)clips_.size(); }
    const std::vector<std::string>& getClips() const { return clips_; }

private:
    std::vector<std::string>   clips_;
    std::map<int, std::string> activeClips_;
};

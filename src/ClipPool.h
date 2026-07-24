#pragma once
#include "ofMain.h"
#include <vector>
#include <string>
#include <map>
#include <deque>

class ClipPool {
public:
    void scan(const std::string& folder);

    // Returns a random clip for channelIdx, avoiding clips active on other channels.
    std::string getRandomClip(int channelIdx);
    std::string getIndependentClip(int channelIdx);
    std::string getSharedClip();

    // Called by each Channel after it loads a clip.
    void setActiveClip(int channelIdx, const std::string& path);

    int  totalClips() const { return (int)clips_.size(); }
    const std::vector<std::string>& getClips() const { return clips_; }

private:
    std::string chooseAndRemember(const std::vector<std::string>& candidates,
                                  std::deque<std::string>& history);

    std::vector<std::string>   clips_;
    std::map<int, std::string> activeClips_;
    std::map<int, std::deque<std::string>> channelHistory_;
    std::deque<std::string> sharedHistory_;
};

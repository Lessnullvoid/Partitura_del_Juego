#include "ClipPool.h"
#include <set>

void ClipPool::scan(const std::string& folder) {
    clips_.clear();
    ofDirectory dir(folder);
    dir.allowExt("mp4");
    dir.allowExt("mov");
    dir.allowExt("avi");
    dir.listDir();
    dir.sort();
    for (auto& f : dir.getFiles()) {
        clips_.push_back(f.getAbsolutePath());
        ofLogNotice("ClipPool") << "Found: " << f.getFileName();
    }
    ofLogNotice("ClipPool") << "Total clips: " << clips_.size();
}

void ClipPool::setActiveClip(int channelIdx, const std::string& path) {
    activeClips_[channelIdx] = path;
}

std::string ClipPool::getRandomClip(int channelIdx) {
    if (clips_.empty()) return "";

    // Collect paths already active on OTHER channels
    std::set<std::string> usedByOthers;
    for (auto& kv : activeClips_) {
        if (kv.first != channelIdx && !kv.second.empty())
            usedByOthers.insert(kv.second);
    }

    std::string currentOnThis = activeClips_.count(channelIdx) ? activeClips_.at(channelIdx) : "";

    // Prefer clips not used by anyone
    std::vector<std::string> candidates;
    for (auto& c : clips_) {
        if (!usedByOthers.count(c) && c != currentOnThis)
            candidates.push_back(c);
    }

    // Fallback: allow clips on other channels, but avoid repeat on this channel
    if (candidates.empty()) {
        for (auto& c : clips_) {
            if (c != currentOnThis) candidates.push_back(c);
        }
    }

    // Final fallback: use any clip
    if (candidates.empty()) candidates = clips_;

    int pick = (int)ofRandom(0.f, (float)candidates.size() - 0.001f);
    return candidates[std::max(0, std::min(pick, (int)candidates.size() - 1))];
}

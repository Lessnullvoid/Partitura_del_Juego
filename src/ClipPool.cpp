#include "ClipPool.h"
#include <set>
#include <algorithm>

namespace {
constexpr size_t kHistorySize = 3;

bool contains(const std::deque<std::string>& history, const std::string& path) {
    return std::find(history.begin(), history.end(), path) != history.end();
}
}

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
    return getIndependentClip(channelIdx);
}

std::string ClipPool::getIndependentClip(int channelIdx) {
    if (clips_.empty()) return "";

    // Collect paths already active on OTHER channels
    std::set<std::string> usedByOthers;
    for (auto& kv : activeClips_) {
        if (kv.first != channelIdx && !kv.second.empty())
            usedByOthers.insert(kv.second);
    }

    const std::string currentOnThis =
        activeClips_.count(channelIdx) ? activeClips_.at(channelIdx) : "";
    auto& history = channelHistory_[channelIdx];

    // Prefer clips neither active elsewhere nor recently used by this channel.
    std::vector<std::string> candidates;
    for (auto& c : clips_) {
        if (!usedByOthers.count(c) && c != currentOnThis && !contains(history, c))
            candidates.push_back(c);
    }

    // Relax history before allowing a cross-channel duplicate.
    if (candidates.empty()) {
        for (auto& c : clips_) {
            if (!usedByOthers.count(c) && c != currentOnThis) candidates.push_back(c);
        }
    }

    if (candidates.empty()) {
        for (auto& c : clips_) {
            if (c != currentOnThis && !contains(history, c)) candidates.push_back(c);
        }
    }

    // Final fallback supports small clip libraries.
    if (candidates.empty()) candidates = clips_;
    return chooseAndRemember(candidates, history);
}

std::string ClipPool::getSharedClip() {
    if (clips_.empty()) return "";
    std::vector<std::string> candidates;
    for (const auto& clip : clips_) {
        if (!contains(sharedHistory_, clip)) candidates.push_back(clip);
    }
    if (candidates.empty()) candidates = clips_;
    return chooseAndRemember(candidates, sharedHistory_);
}

std::string ClipPool::chooseAndRemember(const std::vector<std::string>& candidates,
                                        std::deque<std::string>& history) {
    if (candidates.empty()) return "";
    int pick = (int)ofRandom(0.f, (float)candidates.size() - 0.001f);
    const std::string selected =
        candidates[std::max(0, std::min(pick, (int)candidates.size() - 1))];
    history.push_back(selected);
    while (history.size() > kHistorySize) history.pop_front();
    return selected;
}

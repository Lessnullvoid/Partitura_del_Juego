#include "ClipPool.h"
#include <set>
#include <algorithm>

namespace {
constexpr size_t kHistorySize = 3;

bool contains(const std::deque<std::string>& history, const std::string& path) {
    return std::find(history.begin(), history.end(), path) != history.end();
}

void scanFolder(const std::string& folder, std::vector<std::string>& out,
                const std::string& label) {
    out.clear();
    ofDirectory dir(ofToDataPath(folder, true));
    dir.allowExt("mp4");
    dir.allowExt("mov");
    dir.allowExt("avi");
    dir.listDir();
    dir.sort();
    for (auto& f : dir.getFiles()) {
        out.push_back(f.getAbsolutePath());
        ofLogNotice("ClipPool") << "[" << label << "] Found: " << f.getFileName();
    }
    ofLogNotice("ClipPool") << "[" << label << "] Total: " << out.size();
}
}

// ---------------------------------------------------------------------------
// Escaneo

void ClipPool::scan(const std::string& folder) {
    scanFolder(folder, clips_, "portrait");
}

void ClipPool::scanHorizontal(const std::string& folder) {
    scanFolder(folder, horizontalClips_, "horizontal");
}

// ---------------------------------------------------------------------------
// Ayudas

bool ClipPool::isHorizontalChannel(int channelIdx) const {
    return channelIdx >= kGroupThreshold && !horizontalClips_.empty();
}

// ---------------------------------------------------------------------------
// Seguimiento de clips activos

void ClipPool::setActiveClip(int channelIdx, const std::string& path) {
    if (isHorizontalChannel(channelIdx))
        activeHorizontalClips_[channelIdx] = path;
    else
        activeClips_[channelIdx] = path;
}

// ---------------------------------------------------------------------------
// Selección independiente de clips

std::string ClipPool::getRandomClip(int channelIdx) {
    return getIndependentClip(channelIdx);
}

std::string ClipPool::getIndependentClip(int channelIdx) {
    const bool horizontal = isHorizontalChannel(channelIdx);
    const std::vector<std::string>& pool = horizontal ? horizontalClips_ : clips_;
    std::map<int, std::string>&     active = horizontal ? activeHorizontalClips_ : activeClips_;
    std::map<int, std::deque<std::string>>& histories =
        horizontal ? horizontalChannelHistory_ : channelHistory_;

    if (pool.empty()) {
        // Canal horizontal sin clips horizontales — caer al pool vertical.
        if (horizontal) {
            const bool portFallback = !clips_.empty();
            if (!portFallback) return "";
            // Usa la selección vertical para este canal sin interferir con el
            // seguimiento de clips activos del pool vertical.
            auto& history = channelHistory_[channelIdx];
            std::vector<std::string> candidates;
            for (auto& c : clips_) {
                if (!contains(history, c)) candidates.push_back(c);
            }
            if (candidates.empty()) candidates = clips_;
            return chooseAndRemember(candidates, history);
        }
        return "";
    }

    // Recoge rutas ya activas en OTROS canales del mismo pool.
    std::set<std::string> usedByOthers;
    for (auto& kv : active) {
        if (kv.first != channelIdx && !kv.second.empty())
            usedByOthers.insert(kv.second);
    }

    const std::string currentOnThis =
        active.count(channelIdx) ? active.at(channelIdx) : "";
    auto& history = histories[channelIdx];

    // Prefiere clips que no estén activos en otro canal ni usados recientemente por este.
    std::vector<std::string> candidates;
    for (auto& c : pool) {
        if (!usedByOthers.count(c) && c != currentOnThis && !contains(history, c))
            candidates.push_back(c);
    }

    // Relaja el historial antes de permitir un duplicado entre canales.
    if (candidates.empty()) {
        for (auto& c : pool) {
            if (!usedByOthers.count(c) && c != currentOnThis) candidates.push_back(c);
        }
    }

    if (candidates.empty()) {
        for (auto& c : pool) {
            if (c != currentOnThis && !contains(history, c)) candidates.push_back(c);
        }
    }

    // El respaldo final cubre bibliotecas de clips pequeñas.
    if (candidates.empty()) candidates = pool;
    return chooseAndRemember(candidates, history);
}

// ---------------------------------------------------------------------------
// Selección de clip compartido

std::string ClipPool::getSharedClip() {
    return getSharedClipForGroup(0);
}

std::string ClipPool::getSharedClipForGroup(int group) {
    const bool useHorizontal = (group >= 1) && !horizontalClips_.empty();
    const std::vector<std::string>& pool =
        useHorizontal ? horizontalClips_ : clips_;
    std::deque<std::string>& history =
        useHorizontal ? horizontalSharedHistory_ : sharedHistory_;

    if (pool.empty()) return "";

    std::vector<std::string> candidates;
    for (const auto& clip : pool) {
        if (!contains(history, clip)) candidates.push_back(clip);
    }
    if (candidates.empty()) candidates = pool;
    return chooseAndRemember(candidates, history);
}

// ---------------------------------------------------------------------------
// Interno

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

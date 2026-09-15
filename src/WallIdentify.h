#pragma once

#include <atomic>

// Shared live overlay so ControlApp can mark slices on both presentation windows.
struct WallIdentifyState {
    enum class Mode : int {
        Off = 0,
        WallA = 1,
        WallB = 2,
        Both = 3
    };

    static constexpr float kDefaultDurationSeconds = 30.f;

    std::atomic<int> mode{0};
    std::atomic<int> wallARotationDegrees{90};
    std::atomic<int> wallBRotationDegrees{0};
    std::atomic<float> untilTime{-1.f};

    Mode current() const {
        return static_cast<Mode>(mode.load());
    }

    bool showsWallA() const {
        const Mode m = current();
        return m == Mode::WallA || m == Mode::Both;
    }

    bool showsWallB() const {
        const Mode m = current();
        return m == Mode::WallB || m == Mode::Both;
    }

    void setMode(Mode next, float now,
                 float durationSeconds = kDefaultDurationSeconds) {
        mode.store(static_cast<int>(next));
        untilTime.store(next == Mode::Off ? -1.f : now + durationSeconds);
    }

    void tick(float now) {
        const float until = untilTime.load();
        if (until >= 0.f && now >= until) {
            mode.store(static_cast<int>(Mode::Off));
            untilTime.store(-1.f);
        }
    }
};

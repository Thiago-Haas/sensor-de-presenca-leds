#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

class CalibrationButton {
    bool raw_ = false, stable_ = false;
    uint32_t changed_ = 0;
public:
    bool pressed(bool down, uint32_t now) {
        if (down != raw_) { raw_ = down; changed_ = now; }
        if (raw_ != stable_ && uint32_t(now - changed_) >= 50) {
            stable_ = raw_;
            return stable_;
        }
        return false;
    }
};

class FloorCalibration {
    float samples_[15]{};
    unsigned count_ = 0;
    uint32_t started_ = 0;
    bool active_ = false;
public:
    enum Result { Pending, Ready, Unstable, NoEcho };
    void start(uint32_t now) { started_ = now; count_ = 0; active_ = true; }
    bool active() const { return active_; }
    Result sample(float distance, bool valid, uint32_t now, float minimum, float& value) {
        if (!active_) return Pending;
        if (uint32_t(now - started_) >= 5000) { active_ = false; return NoEcho; }
        if (uint32_t(now - started_) < 1000) return Pending;
        if (!valid || !std::isfinite(distance) || distance <= minimum || distance > 400) {
            count_ = 0;
            return Pending;
        }
        samples_[count_++] = distance;
        if (count_ < 15) return Pending;
        active_ = false;
        std::sort(samples_, samples_ + 15);
        if (samples_[14] - samples_[0] > 3.0f) return Unstable;
        value = samples_[7];
        return Ready;
    }
};

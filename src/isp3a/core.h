#pragma once

#include <algorithm>
#include <cmath>

namespace isp3a {

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

inline float safeDiv(float a, float b, float eps = 1e-6f) {
    return a / (std::fabs(b) < eps ? (b < 0.0f ? -eps : eps) : b);
}

struct Stats {
    float meanR;     // channel mean before WB (0..1)
    float meanG;     // channel mean before WB (0..1)
    float meanB;     // channel mean before WB (0..1)
    float meanLuma;  // luma after WB + clip (0..1)
    float sharpness; // arbitrary units
};

struct ControlSet {
    float exposure; // sensor exposure time (relative)
    float gain;     // analog/digital gain multiplier
    float gainR;    // WB gains
    float gainG;
    float gainB;
    float focus;    // lens position
};

struct Scene {
    float baseLuma = 0.18f; // nominal scene brightness
    float colorR = 1.0f;    // scene illuminant/object color bias
    float colorG = 1.0f;
    float colorB = 1.0f;
    float optimalFocus = 75.0f;
};

} // namespace isp3a

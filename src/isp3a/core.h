#pragma once

#include <algorithm>
#include <cmath>

/**
 * @file core.h
 * @brief Core math helpers and shared data structures for the 3A demo.
 */

namespace isp3a {

/**
 * @brief Clamp a float into [lo, hi].
 */
inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}

/**
 * @brief Linear interpolation.
 * @param a Start value.
 * @param b End value.
 * @param t Interpolation factor (typically 0..1).
 */
inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * @brief Safe division that avoids division-by-zero.
 * @param a Numerator.
 * @param b Denominator.
 * @param eps Small epsilon used when |b| is too small.
 */
inline float safeDiv(float a, float b, float eps = 1e-6f) {
    return a / (std::fabs(b) < eps ? (b < 0.0f ? -eps : eps) : b);
}

/**
 * @brief Statistics produced by the simulator / computed from the pipeline.
 */
struct Stats {
    float meanR;     // channel mean before WB (0..1)
    float meanG;     // channel mean before WB (0..1)
    float meanB;     // channel mean before WB (0..1)
    float meanLuma;  // luma after WB + clip (0..1)
    float sharpness; // arbitrary units
};

/**
 * @brief Control values produced by the 3A algorithms.
 */
struct ControlSet {
    float exposure; // sensor exposure time (relative)
    float gain;     // analog/digital gain multiplier
    float gainR;    // WB gains
    float gainG;
    float gainB;
    float focus;    // lens position
};

/**
 * @brief Simple scene model used by the simulation mode.
 */
struct Scene {
    float baseLuma = 0.18f; // nominal scene brightness
    float colorR = 1.0f;    // scene illuminant/object color bias
    float colorG = 1.0f;
    float colorB = 1.0f;
    float optimalFocus = 75.0f;
};

} // namespace isp3a

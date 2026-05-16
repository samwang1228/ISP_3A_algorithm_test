#include "isp3a/simulation.h"

#include "isp3a/core.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>

namespace isp3a {

class Simulator {
public:
    struct Params {
        float sensorScale;
        float clipMin;
        float clipMax;
        float lumaNoiseSigma;
        float chromaNoiseSigma;
        float sharpnessNoiseSigma;
        float focusSigma;
        float sharpnessMax;
        Params()
            : sensorScale(1.0f),
              clipMin(0.0f),
              clipMax(1.0f),
              lumaNoiseSigma(0.0025f),
              chromaNoiseSigma(0.0030f),
              sharpnessNoiseSigma(0.75f),
              focusSigma(10.0f),
              sharpnessMax(100.0f) {}
    };

    explicit Simulator(uint32_t seed = 0, Params p = Params()) : params_(p), rng_(seed) {}

    Stats renderStats(const Scene& scene, const ControlSet& controls) {
        float totalExposure = controls.exposure * controls.gain;

        std::normal_distribution<float> nChroma(0.0f, params_.chromaNoiseSigma);
        float rawR = scene.baseLuma * scene.colorR * totalExposure * params_.sensorScale + nChroma(rng_);
        float rawG = scene.baseLuma * scene.colorG * totalExposure * params_.sensorScale + nChroma(rng_);
        float rawB = scene.baseLuma * scene.colorB * totalExposure * params_.sensorScale + nChroma(rng_);
        rawR = clampf(rawR, params_.clipMin, params_.clipMax);
        rawG = clampf(rawG, params_.clipMin, params_.clipMax);
        rawB = clampf(rawB, params_.clipMin, params_.clipMax);

        float postR = clampf(rawR * controls.gainR, params_.clipMin, params_.clipMax);
        float postG = clampf(rawG * controls.gainG, params_.clipMin, params_.clipMax);
        float postB = clampf(rawB * controls.gainB, params_.clipMin, params_.clipMax);

        float luma = 0.2126f * postR + 0.7152f * postG + 0.0722f * postB;
        std::normal_distribution<float> nLuma(0.0f, params_.lumaNoiseSigma);
        luma = clampf(luma + nLuma(rng_), params_.clipMin, params_.clipMax);

        float focusErr = controls.focus - scene.optimalFocus;
        float focusTerm = std::exp(-(focusErr * focusErr) / (2.0f * params_.focusSigma * params_.focusSigma));
        float lightTerm = clampf((luma - 0.02f) / 0.18f, 0.0f, 1.0f);
        float sharp = params_.sharpnessMax * focusTerm * (0.4f + 0.6f * lightTerm);
        std::normal_distribution<float> nSharp(0.0f, params_.sharpnessNoiseSigma);
        sharp = std::max(0.0f, sharp + nSharp(rng_));

        return Stats{rawR, rawG, rawB, luma, sharp};
    }

private:
    Params params_;
    std::mt19937 rng_;
};

class AutoExposure {
public:
    struct Params {
        float targetMeanLuma;
        float exposureMin;
        float exposureMax;
        float gainMin;
        float gainMax;
        float kp;
        float smoothing;
        float exposurePriority;
        float freezeAboveLuma;
        Params()
            : targetMeanLuma(0.18f),
              exposureMin(0.1f),
              exposureMax(8.0f),
              gainMin(1.0f),
              gainMax(8.0f),
              kp(0.7f),
              smoothing(0.25f),
              exposurePriority(0.85f),
              freezeAboveLuma(0.995f) {}
    };

    explicit AutoExposure(Params p = Params()) : params_(p) {}

    void process(const Stats& stats, ControlSet& controls) {
        float currentLuma = clampf(stats.meanLuma, 0.0f, 1.0f);
        float ratio = safeDiv(params_.targetMeanLuma, currentLuma + 1e-4f);

        float currentTotal = controls.exposure * controls.gain;
        float desiredTotal = currentTotal * std::pow(ratio, params_.kp);
        desiredTotal = clampf(desiredTotal,
                              params_.exposureMin * params_.gainMin,
                              params_.exposureMax * params_.gainMax);

        float expLow = std::max(params_.exposureMin, desiredTotal / params_.gainMax);
        float expHigh = std::min(params_.exposureMax, desiredTotal / params_.gainMin);

        float desiredExp = 0.0f;
        float desiredGain = 0.0f;

        if (expLow <= expHigh) {
            desiredExp = expLow + (expHigh - expLow) * params_.exposurePriority;
            desiredGain = clampf(desiredTotal / desiredExp, params_.gainMin, params_.gainMax);
        } else {
            desiredExp = clampf(desiredTotal, params_.exposureMin, params_.exposureMax);
            desiredGain = clampf(desiredTotal / desiredExp, params_.gainMin, params_.gainMax);
        }

        float smoothing = params_.smoothing;
        if (currentLuma > params_.freezeAboveLuma) {
            smoothing = std::min(0.6f, smoothing * 2.0f);
        }

        controls.exposure = lerpf(controls.exposure, desiredExp, smoothing);
        controls.gain = lerpf(controls.gain, desiredGain, smoothing);
    }

private:
    Params params_;
};

class AutoWhiteBalance {
public:
    struct Params {
        float smoothing;
        float gainMin;
        float gainMax;
        float maxStepPerFrame;
        float minLumaToUpdate;
        float maxLumaToUpdate;
        Params()
            : smoothing(0.25f),
              gainMin(0.5f),
              gainMax(4.0f),
              maxStepPerFrame(0.25f),
              minLumaToUpdate(0.03f),
              maxLumaToUpdate(0.95f) {}
    };

    explicit AutoWhiteBalance(Params p = Params()) : params_(p) {}

    void process(const Stats& stats, ControlSet& controls) {
        if (stats.meanLuma < params_.minLumaToUpdate || stats.meanLuma > params_.maxLumaToUpdate) {
            return;
        }

        float avg = (stats.meanR + stats.meanG + stats.meanB) / 3.0f;
        if (avg < 1e-3f) return;

        float targetR = clampf(safeDiv(avg, stats.meanR + 1e-4f), params_.gainMin, params_.gainMax);
        float targetG = clampf(safeDiv(avg, stats.meanG + 1e-4f), params_.gainMin, params_.gainMax);
        float targetB = clampf(safeDiv(avg, stats.meanB + 1e-4f), params_.gainMin, params_.gainMax);

        auto updateGain = [&](float& current, float target) {
            float next = clampf(target, params_.gainMin, params_.gainMax);
            float delta = clampf(next - current, -params_.maxStepPerFrame, params_.maxStepPerFrame);
            current = clampf(current + delta * params_.smoothing, params_.gainMin, params_.gainMax);
        };

        updateGain(controls.gainR, targetR);
        updateGain(controls.gainG, targetG);
        updateGain(controls.gainB, targetB);
    }

private:
    Params params_;
};

class AutoFocus {
public:
    struct Params {
        float focusMin;
        float focusMax;
        float scanStep;
        float trackStep;
        float reScanDropRatio;
        int settleFrames;
        Params()
            : focusMin(0.0f),
              focusMax(100.0f),
              scanStep(2.0f),
              trackStep(0.5f),
              reScanDropRatio(0.75f),
              settleFrames(2) {}
    };

    explicit AutoFocus(Params p = Params()) : params_(p) {}

    void process(const Stats& stats, ControlSet& controls) {
        if (mode_ == Mode::Scan) {
            stepScan(stats, controls);
        } else {
            stepTrack(stats, controls);
        }
    }

private:
    enum class Mode { Scan, Track };

    void stepScan(const Stats& stats, ControlSet& controls) {
        if (settleCounter_ > 0) {
            --settleCounter_;
            updateBest(stats, controls);
            return;
        }

        updateBest(stats, controls);

        controls.focus += params_.scanStep;
        if (controls.focus >= params_.focusMax) {
            controls.focus = bestFocus_;
            mode_ = Mode::Track;
            settleCounter_ = params_.settleFrames;
        }
        controls.focus = clampf(controls.focus, params_.focusMin, params_.focusMax);
    }

    void stepTrack(const Stats& stats, ControlSet& controls) {
        if (settleCounter_ > 0) {
            --settleCounter_;
            updateBest(stats, controls);
            return;
        }

        if (bestSharpness_ > 1e-3f && stats.sharpness < bestSharpness_ * params_.reScanDropRatio) {
            mode_ = Mode::Scan;
            bestSharpness_ = 0.0f;
            bestFocus_ = params_.focusMin;
            controls.focus = params_.focusMin;
            settleCounter_ = params_.settleFrames;
            return;
        }

        if (stats.sharpness >= lastSharpness_) {
        } else {
            direction_ = -direction_;
        }
        lastSharpness_ = stats.sharpness;

        controls.focus = clampf(controls.focus + direction_ * params_.trackStep, params_.focusMin, params_.focusMax);
        updateBest(stats, controls);
    }

    void updateBest(const Stats& stats, const ControlSet& controls) {
        if (stats.sharpness > bestSharpness_) {
            bestSharpness_ = stats.sharpness;
            bestFocus_ = controls.focus;
        }
    }

    Params params_;
    Mode mode_ = Mode::Scan;
    float bestSharpness_ = 0.0f;
    float bestFocus_ = 0.0f;
    float lastSharpness_ = 0.0f;
    float direction_ = 1.0f;
    int settleCounter_ = 0;
};

int runSimulation(int iterations, uint32_t seed, bool csv, bool header) {
    ControlSet controls = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 20.0f};
    Scene scene;
    Simulator sim(seed);
    AutoExposure ae;
    AutoWhiteBalance awb;
    AutoFocus af;

    std::cout << std::fixed << std::setprecision(4);
    if (header) {
        if (csv) {
            std::cout << "frame,exposure,gain,wb_r,wb_g,wb_b,focus,mean_r,mean_g,mean_b,luma,sharpness\n";
        } else {
            std::cout << "Starting 3A Simulation (" << iterations << " frames)\n";
            std::cout << "Frame | Exp    | Gain   | WB(R,G,B)           | Focus  | Luma   | Sharp\n";
            std::cout << "------------------------------------------------------------------------\n";
        }
    }

    for (int frame = 0; frame < iterations; ++frame) {
        if (frame == iterations / 3) {
            scene.baseLuma *= 0.35f;
        }
        if (frame == (2 * iterations) / 3) {
            scene.colorR = 1.25f;
            scene.colorG = 1.00f;
            scene.colorB = 0.80f;
            scene.optimalFocus = 78.0f;
        }

        Stats stats = sim.renderStats(scene, controls);

        ae.process(stats, controls);
        awb.process(stats, controls);
        af.process(stats, controls);

        if (csv) {
            std::cout << frame << ',' << controls.exposure << ',' << controls.gain << ',' << controls.gainR << ','
                      << controls.gainG << ',' << controls.gainB << ',' << controls.focus << ',' << stats.meanR
                      << ',' << stats.meanG << ',' << stats.meanB << ',' << stats.meanLuma << ',' << stats.sharpness
                      << '\n';
        } else {
            std::cout << std::setw(5) << frame << " | " << std::setw(6) << controls.exposure << " | " << std::setw(6)
                      << controls.gain << " | "
                      << "(" << std::setw(6) << controls.gainR << "," << std::setw(6) << controls.gainG << ","
                      << std::setw(6) << controls.gainB << ") | " << std::setw(6) << controls.focus << " | "
                      << std::setw(6) << stats.meanLuma << " | " << std::setw(6) << stats.sharpness << "\n";
        }
    }

    return 0;
}

} // namespace isp3a

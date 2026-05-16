#pragma once

#include <string>
#include <vector>

namespace isp3a {

struct Image3AParams {
    float targetMeanLuma = 0.18f;
    float maxGain = 4.0f;
    float minGain = 0.25f;
    float minExposure = 0.25f;
    float maxExposure = 4.0f;
    float clipProtectP = 0.99f;
    float clipProtectMax = 0.98f;
    bool toneMap = false;
    float toneMapWhite = 4.0f; // extended Reinhard white point (linear luma), > 1 allows brighter highlights
    float sharpenAmount = 0.0f;
    float sharpenSigma = 1.2f;
};

int runImageMode(const std::vector<std::string>& inputImages,
                 const std::string& outputPath,
                 const std::string& outputDir,
                 const Image3AParams& params,
                 bool printParams);

} // namespace isp3a

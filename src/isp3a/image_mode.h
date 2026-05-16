#pragma once

#include <string>
#include <vector>

/**
 * @file image_mode.h
 * @brief Entry point and parameters for image-mode (OpenCV) processing.
 */

namespace isp3a {

/**
 * @brief Parameters controlling the single-frame "image mode" pipeline.
 *
 * Image mode is a post-process approximation of 3A:
 * - AWB-like: gray-world gains from global RGB means
 * - AE-like: global exposure scaling from post-WB mean luma
 * - highlight protection via a luma percentile cap
 * - optional tone mapping and sharpening
 */
struct Image3AParams {
    /** Target mean luma (linear) after WB + exposure. Typical: 0.18. */
    float targetMeanLuma = 0.18f;

    /** Clamp range for WB gains. */
    float maxGain = 4.0f;
    float minGain = 0.25f;

    /** Clamp range for global exposure scaling. */
    float minExposure = 0.25f;
    float maxExposure = 4.0f;

    /** Percentile used for highlight protection (linear luma). Typical: 0.99. */
    float clipProtectP = 0.99f;

    /** Max allowed luma at that percentile (can be > 1 to allow HDR headroom). */
    float clipProtectMax = 0.98f;

    /** Enable tone mapping (extended Reinhard). */
    bool toneMap = false;

    /**
     * White point for tone mapping (linear luma).
     * Larger values preserve highlight detail but reduce compression.
     */
    float toneMapWhite = 4.0f; // extended Reinhard white point (linear luma), > 1 allows brighter highlights

    /** Unsharp mask amount, 0 disables sharpening. */
    float sharpenAmount = 0.0f;

    /** Gaussian sigma for unsharp mask blur. */
    float sharpenSigma = 1.2f;
};

/**
 * @brief Run image-mode pipeline on one or more input images.
 *
 * @param inputImages One or more input image paths.
 * @param outputPath Output path (use when there is exactly 1 input).
 * @param outputDir Output directory (use when there are multiple inputs).
 * @param params Processing parameters.
 * @param printParams Whether to print estimated parameters for each image.
 * @return Process exit code (0 on success).
 */
int runImageMode(const std::vector<std::string>& inputImages,
                 const std::string& outputPath,
                 const std::string& outputDir,
                 const Image3AParams& params,
                 bool printParams);

} // namespace isp3a

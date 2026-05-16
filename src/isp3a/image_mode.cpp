#include "isp3a/image_mode.h"

#include "isp3a/core.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#if defined(ISP3A_HAVE_OPENCV)
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace isp3a {

static std::string basenameNoExt(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

#if defined(ISP3A_HAVE_OPENCV)

static inline float srgbToLinear1(float x) {
    x = clampf(x, 0.0f, 1.0f);
    if (x <= 0.04045f) return x / 12.92f;
    return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

static inline float linearToSrgb1(float x) {
    x = clampf(x, 0.0f, 1.0f);
    if (x <= 0.0031308f) return 12.92f * x;
    return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

static cv::Mat srgbToLinear(const cv::Mat& bgr8u) {
    // NOTE: OpenCV uses BGR channel order.
    // We convert sRGB -> linear (approx) so exposure/WB math behaves more like real ISP.
    cv::Mat bgr32f;
    bgr8u.convertTo(bgr32f, CV_32FC3, 1.0 / 255.0);
    cv::Mat linear(bgr32f.size(), CV_32FC3);
    for (int y = 0; y < bgr32f.rows; ++y) {
        const cv::Vec3f* src = bgr32f.ptr<cv::Vec3f>(y);
        cv::Vec3f* dst = linear.ptr<cv::Vec3f>(y);
        for (int x = 0; x < bgr32f.cols; ++x) {
            dst[x][0] = srgbToLinear1(src[x][0]);
            dst[x][1] = srgbToLinear1(src[x][1]);
            dst[x][2] = srgbToLinear1(src[x][2]);
        }
    }
    return linear;
}

static cv::Mat linearToSrgb8u(const cv::Mat& bgrLin32f) {
    // Convert linear -> sRGB for display/file output.
    cv::Mat srgb(bgrLin32f.size(), CV_32FC3);
    for (int y = 0; y < bgrLin32f.rows; ++y) {
        const cv::Vec3f* src = bgrLin32f.ptr<cv::Vec3f>(y);
        cv::Vec3f* dst = srgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < bgrLin32f.cols; ++x) {
            dst[x][0] = linearToSrgb1(src[x][0]);
            dst[x][1] = linearToSrgb1(src[x][1]);
            dst[x][2] = linearToSrgb1(src[x][2]);
        }
    }
    cv::Mat out8u;
    srgb.convertTo(out8u, CV_8UC3, 255.0);
    return out8u;
}

static void computeMeansLinear(const cv::Mat& bgrLin, float& meanB, float& meanG, float& meanR, float& meanLuma) {
    // Global means in *linear* space.
    cv::Scalar m = cv::mean(bgrLin);
    meanB = static_cast<float>(m[0]);
    meanG = static_cast<float>(m[1]);
    meanR = static_cast<float>(m[2]);
    meanLuma = 0.2126f * meanR + 0.7152f * meanG + 0.0722f * meanB;
}

static float percentileLumaLinear(const cv::Mat& bgrLin, float p) {
    // Approximate a luma percentile (used for highlight protection).
    // Downsample to keep it fast enough for large images.
    cv::Mat small;
    int maxW = 512;
    if (bgrLin.cols > maxW) {
        float scale = static_cast<float>(maxW) / static_cast<float>(bgrLin.cols);
        cv::resize(bgrLin, small, cv::Size(), scale, scale, cv::INTER_AREA);
    } else {
        small = bgrLin;
    }

    std::vector<float> lumas;
    lumas.reserve(static_cast<size_t>(small.rows) * static_cast<size_t>(small.cols));
    for (int y = 0; y < small.rows; ++y) {
        const cv::Vec3f* row = small.ptr<cv::Vec3f>(y);
        for (int x = 0; x < small.cols; ++x) {
            float b = row[x][0];
            float g = row[x][1];
            float r = row[x][2];
            float l = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            lumas.push_back(clampf(l, 0.0f, 1.0f));
        }
    }
    if (lumas.empty()) return 0.0f;

    p = clampf(p, 0.0f, 1.0f);
    size_t k = static_cast<size_t>(std::floor(p * static_cast<float>(lumas.size() - 1)));
    std::nth_element(lumas.begin(), lumas.begin() + static_cast<long>(k), lumas.end());
    return lumas[k];
}

struct Image3AResult {
    float wbGainB;
    float wbGainG;
    float wbGainR;
    float exposureScale;
    float inMeanLuma;
    float outMeanLuma;
};

static Image3AResult estimateImage3A(const cv::Mat& bgrLin, const Image3AParams& p) {
    // Image 3A (single-frame, post-process):
    // - AWB-like: gray-world gains from global RGB means
    // - AE-like: single global exposureScale from post-WB mean luma
    // - Highlight protection: limit exposureScale using a high luma percentile
    float meanB = 0, meanG = 0, meanR = 0, meanL = 0;
    computeMeansLinear(bgrLin, meanB, meanG, meanR, meanL);

    float avg = (meanR + meanG + meanB) / 3.0f;
    // Gray-world: push each channel mean toward the overall average.
    float gainR = clampf(safeDiv(avg, meanR + 1e-6f), p.minGain, p.maxGain);
    float gainG = clampf(safeDiv(avg, meanG + 1e-6f), p.minGain, p.maxGain);
    float gainB = clampf(safeDiv(avg, meanB + 1e-6f), p.minGain, p.maxGain);

    cv::Mat wb = bgrLin.clone();
    // Apply WB to estimate post-WB luma for AE.
    for (int y = 0; y < wb.rows; ++y) {
        cv::Vec3f* row = wb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < wb.cols; ++x) {
            row[x][0] = row[x][0] * gainB;
            row[x][1] = row[x][1] * gainG;
            row[x][2] = row[x][2] * gainR;
        }
    }
    float wbMeanB = 0, wbMeanG = 0, wbMeanR = 0, wbMeanL = 0;
    computeMeansLinear(wb, wbMeanB, wbMeanG, wbMeanR, wbMeanL);

    float exposureScale = clampf(safeDiv(p.targetMeanLuma, wbMeanL + 1e-6f), p.minExposure, p.maxExposure);

    // Highlight protection:
    // If the p-th percentile luma is already high, cap exposureScale so that percentile
    // does not exceed clipProtectMax. This avoids blowing out bright regions.
    float pL = percentileLumaLinear(wb, p.clipProtectP);
    if (pL > 1e-6f) {
        exposureScale = std::min(exposureScale, p.clipProtectMax / pL);
        exposureScale = clampf(exposureScale, p.minExposure, p.maxExposure);
    }

    float outMeanL = clampf(wbMeanL * exposureScale, 0.0f, 1.0f);
    return Image3AResult{gainB, gainG, gainR, exposureScale, meanL, outMeanL};
}

static cv::Mat applyImage3A(const cv::Mat& bgrLin, const Image3AParams& p, const Image3AResult& r) {
    cv::Mat out = bgrLin.clone();
    for (int y = 0; y < out.rows; ++y) {
        cv::Vec3f* row = out.ptr<cv::Vec3f>(y);
        for (int x = 0; x < out.cols; ++x) {
            // Apply WB + exposure in linear. Do NOT clamp here; tone mapping may compress HDR values.
            row[x][0] = row[x][0] * r.wbGainB * r.exposureScale;
            row[x][1] = row[x][1] * r.wbGainG * r.exposureScale;
            row[x][2] = row[x][2] * r.wbGainR * r.exposureScale;
        }
    }

    if (p.toneMap) {
        // Tone mapping (extended Reinhard):
        // - Operates on luma (linear), preserves chroma by scaling RGB uniformly.
        // - Lets us allow HDR headroom (values > 1) and compress back into [0, 1] smoothly.
        // - `toneMapWhite` roughly controls how quickly highlights roll off.
        float white = std::max(1e-3f, p.toneMapWhite);
        float white2 = white * white;
        for (int y = 0; y < out.rows; ++y) {
            cv::Vec3f* row = out.ptr<cv::Vec3f>(y);
            for (int x = 0; x < out.cols; ++x) {
                float b = std::max(0.0f, row[x][0]);
                float g = std::max(0.0f, row[x][1]);
                float rr = std::max(0.0f, row[x][2]);

                float l = 0.2126f * rr + 0.7152f * g + 0.0722f * b;
                if (l > 1e-6f) {
                    // Extended Reinhard:
                    //   Lm = (L * (1 + L / W^2)) / (1 + L)
                    float lm = (l * (1.0f + l / white2)) / (1.0f + l);
                    float s = lm / l;
                    row[x][0] = b * s;
                    row[x][1] = g * s;
                    row[x][2] = rr * s;
                } else {
                    row[x][0] = 0.0f;
                    row[x][1] = 0.0f;
                    row[x][2] = 0.0f;
                }
            }
        }
    }

    // Clamp to displayable range before optional sharpening.
    cv::min(out, 1.0f, out);
    cv::max(out, 0.0f, out);

    if (p.sharpenAmount > 0.0f) {
        cv::Mat blur;
        cv::GaussianBlur(out, blur, cv::Size(0, 0), p.sharpenSigma);
        out = out + p.sharpenAmount * (out - blur);
        cv::min(out, 1.0f, out);
        cv::max(out, 0.0f, out);
    }

    return out;
}

int runImageMode(const std::vector<std::string>& inputImages,
                 const std::string& outputPath,
                 const std::string& outputDir,
                 const Image3AParams& params,
                 bool printParams) {
    for (size_t idx = 0; idx < inputImages.size(); ++idx) {
        const std::string& inPath = inputImages[idx];
        cv::Mat in = cv::imread(inPath, cv::IMREAD_COLOR);
        if (in.empty()) {
            std::cerr << "Failed to read image: " << inPath << "\n";
            return 2;
        }
        cv::Mat lin = srgbToLinear(in);
        Image3AResult r = estimateImage3A(lin, params);
        cv::Mat outLin = applyImage3A(lin, params, r);
        cv::Mat out8u = linearToSrgb8u(outLin);

        std::string outPath;
        if (!outputPath.empty()) {
            outPath = outputPath;
        } else {
            std::string base = basenameNoExt(inPath);
            std::ostringstream oss;
            oss << outputDir;
            if (!outputDir.empty() && outputDir.back() != '/' && outputDir.back() != '\\') oss << '/';
            oss << base << "_3a.jpg";
            outPath = oss.str();
        }

        if (printParams) {
            std::cout << "[Image3A] " << inPath << " -> " << outPath << "\n"
                      << "  inMeanLuma=" << r.inMeanLuma << "  outMeanLuma~=" << r.outMeanLuma << "\n"
                      << "  WB gains (R,G,B)= (" << r.wbGainR << "," << r.wbGainG << "," << r.wbGainB << ")\n"
                      << "  exposureScale=" << r.exposureScale
                      << "  toneMap=" << (params.toneMap ? "on" : "off")
                      << "  toneMapWhite=" << params.toneMapWhite
                      << "  sharpenAmount=" << params.sharpenAmount
                      << "\n";
        }

        if (!cv::imwrite(outPath, out8u)) {
            std::cerr << "Failed to write image: " << outPath << "\n";
            return 2;
        }
    }

    return 0;
}

#else

int runImageMode(const std::vector<std::string>&,
                 const std::string&,
                 const std::string&,
                 const Image3AParams&,
                 bool) {
    std::cerr << "Image mode requires OpenCV. Rebuild with OpenCV to enable it.\n";
    return 2;
}

#endif

} // namespace isp3a

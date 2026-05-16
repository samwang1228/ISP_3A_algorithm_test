#include <iostream>
#include <string>
#include <vector>

#include "isp3a/image_mode.h"
#include "isp3a/simulation.h"

int main(int argc, char** argv) {
    int iterations = 60;
    uint32_t seed = 0;
    bool csv = false;
    bool header = true;

#if defined(ISP3A_HAVE_OPENCV)
    std::vector<std::string> inputImages;
    std::string outputPath;
    std::string outputDir;
    isp3a::Image3AParams imgParams;
    bool imageMode = false;
    bool printParams = true;
#endif

    auto printHelp = [&]() {
        std::cout
            << "3A demo (AE/AWB/AF)\n\n"
            << "Usage:\n"
            << "  isp_3a_demo [frames] [--seed N] [--csv] [--no-header]\n";

#if defined(ISP3A_HAVE_OPENCV)
        std::cout
            << "  isp_3a_demo --in img.jpg --out out.jpg [--target-luma X] [--sharpen A]\n"
            << "  isp_3a_demo --in a.jpg --in b.jpg --out-dir out/ [--target-luma X] [--sharpen A]\n";
#else
        std::cout << "  (Rebuild with OpenCV to enable image mode)\n";
#endif

        std::cout
            << "\nOptions (simulation):\n"
            << "Options:\n"
            << "  frames        Number of frames to simulate (default: 60)\n"
            << "  --seed N      Random seed (default: 0)\n"
            << "  --csv         Output CSV format\n"
            << "  --no-header   Suppress header line(s)\n"
            << "  --help        Show this help\n";

#if defined(ISP3A_HAVE_OPENCV)
        std::cout
            << "\nOptions (image mode):\n"
            << "  --in PATH           Input image (repeatable)\n"
            << "  --out PATH          Output image (single input)\n"
            << "  --out-dir DIR       Output directory (multi input)\n"
            << "  --target-luma X     Target mean luma in linear space (default: 0.18)\n"
            << "  --clip-protect-p P  Highlight protection percentile in linear luma (default: 0.99)\n"
            << "  --clip-protect-max M  Max allowed luma at that percentile (default: 0.98)\n"
            << "  --tonemap           Enable tone mapping (extended Reinhard)\n"
            << "  --tonemap-white W   Tone mapping white point in linear luma (default: 4.0)\n"
            << "  --sharpen A         Unsharp amount, 0=off (default: 0)\n"
            << "  --sharpen-sigma S   Unsharp blur sigma (default: 1.2)\n"
            << "  --no-print-params   Don't print estimated gains/scale\n";
#endif
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
        if (arg == "--csv") {
            csv = true;
            continue;
        }
        if (arg == "--no-header") {
            header = false;
            continue;
        }
        if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::stoul(argv[++i]));
            continue;
        }

#if defined(ISP3A_HAVE_OPENCV)
        if (arg == "--in" && i + 1 < argc) {
            inputImages.push_back(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--out" && i + 1 < argc) {
            outputPath = argv[++i];
            imageMode = true;
            continue;
        }
        if (arg == "--out-dir" && i + 1 < argc) {
            outputDir = argv[++i];
            imageMode = true;
            continue;
        }
        if (arg == "--target-luma" && i + 1 < argc) {
            imgParams.targetMeanLuma = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--clip-protect-p" && i + 1 < argc) {
            imgParams.clipProtectP = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--clip-protect-max" && i + 1 < argc) {
            imgParams.clipProtectMax = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--tonemap") {
            imgParams.toneMap = true;
            imageMode = true;
            continue;
        }
        if (arg == "--tonemap-white" && i + 1 < argc) {
            imgParams.toneMapWhite = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--sharpen" && i + 1 < argc) {
            imgParams.sharpenAmount = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--sharpen-sigma" && i + 1 < argc) {
            imgParams.sharpenSigma = std::stof(argv[++i]);
            imageMode = true;
            continue;
        }
        if (arg == "--no-print-params") {
            printParams = false;
            imageMode = true;
            continue;
        }
#endif
        if (!arg.empty() && arg[0] != '-') {
            iterations = std::max(1, std::stoi(arg));
            continue;
        }

        std::cerr << "Unknown argument: " << arg << "\n\n";
        printHelp();
        return 2;
    }

#if defined(ISP3A_HAVE_OPENCV)
    if (imageMode) {
        if (inputImages.empty()) {
            std::cerr << "Image mode requires at least one --in PATH\n\n";
            printHelp();
            return 2;
        }
        if (!outputPath.empty() && inputImages.size() != 1) {
            std::cerr << "--out can only be used with a single --in\n\n";
            return 2;
        }
        if (outputPath.empty() && outputDir.empty()) {
            std::cerr << "Image mode requires --out (single input) or --out-dir (multi input)\n\n";
            return 2;
        }
        if (imgParams.targetMeanLuma <= 0.0f || imgParams.targetMeanLuma > 1.0f) {
            std::cerr << "--target-luma must be in (0, 1]\n\n";
            return 2;
        }
        if (imgParams.clipProtectP <= 0.0f || imgParams.clipProtectP > 1.0f) {
            std::cerr << "--clip-protect-p must be in (0, 1]\n\n";
            return 2;
        }
        if (imgParams.clipProtectMax <= 0.0f) {
            std::cerr << "--clip-protect-max must be > 0\n\n";
            return 2;
        }
        if (imgParams.toneMapWhite <= 0.0f) {
            std::cerr << "--tonemap-white must be > 0\n\n";
            return 2;
        }
        if (imgParams.sharpenAmount < 0.0f) {
            std::cerr << "--sharpen must be >= 0\n\n";
            return 2;
        }

        return isp3a::runImageMode(inputImages, outputPath, outputDir, imgParams, printParams);
    }
#endif

    return isp3a::runSimulation(iterations, seed, csv, header);
}

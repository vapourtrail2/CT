#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace measure {

struct ZcGrayImage {
    int width = 0;
    int height = 0;
    int widthStep = 0;
    std::vector<std::uint8_t> pixels;
};

struct ZcRectFrame {
    double startX = 0.0;
    double startY = 0.0;
    double width = 0.0;
    double height = 0.0;
    double cosAngle = 1.0;
    double sinAngle = 0.0;
};

struct ZcMeasuredLine {
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    int measuredPointsCount = 0;
};

class ZcEdgeAlgorithm final {
public:
    struct Options {
        int featureId = 1;
    };

    explicit ZcEdgeAlgorithm(Options options = {});
    ~ZcEdgeAlgorithm();

    ZcEdgeAlgorithm(const ZcEdgeAlgorithm&) = delete;
    ZcEdgeAlgorithm& operator=(const ZcEdgeAlgorithm&) = delete;

    bool MeasureLineByRect(
        const ZcGrayImage& image,
        const ZcRectFrame& frame,
        ZcMeasuredLine& line,
        std::string& error);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace measure

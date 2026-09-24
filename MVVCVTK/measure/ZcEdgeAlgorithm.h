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

struct ZcRectFrame {//在图像的哪个矩形区域抓边
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

struct ZcCircleRingFrame {
    double x = 0, y = 0, innerRadius = 0, outerRadius = 0;
};

struct ZcMeasuredCircle {
    double x = 0, y = 0, radius = 0;
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

    bool MeasureCircleByCircleRing(const ZcGrayImage& image,
        const ZcCircleRingFrame& frame, ZcMeasuredCircle& circle, std::string& error);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace measure

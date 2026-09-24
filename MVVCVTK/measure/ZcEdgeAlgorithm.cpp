#include "measure/ZcEdgeAlgorithm.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <opencv2/core/types_c.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <utility>

namespace measure {
namespace {

#pragma pack(push, 8)
struct TEncryptionData {
    std::uint8_t data[8]{};
};

struct TPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// ZC_GetMeasuredPoints 当前 C# 调用按 16 字节步长读取 Point.X/Y。
struct TMeasuredPoint2D {
    double x = 0.0;
    double y = 0.0;
};

struct TScalePara {
    double xScale = 1.0;
    double yScale = 1.0;
    double angle = 0.0;
    std::int32_t calibratedFlag = 1;
    const char* magnification = nullptr;
};

struct TMeasureAlgorithmPara {
    std::int32_t zoomIndex = 0;
    std::int32_t edgeContrastType = 0;
    std::int32_t interferenceLength = 10;
    double integrity = 0.5;
    std::int32_t contrast = 20;
    std::int32_t roughness = 5;
    std::int32_t measureIndex = 0;
    std::int32_t measurePointQty = 0;
    std::int32_t pinkSkipPercent = 0;
    std::int32_t edgeLineType = 0;
    std::int32_t aiIndex = 0;
    std::int32_t fastMeasure = 1;
    std::int32_t filtering = 1;
};

struct TAlgorithmRectFramePara {
    double startX = 0.0;
    double startY = 0.0;
    double width = 0.0;
    double height = 0.0;
    double cosAngle = 1.0;
    double sinAngle = 0.0;
};

struct TMeasureLine {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double length = 0.0;
    double direction = 0.0;
    double positiveTolerance = 0.0;
    double negativeTolerance = 0.0;
    double straightness = 0.0;
    double positional = 0.0;
    std::int32_t pointsCount = 0;
    double x1 = 0.0;
    double y1 = 0.0;
    double z1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    double z2 = 0.0;
};
struct TAlgorithmCircleRingFramePara {
    double x, y, r1, r2;
};
struct TImageCircle {
    double x = 0, y = 0, r = 0;
};
struct TMeasuredCircle {
    double x = 0, y = 0, z = 0;
    double r = 0, minR = 0, maxR = 0, d = 0, circumference = 0;
    double pTolerance = 0, nTolerance = 0, circularity = 0, positional = 0;
    std::int32_t pointsCount = 0;
    TMeasuredPoint2D* points = nullptr;
    double standartDistance = 0;
};
struct TAlgorithmArcRingFramePara { double x, y, r1, r2, r, angle1, angle2; };
struct TMeasuredArc {
    double x, y, z, r, d, angle1, angle2, angleLength;
    double minR, maxR, pTolerance, nTolerance, circularity, positional;
    std::int32_t pointsCount;
    double x1, y1, x2, y2, length;
    TMeasuredPoint2D* points;
};
#pragma pack(pop)
static_assert(sizeof(TAlgorithmArcRingFramePara) == 56, "Arc frame ABI");
static_assert(sizeof(TMeasuredArc) == 168 && offsetof(TMeasuredArc, x1) == 120
    && offsetof(TMeasuredArc, points) == 160, "Measured arc x64 ABI");
using MeasureArcFn = int(__cdecl*)(void*, TPoint*, TMeasureAlgorithmPara*,
    TAlgorithmArcRingFramePara*, TMeasuredArc*);

static_assert(sizeof(TAlgorithmCircleRingFramePara) == 32, "Circle ring ABI");
static_assert(sizeof(TImageCircle) == 24, "Image circle ABI");
static_assert(sizeof(TMeasuredCircle) == 120, "Measured circle x64 ABI");
static_assert(offsetof(TMeasuredCircle, points) == 104, "Circle points offset");
static_assert(offsetof(TMeasuredCircle, standartDistance) == 112, "Circle distance offset");

static_assert(sizeof(TEncryptionData) == 8, "Unexpected TEncryptionData ABI");
static_assert(sizeof(TPoint) == 24, "Unexpected TPoint ABI");
static_assert(sizeof(TMeasuredPoint2D) == 16, "Unexpected measured-point ABI");
static_assert(sizeof(TScalePara) == 40, "Unexpected TScalePara ABI");
static_assert(sizeof(TMeasureAlgorithmPara) == 64, "Unexpected TMeasureAlgorithmPara ABI");
static_assert(sizeof(TAlgorithmRectFramePara) == 48, "Unexpected TAlgorithmRectFramePara ABI");
static_assert(sizeof(TMeasureLine) == 128, "Unexpected TMeasureLine ABI");

using SetFeatureIdFn = int(__cdecl*)(int);
using InitFn = int(__cdecl*)(TEncryptionData*, TEncryptionData*);
using InitImageFn = int(__cdecl*)(int, int);
using MeasureLineByRectFn = int(__cdecl*)(
    void*,
    TPoint*,
    TMeasureAlgorithmPara*,
    TAlgorithmRectFramePara*,
    TMeasureLine*);
using GetMeasuredPointsFn = int(__cdecl*)(void*);
using MeasureCircleFn = int(__cdecl*)(
    void*,                           // 图像指针
    TPoint*,                         // 相机位置
    TMeasureAlgorithmPara*,          // 算法参数
    TAlgorithmCircleRingFramePara*,  // 圆环框参数
    TImageCircle*,                    // 图像坐标下的圆结果
    TMeasuredCircle*                  // 测量坐标下的圆结果
    );
using GetGrayValueFn = int(__cdecl*)(void*, int, int, int*);
using SetScaleParaFn = int(__cdecl*)(int, TScalePara*);

constexpr int kStructuredExceptionResult =
    (std::numeric_limits<int>::min)();

std::filesystem::path DiagnosticLogPath()
{
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetTempPathW(
        static_cast<DWORD>(buffer.size()),
        buffer.data());
    if (length == 0 || length >= buffer.size()) {
        return L"GviewCT_ZCAlgorithm.log";
    }
    buffer.resize(length);
    return std::filesystem::path(buffer) / L"GviewCT_ZCAlgorithm.log";
}

std::filesystem::path DiagnosticImagePath()
{
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetTempPathW(
        static_cast<DWORD>(buffer.size()),
        buffer.data());
    if (length == 0 || length >= buffer.size()) {
        return L"GviewCT_ZCInput.pgm";
    }
    buffer.resize(length);
    return std::filesystem::path(buffer) / L"GviewCT_ZCInput.pgm";
}

void Trace(const std::string& message)
{
    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::ostringstream stream;
    stream << "[ZC "
        << std::setfill('0')
        << std::setw(2) << time.wHour << ':'
        << std::setw(2) << time.wMinute << ':'
        << std::setw(2) << time.wSecond << '.'
        << std::setw(3) << time.wMilliseconds
        << "] " << message;
    const std::string line = stream.str();

    std::cerr << line << std::endl;
    OutputDebugStringA((line + "\n").c_str());

    std::ofstream log(
        DiagnosticLogPath(),
        std::ios::out | std::ios::app);
    if (log) {
        log << line << '\n';
    }
}

void TraceAndDumpInputImage(
    const ZcGrayImage& image,
    const ZcRectFrame& frame)
{
    std::uint8_t imageMinimum = 255;
    std::uint8_t imageMaximum = 0;
    std::uint64_t imageSum = 0;
    std::uint64_t imageNonzero = 0;
    for (int row = 0; row < image.height; ++row) {
        const auto* source = image.pixels.data()
            + static_cast<std::size_t>(row) * image.widthStep;
        for (int column = 0; column < image.width; ++column) {
            const std::uint8_t value = source[column];
            imageMinimum = std::min(imageMinimum, value);
            imageMaximum = std::max(imageMaximum, value);
            imageSum += value;
            imageNonzero += value != 0;
        }
    }

    const int roiLeft = std::max(
        0,
        static_cast<int>(std::floor(frame.startX)));
    const int roiTop = std::max(
        0,
        static_cast<int>(std::floor(frame.startY)));
    const int roiRight = std::min(
        image.width,
        static_cast<int>(std::ceil(frame.startX + frame.width)));
    const int roiBottom = std::min(
        image.height,
        static_cast<int>(std::ceil(frame.startY + frame.height)));
    std::uint8_t roiMinimum = 255;
    std::uint8_t roiMaximum = 0;
    std::uint64_t roiSum = 0;
    std::uint64_t roiNonzero = 0;
    std::uint64_t roiCount = 0;
    for (int row = roiTop; row < roiBottom; ++row) {
        const auto* source = image.pixels.data()
            + static_cast<std::size_t>(row) * image.widthStep;
        for (int column = roiLeft; column < roiRight; ++column) {
            const std::uint8_t value = source[column];
            roiMinimum = std::min(roiMinimum, value);
            roiMaximum = std::max(roiMaximum, value);
            roiSum += value;
            roiNonzero += value != 0;
            ++roiCount;
        }
    }

    const auto imageCount = static_cast<std::uint64_t>(image.width)
        * static_cast<std::uint64_t>(image.height);
    std::ostringstream statistics;
    statistics << "input gray stats image[min="
        << static_cast<unsigned int>(imageMinimum)
        << " max=" << static_cast<unsigned int>(imageMaximum)
        << " mean="
        << (imageCount == 0
            ? 0.0
            : static_cast<double>(imageSum) / imageCount)
        << " nonzero=" << imageNonzero << '/' << imageCount
        << "] roi[min="
        << (roiCount == 0 ? 0U : static_cast<unsigned int>(roiMinimum))
        << " max="
        << (roiCount == 0 ? 0U : static_cast<unsigned int>(roiMaximum))
        << " mean="
        << (roiCount == 0
            ? 0.0
            : static_cast<double>(roiSum) / roiCount)
        << " nonzero=" << roiNonzero << '/' << roiCount << ']';
    Trace(statistics.str());

    const auto path = DiagnosticImagePath();
    std::ofstream output(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) {
        Trace("cannot write diagnostic input image: " + path.string());
        return;
    }
    output << "P5\n" << image.width << ' ' << image.height << "\n255\n";
    for (int row = 0; row < image.height; ++row) {
        const auto* source = image.pixels.data()
            + static_cast<std::size_t>(row) * image.widthStep;
        output.write(
            reinterpret_cast<const char*>(source),
            image.width);
    }
    Trace("diagnostic input image written: " + path.string());
}

std::string ByteArrayText(const TEncryptionData& value)
{
    std::ostringstream stream;
    stream << '[';
    for (std::size_t i = 0; i < std::size(value.data); ++i) {
        if (i != 0) {
            stream << ',';
        }
        stream << static_cast<unsigned int>(value.data[i]);
    }
    stream << ']';
    return stream.str();
}

std::string StructuredExceptionText(DWORD code)
{
    std::ostringstream stream;
    stream << "Windows exception 0x"
        << std::uppercase << std::hex
        << std::setw(8) << std::setfill('0') << code;
    return stream.str();
}

int ProtectedSetFeatureId(
    SetFeatureIdFn function,
    int featureId,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(featureId);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedInit(
    InitFn function,
    TEncryptionData* challenge,
    TEncryptionData* response,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(challenge, response);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedInitImage(
    InitImageFn function,
    int width,
    int height,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(width, height);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedMeasureLineByRect(
    MeasureLineByRectFn function,
    void* image,
    TPoint* cameraPosition,
    TMeasureAlgorithmPara* algorithmPara,
    TAlgorithmRectFramePara* framePara,
    TMeasureLine* measuredLine,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(
            image,
            cameraPosition,
            algorithmPara,
            framePara,
            measuredLine);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedMeasureCircle(MeasureCircleFn function, void* image, TPoint* camera,
    TMeasureAlgorithmPara* algorithm, TAlgorithmCircleRingFramePara* frame,
    TImageCircle* imageCircle, TMeasuredCircle* machineCircle, DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(image, camera, algorithm, frame, imageCircle, machineCircle);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedMeasureArc(MeasureArcFn function, void* image, TPoint* camera,
    TMeasureAlgorithmPara* algorithm, TAlgorithmArcRingFramePara* frame,
    TMeasuredArc* arc, DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try { return function(image, camera, algorithm, frame, arc); }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedGetMeasuredPoints(
    GetMeasuredPointsFn function,
    void* measuredPoints,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(measuredPoints);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedGetGrayValue(
    GetGrayValueFn function,
    void* image,
    int x,
    int y,
    int* grayValue,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(image, x, y, grayValue);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

int ProtectedSetScalePara(
    SetScaleParaFn function,
    int zoomIndex,
    TScalePara* scalePara,
    DWORD& exceptionCode) noexcept
{
    exceptionCode = 0;
    __try {
        return function(zoomIndex, scalePara);
    }
    __except ((exceptionCode = GetExceptionCode()), EXCEPTION_EXECUTE_HANDLER) {
        return kStructuredExceptionResult;
    }
}

std::string WindowsError(const char* action)
{
    std::ostringstream stream;
    stream << action << " (Windows error " << GetLastError() << ")";
    return stream.str();
}

std::filesystem::path ExecutableDirectory()
{
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

TEncryptionData CreateChallenge()
{
    std::random_device seed;
    std::mt19937 generator(seed());
    std::uniform_int_distribution<int> distribution(2, 254);

    TEncryptionData challenge;
    for (auto& value : challenge.data) {
        value = static_cast<std::uint8_t>(distribution(generator));
    }
    return challenge;
}

TEncryptionData CreateResponse(const TEncryptionData& challenge)
{
    TEncryptionData response;
    for (std::size_t i = 0; i < std::size(challenge.data); ++i) {
        const int a = challenge.data[i];
        const int b = challenge.data[(i + 1) % std::size(challenge.data)];
        const int c = challenge.data[(i + 2) % std::size(challenge.data)];
        response.data[i] = static_cast<std::uint8_t>(
            a * b / c + 2 * a + b + c);
    }
    return response;
}

IplImage MakeImageHeader(const ZcGrayImage& source)
{
    IplImage image{};
    image.nSize = sizeof(IplImage);
    image.nChannels = 1;
    image.depth = IPL_DEPTH_8U;
    std::memcpy(image.colorModel, "GRAY", 4);
    std::memcpy(image.channelSeq, "GRAY", 4);
    image.dataOrder = IPL_DATA_ORDER_PIXEL;
    image.origin = IPL_ORIGIN_TL;
    image.align = 4;
    image.width = source.width;
    image.height = source.height;
    image.imageSize = source.widthStep * source.height;
    image.imageData = reinterpret_cast<char*>(
        const_cast<std::uint8_t*>(source.pixels.data()));
    image.widthStep = source.widthStep;
    image.imageDataOrigin = image.imageData;
    return image;
}

} 

class ZcEdgeAlgorithm::Impl final {
public:
    explicit Impl(Options options)
        : m_options(std::move(options))
    {
    }

    ~Impl()
    {
        if (m_module) {
            FreeLibrary(m_module);
        }
    }

    bool MeasureArcByArcRing(const ZcGrayImage& image, const ZcArcRingFrame& frame,
        ZcMeasuredArc& arc, std::string& error)
    {
        arc = {}; error.clear();
        constexpr double tau = 6.283185307179586;
        const double sweep = frame.endAngle - frame.startAngle;
        if (image.width <= 0 || image.height <= 0 || image.widthStep < image.width
            || image.pixels.size() < static_cast<size_t>(image.widthStep) * image.height
            || !std::isfinite(frame.x) || !std::isfinite(frame.y)
            || !std::isfinite(frame.innerRadius) || !std::isfinite(frame.outerRadius)
            || !std::isfinite(frame.startAngle) || !std::isfinite(frame.endAngle)
            || sweep <= 0 || sweep >= tau || frame.innerRadius <= 0
            || frame.outerRadius <= frame.innerRadius
            || frame.x - frame.outerRadius < 0 || frame.y - frame.outerRadius < 0
            || frame.x + frame.outerRadius > image.width - 1
            || frame.y + frame.outerRadius > image.height - 1) {
            error = "invalid arc image or ring"; return false;
        }
        if (!EnsureReady(image.width, image.height, error)) return false;
        if (!m_measureArc && !Resolve("ZC_MeasureArcByArcRing", m_measureArc, error)) return false;
        IplImage header = MakeImageHeader(image);
        TPoint camera{};
        TMeasureAlgorithmPara algorithm{};
        // UI and validation remain in radians; convert only at the DLL boundary.
        constexpr double radiansToDegrees = 360.0 / tau;
        TAlgorithmArcRingFramePara ring{frame.x, frame.y, frame.innerRadius,
            frame.outerRadius, 0, frame.startAngle * radiansToDegrees,
            frame.endAngle * radiansToDegrees};
        TMeasuredArc measured{};
        DWORD code = 0;
        std::ostringstream begin;
        begin << "ZC_MeasureArcByArcRing begin center=(" << ring.x << ',' << ring.y
            << ") radii=" << ring.r1 << ',' << ring.r2 << " angles(deg)=" << ring.angle1 << ',' << ring.angle2;
        Trace(begin.str());
        const int result = ProtectedMeasureArc(m_measureArc, &header, &camera, &algorithm, &ring, &measured, code);
        std::ostringstream output;
        output << "ZC_MeasureArcByArcRing returned " << result << " center=(" << measured.x
            << ',' << measured.y << ") r=" << measured.r << " points=" << measured.pointsCount;
        Trace(output.str());
        if (result != 0) {
            error = result == kStructuredExceptionResult ? StructuredExceptionText(code)
                : "ZC_MeasureArcByArcRing returned " + std::to_string(result);
            return false;
        }
        if (measured.pointsCount < 3 || measured.pointsCount > 4096
            || !std::isfinite(measured.x) || !std::isfinite(measured.y)
            || !std::isfinite(measured.r) || measured.r <= 0) {
            error = "invalid measured arc or point count"; return false;
        }
        std::vector<TMeasuredPoint2D> points(4096);
        const int pointsResult = ProtectedGetMeasuredPoints(m_getMeasuredPoints, points.data(), code);
        if (pointsResult != 0) {
            error = "arc ZC_GetMeasuredPoints failed: " + std::to_string(pointsResult); return false;
        }
        points.resize(measured.pointsCount);
        const auto map = [&](TMeasuredPoint2D p, bool centered) {
            return centered ? TMeasuredPoint2D{p.x + image.width * 0.5, image.height * 0.5 - p.y} : p;
        };
        const auto phase = [&](double angle) {
            double a = std::fmod(angle - frame.startAngle, tau);
            if (a < 0) a += tau;
            return a > tau - 1e-8 ? 0.0 : a;
        };
        const auto inside = [&](TMeasuredPoint2D p) {
            const double radius = std::hypot(p.x - frame.x, p.y - frame.y);
            return std::isfinite(radius) && radius >= frame.innerRadius - 2
                && radius <= frame.outerRadius + 2
                && phase(std::atan2(p.y - frame.y, p.x - frame.x)) <= sweep + 0.02;
        };
        int raw = 0, centered = 0;
        for (auto p : points) { raw += inside(p); centered += inside(map(p, true)); }
        const bool useCentered = centered > raw;
        if (std::max(raw, centered) < measured.pointsCount * 0.9) {
            error = "arc points outside requested sector; verify angular direction and coordinates"; return false;
        }
        const auto center = map({measured.x, measured.y}, useCentered);
        double first = tau, last = 0;
        for (auto p : points) {
            p = map(p, useCentered);
            if (!inside(p)) continue;
            const double a = phase(std::atan2(p.y - center.y, p.x - center.x));
            first = std::min(first, a); last = std::max(last, a);
        }
        if (last - first < 1e-4 || last - first > sweep + 0.1) {
            error = "degenerate or inconsistent measured arc"; return false;
        }
        for (int i = 0; i <= 180; ++i) {
            const double a = frame.startAngle + first + (last - first) * i / 180.0;
            TMeasuredPoint2D p{center.x + measured.r * std::cos(a), center.y + measured.r * std::sin(a)};
            if (!inside(p)) { error = "fitted arc outside requested sector"; return false; }
            arc.path.push_back({p.x, p.y});
        }
        arc.measuredPointsCount = measured.pointsCount;
        Trace(useCentered ? "arc coordinates: centered unit scale" : "arc coordinates: image pixels");
        return true;
    }

    bool MeasureCircleByCircleRing(
        const ZcGrayImage& image,
        const ZcCircleRingFrame& frame,
        ZcMeasuredCircle& circle,
        std::string& error)
    {
        error.clear();
        circle = {};

        // 1. 检查输入图像
        if (image.width <= 0 || image.height <= 0
            || image.widthStep < image.width
            || image.pixels.size()
            < static_cast<std::size_t>(image.widthStep) * image.height) {
            error = "invalid circle input image";
            return false;
        }

        // 2. 检查圆环参数及图像边界
        if (!std::isfinite(frame.x)
            || !std::isfinite(frame.y)
            || !std::isfinite(frame.innerRadius)
            || !std::isfinite(frame.outerRadius)
            || frame.innerRadius <= 0
            || frame.outerRadius <= frame.innerRadius
            || frame.x - frame.outerRadius < 0
            || frame.y - frame.outerRadius < 0
            || frame.x + frame.outerRadius > image.width - 1
            || frame.y + frame.outerRadius > image.height - 1) {
            error = "invalid circle ring bounds";
            return false;
        }

        // 3. 初始化并获取 DLL 函数
        if (!EnsureReady(image.width, image.height, error)) {
            return false;
        }

        if (!m_measureCircle
            && !Resolve("ZC_MeasureCircleByCircleRing", m_measureCircle, error)) {
            return false;
        }

        // 4. 准备 DLL 输入和输出
        IplImage header = MakeImageHeader(image);
        TPoint camera{};
        TMeasureAlgorithmPara algorithm{};
        TAlgorithmCircleRingFramePara ring{
            frame.x, frame.y, frame.innerRadius, frame.outerRadius
        };
        TImageCircle imageCircle{};
        TMeasuredCircle machineCircle{};

        // 5. 调用 DLL
        const int result = m_measureCircle(
            &header, &camera, &algorithm,
            &ring, &imageCircle, &machineCircle);

        if (result != 0) {
            error = "ZC_MeasureCircleByCircleRing returned "
                + std::to_string(result);
            return false;
        }

        // 6. 检查返回圆是否有效
        if (!std::isfinite(imageCircle.x)
            || !std::isfinite(imageCircle.y)
            || !std::isfinite(imageCircle.r)
            || imageCircle.r <= 0) {
            error = "invalid DLL image circle";
            return false;
        }

        // 保留原来的范围检查：圆周应位于圆环内，允许 2 像素
        const double offset = std::hypot(
            imageCircle.x - ring.x,
            imageCircle.y - ring.y);

        constexpr double tolerance = 2.0;
        if (imageCircle.r - offset < ring.r1 - tolerance
            || imageCircle.r + offset > ring.r2 + tolerance) {
            error = "DLL image circle is outside the pixel ring";
            return false;
        }

        // 7. 输出图像坐标下的圆
        circle = {
            imageCircle.x,
            imageCircle.y,
            imageCircle.r,
            machineCircle.pointsCount
        };
        return true;
    }

    bool MeasureLineByRect(
        const ZcGrayImage& image,
        const ZcRectFrame& frame,
        ZcMeasuredLine& line,
        std::string& error)
    {
        error.clear();
        if (image.width <= 0
            || image.height <= 0
            || image.widthStep < image.width
            || image.pixels.size()
                < static_cast<std::size_t>(image.widthStep) * image.height) {
            error = "invalid 8-bit grayscale image";
            return false;
        }
        if (frame.width < 1.0 || frame.height < 1.0) {
            error = "the edge ROI is too small";
            return false;
        }
        //加载 DLL，并完成加密、图像和比例初始化
        if (!EnsureReady(image.width, image.height, error)) {
            return false;
        }

        TraceAndDumpInputImage(image, frame);

        IplImage imageHeader = MakeImageHeader(image);
        const int probeX = std::max(
            0,
            std::min(
                image.width - 1,
                static_cast<int>(std::lround(frame.startX + frame.width * 0.5))));
        const int probeY = std::max(
            0,
            std::min(
                image.height - 1,
                static_cast<int>(std::lround(frame.startY + frame.height * 0.5))));
        const int localGray = image.pixels[
            static_cast<std::size_t>(probeY) * image.widthStep + probeX];
        int dllGray = -1;
        DWORD probeExceptionCode = 0;
        const int probeResult = ProtectedGetGrayValue(
            m_getGrayValue,
            &imageHeader,
            probeX,
            probeY,
            &dllGray,
            probeExceptionCode);
        std::ostringstream probeDetails;
        probeDetails << "ZC_GetGrayValue probe=(" << probeX << ',' << probeY
            << ") local=" << localGray
            << " dll=" << dllGray
            << " result=" << probeResult;
        if (probeResult == kStructuredExceptionResult) {
            probeDetails << " exception="
                << StructuredExceptionText(probeExceptionCode);
        }
        Trace(probeDetails.str());

        // 准备与 DLL 对应的结构体
        TPoint cameraPosition{};
        TMeasureAlgorithmPara algorithmPara{};
        TAlgorithmRectFramePara framePara;
        framePara.startX = frame.startX;
        framePara.startY = frame.startY;
        framePara.width = frame.width;
        framePara.height = frame.height;
        framePara.cosAngle = frame.cosAngle;
        framePara.sinAngle = frame.sinAngle;
        TMeasureLine measuredLine{};

        std::ostringstream callDetails;
        callDetails << "ZC_MeasureLineByRect begin image="
            << image.width << 'x' << image.height
            << " step=" << image.widthStep
            << " roi=(" << frame.startX << ',' << frame.startY
            << ',' << frame.width << ',' << frame.height << ')';
        Trace(callDetails.str());

        DWORD exceptionCode = 0;
        //调用 DLL 抓直线
        const int result = ProtectedMeasureLineByRect(
            m_measureLineByRect,
            &imageHeader,
            &cameraPosition,
            &algorithmPara,
            &framePara,
            &measuredLine,
            exceptionCode);
        if (result == kStructuredExceptionResult) {
            error = "ZC_MeasureLineByRect raised "
                + StructuredExceptionText(exceptionCode);
            Trace(error);
            return false;
        }
        Trace("ZC_MeasureLineByRect returned " + std::to_string(result));
        if (result != 0) {
            error = "ZC_MeasureLineByRect returned " + std::to_string(result);
            return false;
        }

        std::ostringstream resultDetails;
        resultDetails << "machine line center=("
            << measuredLine.x << ',' << measuredLine.y << ',' << measuredLine.z
            << ") endpoints=("
            << measuredLine.x1 << ',' << measuredLine.y1 << ',' << measuredLine.z1
            << ")->("
            << measuredLine.x2 << ',' << measuredLine.y2 << ',' << measuredLine.z2
            << ") length=" << measuredLine.length
            << " direction=" << measuredLine.direction
            << " points_count=" << measuredLine.pointsCount
            << " straightness=" << measuredLine.straightness;
        Trace(resultDetails.str());

        constexpr int kMaximumMeasuredPoints = 4096;
        if (measuredLine.pointsCount < 2
            || measuredLine.pointsCount > kMaximumMeasuredPoints) {
            error = "invalid measured points count: "
                + std::to_string(measuredLine.pointsCount);
            Trace(error);
            return false;
        }
        
        std::vector<TMeasuredPoint2D> measuredPoints(4096);//放在堆上 C#端为该接口准备的是最多 4096 个、每个 16 字节的二维点缓冲区。
        Trace("ZC_GetMeasuredPoints begin capacity=4096 stride=16");
        exceptionCode = 0;
        // 根据pointsCount获取 DLL的采样点
        const int pointsResult = ProtectedGetMeasuredPoints(
            m_getMeasuredPoints,
            measuredPoints.data(),
            exceptionCode);
        if (pointsResult == kStructuredExceptionResult) {
            error = "ZC_GetMeasuredPoints raised "
                + StructuredExceptionText(exceptionCode);
            Trace(error);
            return false;
        }
        Trace("ZC_GetMeasuredPoints returned " + std::to_string(pointsResult));
        if (pointsResult != 0) {
            error = "ZC_GetMeasuredPoints returned "
                + std::to_string(pointsResult);
            return false;
        }

        double meanX = 0.0;
        double meanY = 0.0;
        double minimumX = (std::numeric_limits<double>::max)();
        double maximumX = (std::numeric_limits<double>::lowest)();
        double minimumY = (std::numeric_limits<double>::max)();
        double maximumY = (std::numeric_limits<double>::lowest)();
        for (int i = 0; i < measuredLine.pointsCount; ++i) {
            const auto& point = measuredPoints[static_cast<std::size_t>(i)];
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                error = "ZC_GetMeasuredPoints returned a non-finite point at index "
                    + std::to_string(i);
                Trace(error);
                return false;
            }
            meanX += point.x;
            meanY += point.y;
            minimumX = std::min(minimumX, point.x);
            maximumX = std::max(maximumX, point.x);
            minimumY = std::min(minimumY, point.y);
            maximumY = std::max(maximumY, point.y);
        }
        meanX /= measuredLine.pointsCount;
        meanY /= measuredLine.pointsCount;

        std::ostringstream pointsDetails;
        pointsDetails << "measured points first=("
            << measuredPoints.front().x << ',' << measuredPoints.front().y
            << ") last=("
            << measuredPoints[static_cast<std::size_t>(measuredLine.pointsCount - 1)].x
            << ','
            << measuredPoints[static_cast<std::size_t>(measuredLine.pointsCount - 1)].y
            << ") bounds=(" << minimumX << ',' << minimumY
            << ")->(" << maximumX << ',' << maximumY << ')';
        Trace(pointsDetails.str());

        //  用采样点重新拟合直线
        //  将结果转换为图像像素端点，填入 line
        double covarianceXX = 0.0;
        double covarianceXY = 0.0;
        double covarianceYY = 0.0;
        for (int i = 0; i < measuredLine.pointsCount; ++i) {
            const auto& point = measuredPoints[static_cast<std::size_t>(i)];
            const double dx = point.x - meanX;
            const double dy = point.y - meanY;
            covarianceXX += dx * dx;
            covarianceXY += dx * dy;
            covarianceYY += dy * dy;
        }
        if (covarianceXX + covarianceYY <= std::numeric_limits<double>::epsilon()) {
            error = "ZC_GetMeasuredPoints returned coincident points";
            Trace(error);
            return false;
        }

        const double angle = 0.5 * std::atan2(
            2.0 * covarianceXY,
            covarianceXX - covarianceYY);
        const double directionX = std::cos(angle);
        const double directionY = std::sin(angle);
        double minimumProjection = (std::numeric_limits<double>::max)();
        double maximumProjection = (std::numeric_limits<double>::lowest)();
        for (int i = 0; i < measuredLine.pointsCount; ++i) {
            const auto& point = measuredPoints[static_cast<std::size_t>(i)];
            const double projection = (point.x - meanX) * directionX+ (point.y - meanY) * directionY;
            minimumProjection = std::min(minimumProjection, projection);
            maximumProjection = std::max(maximumProjection, projection);
        }

        const double rawX1 = meanX + minimumProjection * directionX;
        const double rawY1 = meanY + minimumProjection * directionY;
        const double rawX2 = meanX + maximumProjection * directionX;
        const double rawY2 = meanY + maximumProjection * directionY;

        // ZC_SetScalePara 使用单位比例时，旧算法通常返回以图像中心为
        // 原点、Y 轴向上的坐标。若原始点本身已是整图像素坐标则保持原值。
        const auto insideFrame = [&frame](double x, double y) {
            constexpr double tolerance = 3.0;
            return x >= frame.startX - tolerance
                && x <= frame.startX + frame.width + tolerance
                && y >= frame.startY - tolerance
                && y <= frame.startY + frame.height + tolerance;
        };
        int rawInsideCount = 0;
        int centeredInsideCount = 0;
        for (int i = 0; i < measuredLine.pointsCount; ++i) {
            const auto& point = measuredPoints[static_cast<std::size_t>(i)];
            rawInsideCount += insideFrame(point.x, point.y);
            centeredInsideCount += insideFrame(
                point.x + image.width * 0.5,
                image.height * 0.5 - point.y);
        }
        if (centeredInsideCount > rawInsideCount) {
            line.x1 = rawX1 + image.width * 0.5;
            line.y1 = image.height * 0.5 - rawY1;
            line.x2 = rawX2 + image.width * 0.5;
            line.y2 = image.height * 0.5 - rawY2;
            Trace("measured-points coordinates interpreted as centered machine coordinates"
                " raw_inside=" + std::to_string(rawInsideCount)
                + " centered_inside=" + std::to_string(centeredInsideCount));
        }
        else {
            line.x1 = rawX1;
            line.y1 = rawY1;
            line.x2 = rawX2;
            line.y2 = rawY2;
            Trace("measured-points coordinates interpreted as image pixels"
                " raw_inside=" + std::to_string(rawInsideCount)
                + " centered_inside=" + std::to_string(centeredInsideCount));
        }
        line.measuredPointsCount = measuredLine.pointsCount;
        std::ostringstream fittedDetails;
        fittedDetails << "fitted measured-points line=("
            << line.x1 << ',' << line.y1 << ")->("
            << line.x2 << ',' << line.y2 << ')';
        Trace(fittedDetails.str());
        return true;
    }

private:
    template <typename Function>
    bool Resolve(const char* name, Function& function, std::string& error)
    {
        function = reinterpret_cast<Function>(GetProcAddress(m_module, name));
        if (!function) {
            error = std::string("missing export: ") + name;
            Trace(error);
            return false;
        }
        Trace(std::string("resolved export: ") + name);
        return true;
    }

    bool Load(std::string& error)
    {
        if (m_module) {
            return true;
        }
        const auto dllPath = ExecutableDirectory() / L"ZCAlgorithm.dll";
        Trace("loading DLL: " + dllPath.string());
        m_module = LoadLibraryExW(
            dllPath.c_str(),
            nullptr,
            LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!m_module) {
            error = WindowsError("cannot load ZCAlgorithm.dll");
            Trace(error);
            return false;
        }
        Trace("ZCAlgorithm.dll loaded successfully");
        return Resolve("ZC_SetFeatureID", m_setFeatureId, error)
            && Resolve("ZC_Init", m_init, error)
            && Resolve("ZC_InitImage", m_initImage, error)
            && Resolve("ZC_MeasureLineByRect", m_measureLineByRect, error)
            && Resolve("ZC_GetMeasuredPoints", m_getMeasuredPoints, error)
            && Resolve("ZC_GetGrayValue", m_getGrayValue, error)
            && Resolve("ZC_SetScalePara", m_setScalePara, error);
    }

    bool EnsureReady(int width, int height, std::string& error)
    {
        if (!Load(error)) {
            return false;
        }

        if (!m_initialized) {//初始化加密狗
            Trace("ZC_SetFeatureID begin FeatureID="
                + std::to_string(m_options.featureId));
            DWORD exceptionCode = 0;
            const int featureResult = ProtectedSetFeatureId(
                m_setFeatureId,
                m_options.featureId,
                exceptionCode);
            if (featureResult == kStructuredExceptionResult) {
                error = "ZC_SetFeatureID raised "
                    + StructuredExceptionText(exceptionCode);
                Trace(error);
                return false;
            }
            Trace("ZC_SetFeatureID returned " + std::to_string(featureResult));
            if (featureResult != 0) {
                error = "ZC_SetFeatureID returned "
                    + std::to_string(featureResult)
                    + " (FeatureID="
                    + std::to_string(m_options.featureId)
                    + ")";
                return false;
            }

            auto challenge = CreateChallenge();
            auto response = CreateResponse(challenge);
            Trace("ZC_Init begin challenge=" + ByteArrayText(challenge)
                + " response=" + ByteArrayText(response));
            const int initResult = ProtectedInit(
                m_init,
                &challenge,
                &response,
                exceptionCode);
            if (initResult == kStructuredExceptionResult) {
                error = "ZC_Init raised "
                    + StructuredExceptionText(exceptionCode);
                Trace(error);
                return false;
            }
            Trace("ZC_Init returned " + std::to_string(initResult));
            if (initResult != 0) {
                error = "ZC_Init returned " + std::to_string(initResult);
                return false;
            }
            m_initialized = true;
        }

        if (m_imageWidth != width || m_imageHeight != height) {
            Trace("ZC_InitImage begin width=" + std::to_string(width)
                + " height=" + std::to_string(height));
            DWORD exceptionCode = 0;
            const int imageResult = ProtectedInitImage(
                m_initImage,
                width,
                height,
                exceptionCode);
            if (imageResult == kStructuredExceptionResult) {
                error = "ZC_InitImage raised "
                    + StructuredExceptionText(exceptionCode);
                Trace(error);
                return false;
            }
            Trace("ZC_InitImage returned " + std::to_string(imageResult));
            if (imageResult != 0) {
                error = "ZC_InitImage returned " + std::to_string(imageResult);
                return false;
            }
            m_imageWidth = width;
            m_imageHeight = height;
        }

        if (!m_scaleInitialized) {
            // 当前模块只取 DLL 的抓边位置用于界面叠加，不输出相机标定后的
            // 计量结果。因此使用单位比例，后续再将中心坐标还原为图像像素。
            std::string magnification = "1";
            TScalePara scalePara;
            scalePara.magnification = magnification.c_str();
            Trace("ZC_SetScalePara begin zoom=0 x_scale=1 y_scale=1"
                " angle=0 calibrated=true magnification=1");
            DWORD exceptionCode = 0;
            const int scaleResult = ProtectedSetScalePara(
                m_setScalePara,
                0,
                &scalePara,
                exceptionCode);
            if (scaleResult == kStructuredExceptionResult) {
                error = "ZC_SetScalePara raised "
                    + StructuredExceptionText(exceptionCode);
                Trace(error);
                return false;
            }
            Trace("ZC_SetScalePara returned " + std::to_string(scaleResult));
            if (scaleResult != 0) {
                error = "ZC_SetScalePara returned "
                    + std::to_string(scaleResult);
                return false;
            }
            m_scaleInitialized = true;
        }
        return true;
    }

    Options m_options;
    HMODULE m_module = nullptr;
    SetFeatureIdFn m_setFeatureId = nullptr;
    InitFn m_init = nullptr;
    InitImageFn m_initImage = nullptr;
    MeasureLineByRectFn m_measureLineByRect = nullptr;
    MeasureCircleFn m_measureCircle = nullptr;
    MeasureArcFn m_measureArc = nullptr;
    GetMeasuredPointsFn m_getMeasuredPoints = nullptr;
    GetGrayValueFn m_getGrayValue = nullptr;
    SetScaleParaFn m_setScalePara = nullptr;
    bool m_initialized = false;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    bool m_scaleInitialized = false;
};

ZcEdgeAlgorithm::ZcEdgeAlgorithm(Options options)
    : m_impl(std::make_unique<Impl>(std::move(options)))
{
}

ZcEdgeAlgorithm::~ZcEdgeAlgorithm() = default;

bool ZcEdgeAlgorithm::MeasureArcByArcRing(const ZcGrayImage& image, const ZcArcRingFrame& frame,
    ZcMeasuredArc& arc, std::string& error)
{
    try { return m_impl->MeasureArcByArcRing(image, frame, arc, error); }
    catch (const std::exception& e) { error = e.what(); }
    catch (...) { error = "unknown arc measurement exception"; }
    Trace(error); return false;
}

bool ZcEdgeAlgorithm::MeasureCircleByCircleRing(const ZcGrayImage& image,
    const ZcCircleRingFrame& frame, ZcMeasuredCircle& circle, std::string& error)
{
    try {
        return m_impl->MeasureCircleByCircleRing(image, frame, circle, error);
    }
    catch (const std::exception& exception) {
        error = std::string("C++ exception: ") + exception.what();
    }
    catch (...) { error = "unknown C++ exception in circle measurement"; }
    Trace(error);
    return false;
}

bool ZcEdgeAlgorithm::MeasureLineByRect(
    const ZcGrayImage& image,
    const ZcRectFrame& frame,
    ZcMeasuredLine& line,
    std::string& error)
{
    try {
        return m_impl->MeasureLineByRect(image, frame, line, error);
    }
    catch (const std::exception& exception) {
        error = std::string("C++ exception: ") + exception.what();
        Trace(error);
        return false;
    }
    catch (...) {
        error = "unknown C++ exception";
        Trace(error);
        return false;
    }
}

} // namespace measure

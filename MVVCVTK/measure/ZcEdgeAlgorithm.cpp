#include "measure/ZcEdgeAlgorithm.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <opencv2/core/types_c.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
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
    // C# bool 字段默认按 4 字节 BOOL 传递。
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
#pragma pack(pop)

static_assert(sizeof(TEncryptionData) == 8, "Unexpected TEncryptionData ABI");
static_assert(sizeof(TPoint) == 24, "Unexpected TPoint ABI");
static_assert(sizeof(TMeasuredPoint2D) == 16, "Unexpected measured-point ABI");
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
        if (!EnsureReady(image.width, image.height, error)) {
            return false;
        }

        TraceAndDumpInputImage(image, frame);

        IplImage imageHeader = MakeImageHeader(image);
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

        // C# 端为该接口准备的是最多 4096 个、每个 16 字节的二维点缓冲区。
        std::array<TMeasuredPoint2D, kMaximumMeasuredPoints> measuredPoints{};
        Trace("ZC_GetMeasuredPoints begin capacity=4096 stride=16");
        exceptionCode = 0;
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

        // 用全部测量点做二维正交最小二乘拟合，并用投影范围确定线段端点。
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
            const double projection = (point.x - meanX) * directionX
                + (point.y - meanY) * directionY;
            minimumProjection = std::min(minimumProjection, projection);
            maximumProjection = std::max(maximumProjection, projection);
        }

        line.x1 = meanX + minimumProjection * directionX;
        line.y1 = meanY + minimumProjection * directionY;
        line.x2 = meanX + maximumProjection * directionX;
        line.y2 = meanY + maximumProjection * directionY;
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
            && Resolve("ZC_GetMeasuredPoints", m_getMeasuredPoints, error);
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
        return true;
    }

    Options m_options;
    HMODULE m_module = nullptr;
    SetFeatureIdFn m_setFeatureId = nullptr;
    InitFn m_init = nullptr;
    InitImageFn m_initImage = nullptr;
    MeasureLineByRectFn m_measureLineByRect = nullptr;
    GetMeasuredPointsFn m_getMeasuredPoints = nullptr;
    bool m_initialized = false;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
};

ZcEdgeAlgorithm::ZcEdgeAlgorithm(Options options)
    : m_impl(std::make_unique<Impl>(std::move(options)))
{
}

ZcEdgeAlgorithm::~ZcEdgeAlgorithm() = default;

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

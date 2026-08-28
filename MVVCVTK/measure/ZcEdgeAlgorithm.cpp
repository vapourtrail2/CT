#include "measure/ZcEdgeAlgorithm.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <opencv2/core/types_c.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
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
static_assert(sizeof(TMeasureAlgorithmPara) == 64, "Unexpected TMeasureAlgorithmPara ABI");
static_assert(sizeof(TAlgorithmRectFramePara) == 48, "Unexpected TAlgorithmRectFramePara ABI");
static_assert(sizeof(TMeasureLine) == 128, "Unexpected TMeasureLine ABI");

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

        const int result = m_measureLineByRect(
            &imageHeader,
            &cameraPosition,
            &algorithmPara,
            &framePara,
            &measuredLine);
        if (result != 0) {
            error = "ZC_MeasureLineByRect returned " + std::to_string(result);
            return false;
        }

        line.x1 = measuredLine.x1;
        line.y1 = measuredLine.y1;
        line.x2 = measuredLine.x2;
        line.y2 = measuredLine.y2;
        return true;
    }

private:
    using SetFeatureIdFn = int(__cdecl*)(int);
    using InitFn = int(__cdecl*)(TEncryptionData*, TEncryptionData*);
    using InitImageFn = int(__cdecl*)(int, int);
    using MeasureLineByRectFn = int(__cdecl*)(
        void*,
        TPoint*,
        TMeasureAlgorithmPara*,
        TAlgorithmRectFramePara*,
        TMeasureLine*);

    template <typename Function>
    bool Resolve(const char* name, Function& function, std::string& error)
    {
        function = reinterpret_cast<Function>(GetProcAddress(m_module, name));
        if (!function) {
            error = std::string("missing export: ") + name;
            return false;
        }
        return true;
    }

    bool Load(std::string& error)
    {
        if (m_module) {
            return true;
        }
        const auto dllPath = ExecutableDirectory() / L"ZCAlgorithm.dll";
        m_module = LoadLibraryExW(
            dllPath.c_str(),
            nullptr,
            LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!m_module) {
            error = WindowsError("cannot load ZCAlgorithm.dll");
            return false;
        }
        return Resolve("ZC_SetFeatureID", m_setFeatureId, error)
            && Resolve("ZC_Init", m_init, error)
            && Resolve("ZC_InitImage", m_initImage, error)
            && Resolve("ZC_MeasureLineByRect", m_measureLineByRect, error);
    }

    bool EnsureReady(int width, int height, std::string& error)
    {
        if (!Load(error)) {
            return false;
        }
        if (!m_initialized) {
            const int featureResult = m_setFeatureId(m_options.featureId);
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
            const int initResult = m_init(&challenge, &response);
            if (initResult != 0) {
                error = "ZC_Init returned " + std::to_string(initResult);
                return false;
            }
            m_initialized = true;
        }

        if (m_imageWidth != width || m_imageHeight != height) {
            const int imageResult = m_initImage(width, height);
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
    return m_impl->MeasureLineByRect(image, frame, line, error);
}

} // namespace measure

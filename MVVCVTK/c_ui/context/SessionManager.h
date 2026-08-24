#pragma once

#include "Host/Types/HostRequestTypes.h"
#include "Host/VtkAppHostSession.h"
#include "App/AppInterfaces.h"
#include "Host/CropHostFeature.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <QObject>
#include <QString>
#include <QTimer>

struct HostRequest;
class VtkAppHostSession;

struct CropTreeItem final
{
    QString text;
    std::size_t nodeCount = 0;
    bool isCurrent = false;
    bool isApplied = false;
    bool isSelectable = false;
};

struct CropTreeState final
{
    std::vector<CropTreeItem> items;
    bool isCropping = false;
    bool isBuilding = false;
    bool canApply = false;
    bool canRestoreOriginal = false;
};

class SessionManager final /*wsm*/ : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Empty,
        Loading,
        Ready,
        Failed
    };
    Q_ENUM(State)

    explicit SessionManager(QObject* parent = nullptr);
    ~SessionManager() override;
    
    State getState() const noexcept
    {
        return state_;
    }
    bool gethasData() const noexcept 
    {
        return state_ == State::Ready;
    }
    QString getSourcePath() const noexcept {
        return sourcePath_;
    }

    ImageSnapshot getImageSnapshot();

    std::optional<HostRenderViewState> getRenderViewState(
        const HostViewTarget& target);

    std::optional<std::array<double, 16>> getRenderViewModelMatrix(
        const std::string& viewId);

    bool initHost(HostSessionConfig config, QString* err = nullptr);

    bool openFile(const QString& path,
        const std::array<float, 3>& spacing,
        const std::array<float,3>& origin,
        QString* errorOut = nullptr
        );

    bool openReconstructedData(
        std::vector<float>&& voxels,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        const QString& sourcePath,
        QString* errorOut = nullptr);

    bool sendRequest(
        HostRequest&& request,
        HostCompleteCallback onComplete = nullptr);

    bool startBoxCrop();
    bool startPlaneCrop();
    bool keepCropInside();
    bool removeCropInside();
    bool setCropNode(std::size_t nodeCount);
    bool applyCrop();
    bool restoreOriginalCrop();
    bool exitCrop();
    CropTreeState getCropTreeState() const;
    void clearSession();

signals:
    void sessionChanged(SessionManager::State state);
    void loadFinished(bool issucc, QString message);
    void isoStateChanged(
        double isoValue,
        double scalarMin,
        double scalarMax);
    void isoStateCleared();
    void cropHistoryChanged();
    void cropBuildFinished(bool isSuccess, QString message);

private:
    bool resetHost(QString* errorOut);
    bool resetCropFeature(QString* errorOut = nullptr);
    void clearCropFeature();
    CropHostTarget getCropTarget() const;
    bool sendCropAction(CropHostRequest request);
    void syncCropHistory();
    void clearCropHistory();
    bool sendLoadRequest(
        HostRequest&& request,
        const QString& sourcePath,
        QString* errorOut);
    void finishLoadRequest(std::uint64_t generation, bool isSuccess);
    void setIsoState();
    void setState(State state);

private:
    HostSessionConfig config_;
    bool hasConfig_ = false;
    std::unique_ptr<VtkAppHostSession> hostSession_;
    std::shared_ptr<CropHostFeature> cropFeature_;
    State state_ = State::Empty;
    std::uint64_t requestGeneration_ = 0;
    QString sourcePath_;
    QString pendingSourcePath_;

    enum class CropRecordShape
    {
        Box,
        Plane
    };

    struct CropRecord final
    {
        CropRecordShape shape = CropRecordShape::Box;
        CropRemovalMode removalMode = CropRemovalMode::KeepInside;
    };

    std::vector<CropRecord> cropRecords_;
    CropRecordShape pendingCropShape_ = CropRecordShape::Box;
    CropRemovalMode pendingCropMode_ = CropRemovalMode::KeepInside;
    bool cropBuildPending_ = false;
    bool hasOriginalCrop_ = false;
    QTimer cropStateTimer_;
};

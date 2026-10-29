#pragma once
#include <map>

#include "MVVCVTK/API/Features/GapAnalysis/Host/GapHostFeature.h"
#include "MVVCVTK/API/Features/OrthogonalCrop/Host/CropHostFeature.h"
#include "MVVCVTK/SPI/Host/TrustedDataPort.h"
#include "MVVCVTK/API/Host/Types/HostRequestTypes.h"
#include "MVVCVTK/API/Host/VtkAppHostSession.h"
#include "measure/MeasureHostBridge.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <functional>
#include <set>
#include <atomic>
#include <future>

struct HostRequest;
class VtkAppHostSession;
class VolumePublishBridge;

struct CropTreeItem final
{
    QString text;
    CropNodeId nodeId = 0;
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

class SessionManager final : public QObject
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
    VtkImageGridSnapshot capturedVolumeSnapshot(QString* err = nullptr);
    //publish
    VtkImageGridSnapshot publishVolumeSnapshotForTest(
        const VtkImageGridSnapshot& archive,
        QString* err = nullptr, bool* isPending = nullptr);
    bool areAllRenderViewsAtVersion(
        const DataRevisionRef& expectedRevision,
        DataBindingRevision expectedBindingRevision,
        std::map<std::string, std::uint64_t>& renderTargets,
        QString* report = nullptr);
    void failVolumePublishTest(const QString& reason);
    std::optional<measure::MeasureHostData> getMeasureHostData(
        const HostViewTarget& sourceView);
    std::optional<HostRenderViewState> getRenderViewState(
        const HostViewTarget& target);
    bool initHost(HostSessionConfig config, QString* err = nullptr);
    bool Stop(QString* errorOut = nullptr);
    bool GetIsStopped() const;
    void SetViewVisibility(std::function<bool(const std::string&)> getVisible);
    void SendUpdates();
    void SendRender();
    void RequestRender();
    bool openData(const QString& path,
        const std::array<int,3>& dims,
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
        HostResultCallback onComplete = {});

    bool startBoxCrop();
    bool startPlaneCrop();
    bool keepCropInside();
    bool removeCropInside();
    bool setCropNode(CropNodeId nodeId);
    bool applyCrop();
    bool restoreOriginalCrop();
    bool exitCrop();
    CropTreeState getCropTreeState() const;

    bool startGap(
        GapHostStartParams params,
        GapHostCallback onComplete = nullptr,
        QString* errorOut = nullptr);
    bool toggleGapOverlay(QString* errorOut = nullptr);
    bool exportGapCsv(
        const QString& outputPath,
        std::function<void(bool, QString)> onComplete,
        QString* errorOut = nullptr);
    bool getIsGapExporting() const;
    bool exitGap(QString* errorOut = nullptr);
    GapHostState getGapState() const;
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
    void hostError(QString message);

private:
    bool resetHost(QString* errorOut);
    bool StopHost(QString* errorOut = nullptr);
    bool resetCropFeature(QString* errorOut = nullptr);
    bool resetGapFeature(QString* errorOut = nullptr);
    bool clearCropFeature();
    bool ReleasePublishedVolumes(bool all, QString* errorOut = nullptr);
    bool StartCrop(CropHostAction shape);
    bool SetCropWidget(CropHostAction shape);
    CropHostTarget getCropTarget() const;
    static HostViewTargets getGapTargets();
    bool sendCropAction(CropHostRequest request);
    void syncCropHistory();
    void clearCropHistory();
    bool sendLoadRequest(
        HostRequest&& request,
        const QString& sourcePath,
        QString* errorOut);
    void SendPendingLoad();
    void finishLoadRequest(std::uint64_t generation, HostResult result);
    void finishGapExport();
    std::shared_ptr<const GapResultReader> getGapResultReader() const;
    void setIsoState();
    void setState(State state);

private:
    HostSessionConfig config_;
    bool hasConfig_ = false;
    std::unique_ptr<VtkAppHostSession> hostSession_;
    std::shared_ptr<CropHostFeature> cropFeature_;
    std::shared_ptr<GapHostFeature> gapFeature_;
    State state_ = State::Empty;
    std::uint64_t requestGeneration_ = 0;
    QString sourcePath_;
    QString pendingSourcePath_;
    bool cropBuildPending_ = false;
    bool isGapRestoreCleanupPending_ = false;
    QTimer cropStateTimer_;
    struct WorkSignal;
    std::shared_ptr<WorkSignal> workSignal_;
    std::function<bool(const std::string&)> getVisible_;
    std::set<std::string> pendingRender_;
    QTimer renderTimer_;
    QElapsedTimer renderClock_;
    qint64 nextRenderAt_ = 0;
    bool isClosing_ = false;
    std::uint64_t cropRevision_ = 0;
    CropNodeId cropRenderedHead_ = 0;
    std::unique_ptr<HostRequest> pendingLoad_;
    QTimer loadRetryTimer_;
    QElapsedTimer loadRetryClock_;
    QTimer gapExportTimer_;
    std::future<std::pair<bool, QString>> gapExport_;
    std::shared_ptr<std::atomic_bool> gapExportCancelled_;
    std::function<void(bool, QString)> gapExportCallback_;
    std::shared_ptr<VolumePublishBridge> volumePublishBridge_;

};

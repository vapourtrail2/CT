#include "c_ui/context/SessionManager.h"
#include <QByteArray>
#include <QMetaObject>
#include <QPointer>

#include <array>
#include <cmath>
#include <limits>
#include <utility>

void setError(QString* err,QString message) {
    if(err)
        *err = message;
}

bool isVoxelCountRight(std::array<int, 3> dimensions, std::size_t& voxelcount) {
    voxelcount = 1;

    for (auto dimension : dimensions)
    {
        if (dimension <= 0) {
            return false;
        }

        auto v = static_cast<size_t>(dimension);

        if (voxelcount > std::numeric_limits<size_t>::max() / v) {
            return false;
        }

        voxelcount = voxelcount * v;
    }

    return true;
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
    cropStateTimer_.setInterval(100);
    connect(
        &cropStateTimer_,
        &QTimer::timeout,
        this,
        &SessionManager::syncCropHistory);
}

SessionManager::~SessionManager()
{
    ++requestGeneration_;
    (void)StopHost();
}

bool SessionManager::initHost(HostSessionConfig config, QString* err)
{
    if (config.renderViews.empty()) {
        setError(
            err,
            QStringLiteral("Host render view configuration is empty."));
        return false;
    }

    if (!config.sendOwnerTask) {
        const QPointer<SessionManager> guard(this);
        config.sendOwnerTask =
            [guard](std::function<void()> task) {
                if (!guard || !task) {
                    return false;
                }
                return QMetaObject::invokeMethod(
                    guard.data(),
                    [guard, task = std::move(task)]() mutable {
                        if (guard) {
                            task();
                        }
                    },
                    Qt::QueuedConnection);
            };
    }

    config_ = std::move(config);
    hasConfig_ = true;

    if (!resetHost(err)) {
        setState(State::Failed);
        return false;
    }

    sourcePath_.clear();
    pendingSourcePath_.clear();
    setState(State::Empty);
   
    return true;
}

bool SessionManager::resetHost(QString* errorOut)
{
    ++requestGeneration_;
    if (!StopHost(errorOut)) {
        return false;
    }

    if (!hasConfig_ || config_.renderViews.empty()) {
        setError(
            errorOut,
            QStringLiteral("Host has not been configured."));
        return false;
    }

    auto newHost = std::make_unique<VtkAppHostSession>(config_);

    if (!newHost->BuildSession()) {
        (void)newHost->Stop();
        setError(
            errorOut,
            QStringLiteral("Failed to build VTK host session."));
        return false;
    }

    hostSession_ = std::move(newHost);
    if (!resetCropFeature(errorOut)) {
        (void)hostSession_->Stop();
        hostSession_.reset();
        return false;
    }

    return true;
}

bool SessionManager::StopHost(QString* errorOut)
{
    clearCropFeature();
    if (!hostSession_) {
        return true;
    }
    if (!hostSession_->Stop()) {
        setError(
            errorOut,
            QStringLiteral("Failed to stop VTK host session."));
        return false;
    }
    hostSession_.reset();
    return true;
}

CropHostTarget SessionManager::getCropTarget() const
{
    CropHostTarget target;
    target.referenceView = {
        "", true, HostRenderViewRole::Primary3D };
    target.targetViews.viewRoles = {
        HostRenderViewRole::Primary3D,
    /*    HostRenderViewRole::TopDownSlice,
        HostRenderViewRole::FrontBackSlice,
        HostRenderViewRole::LeftRightSlice*/
    };
    target.isTargetViewsUsed = true;
    target.source = CropHostSource::CurrentImage;
    return target;
}

void SessionManager::clearCropHistory()
{
    cropRecords_.clear();
    cropBuildPending_ = false;
    hasOriginalCrop_ = false;
    emit cropHistoryChanged();
}

void SessionManager::clearCropFeature()
{
    cropStateTimer_.stop();

    if (hostSession_) {
        (void)hostSession_->AttachTimer({});
        if (cropFeature_) {
            (void)hostSession_->DetachFeature(*cropFeature_);
        }
    }

    cropFeature_.reset();
    clearCropHistory();
}

bool SessionManager::resetCropFeature(QString* errorOut)
{
    clearCropFeature();

    if (!hostSession_) {
        setError(
            errorOut,
            QStringLiteral("Host session has not been initialized."));
        return false;
    }

    CropHostConfig config;
    config.defaultTarget = getCropTarget();

    auto feature = std::make_shared<CropHostFeature>(std::move(config));
    if (!hostSession_->AttachFeature(feature)) {
        setError(
            errorOut,
            QStringLiteral("Failed to attach Core crop feature."));
        return false;
    }

    HostTimerConfig timer;
    timer.isTimerEnabled = true;
    timer.targetView = {
        "", true, HostRenderViewRole::Primary3D };
    if (!hostSession_->AttachTimer(timer)) {
        (void)hostSession_->DetachFeature(*feature);
        setError(
            errorOut,
            QStringLiteral("Failed to attach Core crop timer."));
        return false;
    }

    cropFeature_ = std::move(feature);
    cropStateTimer_.start();
    return true;
}



bool SessionManager::openFile(const QString& path,
	const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    QString* errorOut
    )
{
    const QString p = path.trimmed();
    if (p.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Empty path.");
        }
        return false;
    }

    HostLoadRequest request;
    const QByteArray utf8Path = p.toUtf8();
    request.filePath = utf8Path.toStdString();
    request.geometry.dimensions = dims ;
    request.geometry.spacing = spacing;
    request.geometry.origin = origin;

    return sendLoadRequest(std::move(request), p, errorOut);
}

bool SessionManager::openReconstructedData(
    std::vector<float>&& voxels,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    QString* errorOut)
{
    std::size_t voxelCount = 0;

    if (!isVoxelCountRight(
        dims,
        voxelCount)) {
        setError(
            errorOut,
            QStringLiteral(
                "Invalid reconstruction dimensions."));
        return false;
    }

    if (voxels.size() != voxelCount) {
        setError(
            errorOut,
            QStringLiteral(
                "Reconstruction buffer size "
                "does not match dimensions."));
        return false;
    }

    HostReloadRequest request;

    request.voxels = std::move(voxels);

    request.geometry.dimensions = dims;
    request.geometry.spacing = spacing;
    request.geometry.origin = origin;

    const QString displayPath =
        sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();

    return sendLoadRequest(
        std::move(request),
        displayPath,
        errorOut);
}

bool SessionManager::sendRequest(HostRequest&& request, HostCompleteCallback onComplete)
{
    if (!hostSession_) {
        return false;
    }

    return hostSession_->SendRequestResult(
        std::move(request),
        [onComplete = std::move(onComplete)](HostResult result) {
            if (onComplete) {
                onComplete(result.isSucceeded);
            }
        });
}

bool SessionManager::sendCropAction(CropHostRequest request)
{
    return state_ == State::Ready
        && cropFeature_
        && !cropBuildPending_
        && cropFeature_->SendRequest(std::move(request));
}

bool SessionManager::startBoxCrop()
{
    pendingCropShape_ = CropRecordShape::Box;

    CropHostRequest request;
    request.action = CropHostAction::Box;
    request.target = getCropTarget();
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    const bool isModeSet = keepCropInside();
    emit cropHistoryChanged();
    return isModeSet;
}

bool SessionManager::startPlaneCrop()
{
    pendingCropShape_ = CropRecordShape::Plane;

    CropHostRequest request;
    request.action = CropHostAction::Plane;
    request.target = getCropTarget();
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    const bool isModeSet = keepCropInside();
    emit cropHistoryChanged();
    return isModeSet;
}

bool SessionManager::keepCropInside()
{
    CropHostRequest request;
    request.action = CropHostAction::Mode;
    request.target = getCropTarget();
    request.removalMode = CropRemovalMode::KeepInside;
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    pendingCropMode_ = CropRemovalMode::KeepInside;
    syncCropHistory();
    emit cropHistoryChanged();
    return true;
}

bool SessionManager::removeCropInside()
{
    CropHostRequest request;
    request.action = CropHostAction::Mode;
    request.target = getCropTarget();
    request.removalMode = CropRemovalMode::RemoveInside;
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    pendingCropMode_ = CropRemovalMode::RemoveInside;
    syncCropHistory();
    emit cropHistoryChanged();
    return true;
}

bool SessionManager::setCropNode(const std::size_t nodeCount)
{
    CropHostRequest request;
    request.action = CropHostAction::Node;
    request.nodeCount = nodeCount;
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    syncCropHistory();
    emit cropHistoryChanged();
    return true;
}

bool SessionManager::applyCrop()
{
    if (state_ != State::Ready
        || !cropFeature_
        || cropBuildPending_) {
        return false;
    }

    CropHostRequest request;
    request.action = CropHostAction::BuildResult;
    request.target = getCropTarget();

    QPointer<SessionManager> guard(this);
    const bool started = cropFeature_->SendRequest(
        std::move(request),
        [guard](CropBuildResult result) {
            if (!guard) {
                return;
            }

            const bool isSuccess = result.isSucceeded;
            const QString message = QString::fromStdString(result.message);
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, isSuccess, message]() {
                    if (!guard) {
                        return;
                    }

                    guard->cropBuildPending_ = false;
                    if (isSuccess) {
                        guard->hasOriginalCrop_ = true;
                        guard->setIsoState();
                    }
                    guard->syncCropHistory();
                    emit guard->cropHistoryChanged();
                    emit guard->cropBuildFinished(
                        isSuccess,
                        message);
                },
                Qt::QueuedConnection);
        });

    if (!started) {
        return false;
    }

    cropBuildPending_ = true;
    emit cropHistoryChanged();
    return true;
}

bool SessionManager::restoreOriginalCrop()
{
    CropHostRequest request;
    request.action = CropHostAction::RestoreOriginal;
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    cropRecords_.clear();
    cropBuildPending_ = false;
    hasOriginalCrop_ = true;
    setIsoState();
    emit cropHistoryChanged();
    return true;
}

bool SessionManager::exitCrop()
{
    CropHostRequest request;
    request.action = CropHostAction::Exit;
    if (!sendCropAction(std::move(request))) {
        return false;
    }

    syncCropHistory();
    emit cropHistoryChanged();
    return true;
}

CropTreeState SessionManager::getCropTreeState() const
{
    CropTreeState treeState;
    if (!cropFeature_) {
        return treeState;
    }

    const CropHostState cropState = cropFeature_->GetState();
    const auto& history = cropState.history;
    treeState.isCropping = cropState.isActive;
    treeState.isBuilding = cropBuildPending_ || cropState.isPublishing;
    treeState.canApply = !treeState.isBuilding && history.nodeCount > 0;
    treeState.canRestoreOriginal =
        !treeState.isBuilding && hasOriginalCrop_;

    for (std::size_t index = 0;
        index < cropRecords_.size(); ++index) {
        const auto& record = cropRecords_[index];
        const std::size_t absoluteNode = index + 1;
        const bool isApplied = absoluteNode <= history.baseNodeCount;
        const bool isSelectable =
            !isApplied
            && absoluteNode <= history.baseNodeCount
                + history.operationCount;
        const std::size_t nodeCount =
            isSelectable
            ? absoluteNode - history.baseNodeCount
            : 0;
        const QString shape =
            record.shape == CropRecordShape::Box
            ? QStringLiteral("框")
            : QStringLiteral("平面");
        const QString mode =
            record.removalMode == CropRemovalMode::RemoveInside
            ? QStringLiteral("移除内部")
            : QStringLiteral("保留内部");

        treeState.items.push_back(CropTreeItem{
            QStringLiteral("裁切 %1｜%2｜%3")
                .arg(absoluteNode)
                .arg(shape)
                .arg(mode),
            nodeCount,
            isSelectable && nodeCount == history.nodeCount,
            isApplied,
            isSelectable
        });
    }

    return treeState;
}

void SessionManager::syncCropHistory()
{
    if (!cropFeature_) {
        return;
    }

    const CropHostState cropState = cropFeature_->GetState();
    const auto& history = cropState.history;
    bool isChanged = false;

    if (cropRecords_.size() > history.allOperationCount) {
        cropRecords_.resize(history.allOperationCount);
        isChanged = true;
    }
    while (cropRecords_.size() < history.allOperationCount) {
        cropRecords_.push_back(CropRecord{
            pendingCropShape_,
            pendingCropMode_ });
        isChanged = true;
    }

    if (history.hasEditableOp && history.nodeCount > 0) {
        const std::size_t absoluteNode =
            history.baseNodeCount + history.nodeCount;
        if (absoluteNode <= cropRecords_.size()) {
            auto& record = cropRecords_[absoluteNode - 1];
            const CropRemovalMode mode =
                history.editMode == CropRemovalMode::None
                ? pendingCropMode_
                : history.editMode;
            if (record.shape != pendingCropShape_
                || record.removalMode != mode) {
                record.shape = pendingCropShape_;
                record.removalMode = mode;
                isChanged = true;
            }
        }
    }

    if (isChanged) {
        emit cropHistoryChanged();
    }
}

std::optional<measure::MeasureHostData>
SessionManager::GetMeasureHostData(const HostViewTarget& sourceView)
{
    if (!hostSession_ || state_ != State::Ready) {
        return std::nullopt;
    }

    auto bridge =
        std::make_shared<measure::MeasureHostBridge>(sourceView);
    if (!hostSession_->AttachFeature(bridge)) {
        return std::nullopt;
    }

    const auto hostData = bridge->GetHostData();
    if (!hostSession_->DetachFeature(*bridge)) {
        return std::nullopt;
    }
    return hostData;
}

std::optional<HostRenderViewState>SessionManager::getRenderViewState(
    const HostViewTarget& target)
{
    return hostSession_
        ? hostSession_->GetRenderViewState(target)
        : std::nullopt;
}

bool SessionManager::sendLoadRequest(
    HostRequest&& request,
    const QString& sourcePath,
    QString* errorOut)
{
    if (!hostSession_) {
        setError(
            errorOut,
            QStringLiteral("Host session has not been initialized."));
        return false;
    }

    if (state_ == State::Loading) {
        setError(
            errorOut,
            QStringLiteral("A load request is already running."));
        return false;
    }

    if (!resetCropFeature(errorOut)) {
        return false;
    }

    const std::uint64_t generation = ++requestGeneration_;

    sourcePath_.clear();
    pendingSourcePath_ = sourcePath;
    setState(State::Loading);
	emit isoStateCleared();//清除 isoStateChanged 信号 加载第二个文件第一个文件的iosvalue就不会显示

    QPointer<SessionManager> ptr(this);
    const bool started = hostSession_->SendRequestResult(
        std::move(request),
        [ptr, generation](HostResult result) {
            if (!ptr) {
                return;
            }

            const bool isSuccess = result.isSucceeded;

            QMetaObject::invokeMethod(//回到qt线程
                ptr.data(),
                [ptr, generation, isSuccess]() {
                    if (ptr) {
                        ptr->finishLoadRequest(generation,isSuccess);
                    }
                },
                Qt::QueuedConnection);
        });

    if (!started)  
    {
        ++requestGeneration_;
        pendingSourcePath_.clear();
        setState(State::Failed);
        return false;
    }

    return true;
}

void SessionManager::finishLoadRequest(//更新状态
    std::uint64_t generation,
    bool isSuccess)
{
    if (generation != requestGeneration_) {
        return;
    }

    if (isSuccess) {
        sourcePath_ = pendingSourcePath_;
    }

    pendingSourcePath_.clear();

    setState(
        isSuccess
        ? State::Ready
        : State::Failed);

    if (isSuccess) {
        setIsoState();
    }
    else {
        emit isoStateCleared();
    }

    emit loadFinished(
        isSuccess,
        isSuccess
        ? QString()
        : QStringLiteral(
            "Core load request failed."));
}

void SessionManager::setIsoState()
{
    if (!hostSession_) {
        emit isoStateCleared();
        return;
    }

    HostViewTarget target;
    target.isViewRoleUsed = true;
    target.viewRole = HostRenderViewRole::Primary3D;

    const auto state = hostSession_->GetRenderViewState(target);
    if (!state) {
        return;
    }

    const double isoValue  = state->isoThreshold;
    const double scalarMin = state->scalarRange[0];
    const double scalarMax = state->scalarRange[1];

    if (!std::isfinite(isoValue)
        || !std::isfinite(scalarMin)
        || !std::isfinite(scalarMax)
        || scalarMax <= scalarMin)
    {
        emit isoStateCleared();
        return;
    }

    emit isoStateChanged(
        isoValue,
        scalarMin,
        scalarMax);
}

void SessionManager::setState(State state)
{
    if (state_ == state) {
        return;
    }

    state_ = state;
    emit sessionChanged(state_);
}

void SessionManager::clearSession()
{
    sourcePath_.clear();
    pendingSourcePath_.clear();

    QString error;
    if (hasConfig_ && !resetHost(&error)) {
        setState(State::Failed);
        return;
    }

    setState(State::Empty);
    emit isoStateCleared();
}

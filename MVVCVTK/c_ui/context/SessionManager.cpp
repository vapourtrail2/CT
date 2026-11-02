#include "c_ui/context/SessionManager.h"
#include "c_ui/context/VolumeHostBridge.h"
#include "measure/MainMeasurementFeature.h"
#include <QByteArray>
#include <QMetaObject>
#include <QPointer>
#include <Qthread>

#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <memory>
#include <QDebug>
#include <QCoreApplication>
#include <QSaveFile>
#include <QUuid>
#include <atomic>
#include <chrono>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <QStringList>

struct SessionManager::WorkSignal {
    QObject* receiver = nullptr;
    QPointer<SessionManager> owner;
    std::atomic<bool> isQueued{false};
};

namespace {
std::pair<bool, QString> WriteGapCsv(const QString& path,
    const std::shared_ptr<const GapResultReader>& reader,
    const std::shared_ptr<std::atomic_bool>& cancelled)
{
    const auto getCancelled = [cancelled] { return cancelled->load(); };
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString()};
    const auto writeLine = [&file](const QStringList& cells) {
        const auto bytes = (cells.join(',') + "\r\n").toUtf8();
        return file.write(bytes) == bytes.size();
    };
    if (!writeLine({"id", "voxel_count", "volume_mm3", "parent_volume_mm3",
        "intersection_volume_mm3", "equivalent_diameter_mm", "is_roi_partial",
        "has_whole_object_geometry", "bounds_model_min_x_mm", "bounds_model_max_x_mm",
        "bounds_model_min_y_mm", "bounds_model_max_y_mm", "bounds_model_min_z_mm", "bounds_model_max_z_mm", "boundary_status"}))
        return {false, file.errorString()};
    const auto metadata = reader->GetMetadata();
    std::uint64_t offset = 0;
    while (offset < metadata.objectCount) {
        if (getCancelled()) return {false, QStringLiteral("CSV 导出已取消。")};
        const auto page = reader->GetObjects(offset, 4096, getCancelled);
        if (page.error != GapReadError::None || page.result != metadata.result
            || page.total != metadata.objectCount || page.next <= offset
            || page.next > page.total || page.next - offset != page.objects.size())
            return {false, QStringLiteral("孔隙对象分页读取失败，错误码 %1。").arg(int(page.error))};
        for (const auto& object : page.objects) {
            if (getCancelled()) return {false, QStringLiteral("CSV 导出已取消。")};
            QStringList cells{QString::number(object.id), QString::number(qulonglong(object.voxelCount))};
            for (const auto value : {object.volumeMM3, object.parentVolumeMM3,
                object.intersectionVolumeMM3, object.equivalentDiameterMM})
                cells << QString::number(value, 'g', 17);
            cells << QString::number(object.isRoiPartial ? 1 : 0)
                  << QString::number(object.hasWholeObjectGeometry ? 1 : 0);
            for (const auto value : object.boundsModel) cells << QString::number(value, 'g', 17);
            switch (object.boundaryStatus) {
            case GapBoundaryStatus::Interior: cells << QStringLiteral("Interior"); break;
            case GapBoundaryStatus::BoundaryUncertain: cells << QStringLiteral("BoundaryUncertain"); break;
            default: return {false, QStringLiteral("孔隙对象边界状态无法识别。")};
            }
            if (!writeLine(cells)) return {false, file.errorString()};
        }
        offset = page.next;
    }
    if (getCancelled()) return {false, QStringLiteral("CSV 导出已取消。")};
    if (!file.commit()) return {false, file.errorString()};
    return {true, QStringLiteral("CSV 导出完成，共 %1 条对象记录。").arg(qulonglong(offset))};
}
}

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
    : QObject(parent), workSignal_(std::make_shared<WorkSignal>())
{
    workSignal_->receiver = QCoreApplication::instance();
    workSignal_->owner = this;
    renderClock_.start();
    renderTimer_.setSingleShot(true);
    renderTimer_.setTimerType(Qt::PreciseTimer);
    connect(&renderTimer_, &QTimer::timeout, this, &SessionManager::SendRender);
    loadRetryTimer_.setInterval(50);
    connect(&loadRetryTimer_, &QTimer::timeout, this, &SessionManager::SendPendingLoad);
    gapExportTimer_.setInterval(50);
    connect(&gapExportTimer_, &QTimer::timeout, this, &SessionManager::finishGapExport);
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
    if (gapExportCancelled_) gapExportCancelled_->store(true);
    if (gapExport_.valid()) gapExport_.wait();
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
        QObject* receiver = QCoreApplication::instance();
        config.sendOwnerTask =
            [receiver](std::function<void()> task) {
                if (!receiver || !task) {
                    return false;
                }
                return QMetaObject::invokeMethod(
                    receiver,
                    std::move(task),
                    Qt::QueuedConnection);
            };
    }

    config.driveMode = HostDriveMode::HostDriven;
    const std::weak_ptr<WorkSignal> weak = workSignal_;
    config.onWorkAvailable = [weak] {
        const auto signal = weak.lock();
        if (!signal || signal->isQueued.exchange(true)) return;
        if (!signal->receiver || !QMetaObject::invokeMethod(signal->receiver, [weak] {
            const auto ready = weak.lock();
            if (!ready) return;
            ready->isQueued.store(false);
            if (ready->owner) ready->owner->SendUpdates();
        }, Qt::QueuedConnection)) signal->isQueued.store(false);
    };
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
    loadRetryTimer_.stop();
    pendingLoad_.reset();
    if (!StopHost(errorOut)) {
        return false;
    }

    if (!hasConfig_ || config_.renderViews.empty()) {
        setError(
            errorOut,
            QStringLiteral("Host has not been configured."));
        return false;
    }

    isClosing_ = false;
    auto newHost = std::make_unique<VtkAppHostSession>(config_);

    //if (!newHost->BuildSession()) {
    //    (void)newHost->Stop();
    //    setError(
    //        errorOut,
    //        QStringLiteral("Failed to build VTK host session."));
    //    return false;
    //}

    newHost->BuildSession();

    hostSession_ = std::move(newHost);
    if (!resetCropFeature(errorOut)) {
        (void)StopHost();
        return false;
    }
    return true;
}

bool SessionManager::StopHost(QString* errorOut)
{
    if (gapExport_.valid()) {
        gapExportCancelled_->store(true);
        if (gapExport_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            setError(errorOut, QStringLiteral("正在取消 CSV 导出，请稍候。"));
            return false;
        }
        finishGapExport();
    }
    if (!hostSession_) {
        gapFeature_.reset();
        isGapRestoreCleanupPending_ = false;
        clearCropFeature();
        return true;
    }
    if (hostSession_->GetStopState() == HostStopState::Running) {
        const auto pumpClosingViews = [this] {
            (void)hostSession_->SendUpdates();
            HostRenderRequest draw;
            for (const auto& view : hostSession_->GetSceneViewStates())
                if (view.isAvailable) draw.viewIds.push_back(view.id);
            if (!draw.viewIds.empty()) (void)hostSession_->SendRender(draw);
            };
        if (gapFeature_) {
            if (!hostSession_->DetachFeature(*gapFeature_)) {
                pumpClosingViews();
                setError(errorOut, QStringLiteral("正在退出孔隙业务，请稍候。"));
                return false;
            }
            gapFeature_.reset();
            isGapRestoreCleanupPending_ = false;
        }
        if (cropFeature_) {
            if (!hostSession_->DetachFeature(*cropFeature_)) {
                pumpClosingViews();
                setError(errorOut, QStringLiteral("正在关闭裁切工具，请稍候。"));

                return false;
            }
            cropFeature_.reset();
        }
    }
    if (!hostSession_->Stop()) {
        setError(
            errorOut,
            QStringLiteral("Failed to stop VTK host session."));
        return false;
    }

    cropStateTimer_.stop();
    renderTimer_.stop();
    pendingRender_.clear();
    gapFeature_.reset();
    isGapRestoreCleanupPending_ = false;
    cropFeature_.reset();
    volumePublishBridge_.reset();
    hostSession_.reset();
    clearCropHistory();
    return true;
}

CropHostTarget SessionManager::getCropTarget() const
{
    CropHostTarget target;
    target.inputBinding = std::string(primaryVolumeBinding);
    target.referenceView = {
        "", true, HostRenderViewRole::Primary3D };
    target.targetViews = getGapTargets();
    return target;
}

void SessionManager::clearCropHistory()
{
    cropBuildPending_ = false;
    cropRevision_ = 0;
    cropRenderedHead_ = 0;
    emit cropHistoryChanged();
}

bool SessionManager::clearCropFeature()
{
    if (hostSession_ && cropFeature_ && !hostSession_->DetachFeature(*cropFeature_)) return false;
    cropStateTimer_.stop();
    cropFeature_.reset();
    clearCropHistory();
    return true;
}

bool SessionManager::resetCropFeature(QString* errorOut)
{
    if (!clearCropFeature()) {
        setError(errorOut, QStringLiteral("裁剪文档仍在退出，请稍后重试加载。"));
        return false;
    }

    if (!hostSession_ || isClosing_) {
        setError(
            errorOut,
            QStringLiteral("Host session has not been initialized."));
        return false;
    }

    auto feature = std::make_shared<CropHostFeature>();
    if (!hostSession_->AttachFeature(feature)) {//为什么裁切应用之后 在裁切上做了测量之后，切到原始上去 会触发这个Failed to attach Core crop feature，但是四视图已经更新成1的视觉效果 但是会报这个错
        setError(
            errorOut,
            QStringLiteral("Failed to attach Core crop feature."));
        return false;
    }

    cropFeature_ = std::move(feature);
    cropStateTimer_.start();
    return true;
}

HostViewTargets SessionManager::getGapTargets()
{
    HostViewTargets targets;
    targets.viewRoles = {
        HostRenderViewRole::Primary3D,
        HostRenderViewRole::TopDownSlice,
        HostRenderViewRole::FrontBackSlice,
        HostRenderViewRole::LeftRightSlice
    };
    return targets;
}

bool SessionManager::resetGapFeature(QString* errorOut)
{
    gapFeature_.reset();

    if (!hostSession_ || isClosing_) {
        setError(
            errorOut,
            QStringLiteral("Host session has not been initialized."));
        return false;
    }

    GapHostStartParams defaultStart;
    defaultStart.targetViews = getGapTargets();
    defaultStart.surface.isoMode = GapIsoMode::DataRangeRatio;
    defaultStart.surface.dataRangeRatio = 0.5;

    GapHostConfig config;
    config.defaultStart = defaultStart;
    config.inputViews = getGapTargets();
    config.keys.switchOverlay.keyCode = 'j';
    config.keys.exit.keySym = "Escape";

    auto feature = std::make_shared<GapHostFeature>(std::move(config));
    if (!hostSession_->AttachFeature(feature)) {
        setError(
            errorOut,
            QStringLiteral("Failed to attach Core gap analysis feature."));
        return false;
    }

    gapFeature_ = std::move(feature);
    return true;
}

bool SessionManager::openData(const QString& path,
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
    request.geometry.spacing = { spacing[0], spacing[1], spacing[2] };
    request.geometry.origin = { origin[0], origin[1], origin[2] };
    request.metadata.identity.datasetId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    request.metadata.source.kind = ImageSourceKind::RawFile;
    request.metadata.source.uri = request.filePath;

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
    request.geometry.spacing = { spacing[0], spacing[1], spacing[2] };
    request.geometry.origin = { origin[0], origin[1], origin[2] };
    request.metadata.identity.datasetId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    request.metadata.source.kind = ImageSourceKind::Memory;
    request.metadata.source.uri = "ct-reconstruction:" + request.metadata.identity.datasetId;

    const QString displayPath =
        sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();

    return sendLoadRequest(
        std::move(request),
        displayPath,
        errorOut);
}

bool SessionManager::sendRequest(HostRequest&& request, HostResultCallback onComplete)
{
    if (!hostSession_ || isClosing_) {
        return false;
    }

    const auto admission = hostSession_->SendRequest(
        std::move(request),
        [guard = QPointer<SessionManager>(this), onComplete = std::move(onComplete)](HostResult result) {
            if (!guard) return;
            if (!result.isSucceeded) emit guard->hostError(QString::fromStdString(result.message));
            if (guard && onComplete) {
                onComplete(std::move(result));
            }
        });
    if (admission.status != HostAdmissionStatus::Accepted) {
        emit hostError(QString::fromStdString(admission.message));
        return false;
    }
    return true;
}

bool SessionManager::sendCropAction(CropHostRequest request)
{
    return !isClosing_ && state_ == State::Ready
        && cropFeature_
        && !cropBuildPending_
        && cropFeature_->SendRequest(std::move(request));
}

bool SessionManager::startBoxCrop() { return StartCrop(CropHostAction::Box); }

bool SessionManager::startPlaneCrop() { return StartCrop(CropHostAction::Plane); }

bool SessionManager::keepCropInside()
{
    CropHostRequest request;
    request.action = CropHostAction::Mode;
    request.target = getCropTarget();
    request.removalMode = CropRemovalMode::KeepInside;
    const bool accepted = sendCropAction(request);
    if (accepted) syncCropHistory();
    return accepted;
}

bool SessionManager::removeCropInside()
{
    CropHostRequest request;
    request.action = CropHostAction::Mode;
    request.target = getCropTarget();
    request.removalMode = CropRemovalMode::RemoveInside;
    const bool accepted = sendCropAction(request);
    if (accepted) syncCropHistory();
    return accepted;
}

bool SessionManager::setCropNode(CropNodeId nodeId)
{
    if (state_ != State::Ready || isClosing_ || !cropFeature_ || cropBuildPending_) return false;
    const auto history = cropFeature_->GetHistory();
    CropEditRequest request;
    request.documentId = history.documentId;
    request.nodeId = nodeId;
    request.kind = CropEditKind::Select;
    request.requestId = CropHostFeature::CreateRequestId();
    request.expectedRevision = history.stateRevision;
    cropBuildPending_ = true;
    QPointer<SessionManager> guard(this);
    const auto generation = requestGeneration_;
    const auto admission = cropFeature_->SendRequest(request, [guard, generation](CropEditOutcome result) {
        if (!guard || result.status == CropEditStatus::Queued) return;
        QTimer::singleShot(0, guard, [guard, generation, result] {
            if (!guard || guard->requestGeneration_ != generation) return;
            guard->cropBuildPending_ = false;
            if (result.status != CropEditStatus::Succeeded) emit guard->hostError(QStringLiteral("裁剪节点选择失败，错误码 %1。").arg(int(result.failureReason)));
            emit guard->cropHistoryChanged();
        });
    });
    if (!admission.isAccepted) cropBuildPending_ = false;
    emit cropHistoryChanged();
    return admission.isAccepted;
}

bool SessionManager::applyCrop()
{
    if (state_ != State::Ready || isClosing_ || !cropFeature_ || cropBuildPending_) return false;
    const auto history = cropFeature_->GetHistory();
    if (!history.documentId || history.appliedHead == history.rootNodeId || history.renderedHead != history.appliedHead) return false;
    CropBuildRequest request;
    request.documentId = history.documentId;
    request.nodeId = history.appliedHead;
    request.requestId = CropHostFeature::CreateRequestId();
    request.expectedRevision = history.stateRevision;
    const auto generation = requestGeneration_;
    QPointer<SessionManager> guard(this);
    cropBuildPending_ = true;
    const auto admission = cropFeature_->SendRequest(request, [guard, generation](CropBuildResult result) {
        if (!guard) return;
        QTimer::singleShot(0, guard, [guard, generation, result = std::move(result)] {
            if (!guard || guard->requestGeneration_ != generation || guard->isClosing_) return;
            if (!result.isSucceeded) {
                guard->cropBuildPending_ = false;
                emit guard->cropHistoryChanged();
                emit guard->cropBuildFinished(false, QString::fromStdString(result.message));
                return;
            }
            const auto descriptor = guard->hostSession_->GetImageDescriptor();
            if (!descriptor || descriptor->dataRevision != result.sourceRevision) {
                guard->cropBuildPending_ = false;
                emit guard->cropHistoryChanged();
                emit guard->cropBuildFinished(false, QStringLiteral("裁剪已构建，但当前来源已变化，未替换主输入。"));
                return;
            }
            HostDataSelectRequest select;
            select.dataRevision = result.outputRevision;
            select.expectedBindingRevision = descriptor->bindingRevision;
            const auto onSelected = [guard, generation](HostResult selected) {
                if (!guard) return;
                QTimer::singleShot(0, guard, [guard, generation, selected = std::move(selected)] {
                    if (!guard || guard->requestGeneration_ != generation) return;
                    guard->cropBuildPending_ = false;
                    guard->setIsoState();
                    emit guard->cropHistoryChanged();
                    emit guard->cropBuildFinished(selected.isSucceeded, QString::fromStdString(selected.message));
                });
            };
            const auto selected = guard->hostSession_->SendRequest(std::move(select), onSelected);
            // SDK 拒绝时不回调，CT 仍需结束裁剪进度并报告失败。
            if (selected.status != HostAdmissionStatus::Accepted) {
                onSelected({ false, selected.errorCode, selected.message });
            }
        });
    });
    if (!admission.isAccepted) cropBuildPending_ = false;
    emit cropHistoryChanged();
    return admission.isAccepted;
}

bool SessionManager::restoreOriginalCrop()
{
    const auto reject = [this](const QString& message) {
        emit hostError(message);
        return false;
    };
    if (QThread::currentThread() != thread() || isClosing_ || !hostSession_
        || state_ != State::Ready || hostSession_->GetStopState() != HostStopState::Running)
        return reject(QStringLiteral("当前会话尚未就绪，不能恢复原始。"));
    if (!cropFeature_ || cropBuildPending_)
        return reject(QStringLiteral("裁切尚未就绪，请稍后重试恢复原始。"));

    const auto crop = cropFeature_;
    const auto host = hostSession_.get();
    const auto isCropReady = [crop, host] {
        const auto state = crop->GetState();
        const auto history = crop->GetHistory();
        const auto input = host->GetImageDescriptor();
        return !state.isPublishing && !state.history.pendingRequestCount && !state.history.isDragging
            && history.documentId && input && input->dataRevision != history.sourceRevision;
    };
    if (!isCropReady())
        return reject(QStringLiteral("裁切文档忙碌或已显示原始输入，未启动恢复。"));
    if (getIsGapExporting())
        return reject(QStringLiteral("CSV 正在导出，请等待完成后再恢复原始。"));
    if (gapFeature_) {
        // SDK 快捷键也能发起分析；清理重试必须重新核对，不能用 CT 标记跳过。
        const auto state = gapFeature_->GetState();
        if (state.analysisState == GapAnalysisState::Running)
            return reject(QStringLiteral("孔隙分析正在运行，请等待完成后再恢复原始。"));
        if (state.isViewActive || state.isExitPending)
            return reject(QStringLiteral("请先退出孔隙显示并等待退出完成，再恢复原始。"));
    }

    QPointer<SessionManager> guard(this);
    const auto generation = requestGeneration_;
    const auto isCurrent = [guard, generation, host, crop] {
        return guard && !guard->isClosing_ && guard->requestGeneration_ == generation
            && guard->hostSession_.get() == host && guard->cropFeature_ == crop;
    };
    if (gapFeature_) {
        const auto gap = gapFeature_;
        isGapRestoreCleanupPending_ = true;
        cropBuildPending_ = true;
        const bool detached = host->DetachFeature(*gap);
        if (!isCurrent()) return false;
        if (gapFeature_ != gap) { cropBuildPending_ = false; return false; }
        if (!detached) {
            SendUpdates();
            if (!isCurrent()) return false;
            SendRender();
            if (!isCurrent()) return false;
            cropBuildPending_ = false;
            emit cropHistoryChanged();
            if (!isCurrent() || cropBuildPending_) return false;
            return reject(QStringLiteral("孔隙结果仍在释放，请稍后重试恢复原始。"));
        }
        gapFeature_.reset();
        isGapRestoreCleanupPending_ = false;
        cropBuildPending_ = false;
    }
    if (!isCropReady())
        return reject(QStringLiteral("裁切来源状态已变化，未启动恢复，请重试。"));
    const auto history = crop->GetHistory();
    CropDocumentRequest request;
    request.action = CropDocumentAction::ReturnToSource;
    request.documentId = history.documentId;
    request.requestId = CropHostFeature::CreateRequestId();
    request.expectedRevision = history.stateRevision;
    cropBuildPending_ = true;
    const auto admission = crop->SendRequest(request, [guard, generation](CropDocumentOutcome result) {
        if (!guard || result.status == CropEditStatus::Queued) return;
        QTimer::singleShot(0, guard, [guard, generation, result] {
            if (!guard || guard->isClosing_ || guard->requestGeneration_ != generation) return;
            guard->cropBuildPending_ = false;
            guard->setIsoState();
            if (!guard || guard->isClosing_ || guard->requestGeneration_ != generation) return;
            emit guard->cropHistoryChanged();
            // 远程 CT 用 cropBuildFinished 保存归档 B；恢复来源不能覆盖该归档。
            if (guard && !guard->isClosing_ && guard->requestGeneration_ == generation
                && result.status != CropEditStatus::Succeeded)
                emit guard->hostError(QStringLiteral("恢复来源失败，错误码 %1。").arg(int(result.failureReason)));
        });
    });
    if (!isCurrent()) return false;
    if (!admission.isAccepted) cropBuildPending_ = false;
    emit cropHistoryChanged();
    return admission.isAccepted;
}

bool SessionManager::exitCrop()
{
    CropHostRequest request;
    request.action = CropHostAction::Exit;
    const bool accepted = sendCropAction(request);
    if (accepted) emit cropHistoryChanged();
    return accepted;
}

CropTreeState SessionManager::getCropTreeState() const
{
    CropTreeState tree;
    if (!cropFeature_) return tree;
    const auto state = cropFeature_->GetState();
    auto history = cropFeature_->GetHistory();
    const auto input = hostSession_ ? hostSession_->GetImageDescriptor() : std::optional<ImageDescriptor>{};
    tree.isCropping = state.isActive;
    tree.isBuilding = cropBuildPending_ || state.isPublishing || state.history.pendingRequestCount || state.history.isDragging;
    const bool isSourceDisplayed = input && input->dataRevision == history.sourceRevision;
    tree.canApply = !tree.isBuilding && isSourceDisplayed && history.documentId
        && history.appliedHead != history.rootNodeId && history.renderedHead == history.appliedHead;
    tree.canRestoreOriginal = !tree.isBuilding && history.documentId && input && input->dataRevision != history.sourceRevision;
    const auto revision = history.stateRevision;
    const auto results = history.results;
    for (;;) {
        for (const auto& node : history.nodes) {
            QString text = QStringLiteral("原始输入");
            if (node.operation) {
                const auto& operation = *node.operation;
                const auto shape = operation.geometryType == CropShape::Box ? QStringLiteral("框")
                    : operation.geometryType == CropShape::Plane ? QStringLiteral("平面")
                    : operation.geometryType == CropShape::Sphere ? QStringLiteral("球") : QStringLiteral("圆柱");
                text = QStringLiteral("节点 %1｜%2｜%3").arg(qulonglong(node.nodeId)).arg(shape)
                    .arg(operation.removalMode == CropRemovalMode::RemoveInside ? QStringLiteral("移除内部") : QStringLiteral("保留内部"));
            }
            bool isApplied = false;
            for (const auto& result : results) isApplied = isApplied || (input && result.nodeId == node.nodeId
                && result.status == CropResultStatus::Published && result.outputRevision == input->dataRevision);
            tree.items.push_back({text, node.nodeId, node.nodeId == state.history.appliedHead, isApplied, !tree.isBuilding && isSourceDisplayed});
        }
        if (!history.nextPageAfter) break;
        history = cropFeature_->GetHistory(history.documentId, history.nextPageAfter);
        if (history.stateRevision != revision) { tree.items.clear(); break; }
    }
    return tree;
}

bool SessionManager::startGap(
    GapHostStartParams params,
    GapHostCallback onComplete,
    QString* errorOut)
{
    if (!unbindMeasurements()) { setError(errorOut, QStringLiteral("测量工具尚未释放。")); return false; }
    if (isGapRestoreCleanupPending_ || cropBuildPending_) {
        setError(errorOut, QStringLiteral("裁切恢复或结果释放尚未完成，请稍后重试孔隙分析。"));
        return false;
    }
    if (!hostSession_ || state_ != State::Ready || isClosing_) {
        setError(
            errorOut,
            QStringLiteral("请先加载 RAW 数据或重建结果。"));
        return false;
    }

    if (!gapFeature_ && !resetGapFeature(errorOut)) {
        return false;
    }

    params.targetViews = getGapTargets();

    GapHostRequest request;
    request.action = GapHostAction::Start;
    request.start = std::move(params);

    const auto admission = gapFeature_->SendRequest(
        std::move(request),
        std::move(onComplete));
    if (admission.status != GapAdmissionStatus::Accepted) {
        setError(
            errorOut,
            QStringLiteral("孔隙分析请求未被 SDK 接纳，请检查参数或先退出当前孔隙会话。"));
        return false;
    }

    return true;
}

bool SessionManager::toggleGapOverlay(QString* errorOut)
{
    if (!gapFeature_) {
        setError(errorOut, QStringLiteral("孔隙分析功能尚未初始化。"));
        return false;
    }

    GapHostRequest request;
    request.action = GapHostAction::Overlay;
    const auto admission = gapFeature_->SendRequest(std::move(request));
    if (admission.status != GapAdmissionStatus::Accepted) {
        setError(errorOut, QStringLiteral("当前没有可切换的孔隙分析结果。"));
        return false;
    }
    return true;
}

bool SessionManager::exportGapCsv(const QString& outputPath,
    std::function<void(bool, QString)> onComplete, QString* errorOut)
{
    if (!unbindMeasurements()) { setError(errorOut, QStringLiteral("测量工具尚未释放。")); return false; }
    if (isGapRestoreCleanupPending_ || cropBuildPending_) {
        setError(errorOut, QStringLiteral("裁切恢复或结果释放尚未完成，暂不能导出 CSV。"));
        return false;
    }
    if (getIsGapExporting()) {
        setError(errorOut, QStringLiteral("CSV 正在导出，请等待完成。"));
        return false;
    }
    const auto reader = getGapResultReader();
    if (!reader || outputPath.trimmed().isEmpty() || !onComplete) {
        setError(errorOut, QStringLiteral("无法取得当前来源的孔隙结果或有效保存路径。"));
        return false;
    }
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    try {
        gapExport_ = std::async(std::launch::async, [reader, cancelled, outputPath] {
            return WriteGapCsv(outputPath, reader, cancelled);
        });
    } catch (const std::exception& error) {
        setError(errorOut, QString::fromUtf8(error.what()));
        return false;
    }
    gapExportCancelled_ = std::move(cancelled);
    gapExportCallback_ = std::move(onComplete);
    gapExportTimer_.start();
    return true;
}

bool SessionManager::exitGap(QString* errorOut)
{
    if (!gapFeature_) {
        setError(errorOut, QStringLiteral("孔隙分析功能尚未初始化。"));
        return false;
    }

    GapHostRequest request;
    request.action = GapHostAction::Exit;
    const auto admission = gapFeature_->SendRequest(std::move(request));
    if (admission.status != GapAdmissionStatus::Accepted) {
        setError(errorOut, QStringLiteral("当前没有可退出的孔隙分析会话。"));
        return false;
    }
    return true;
}

GapHostState SessionManager::getGapState() const
{
    return gapFeature_
        ? gapFeature_->GetState()
        : GapHostState{};
}

void SessionManager::syncCropHistory()
{
    if (!cropFeature_) return;
    const auto history = cropFeature_->GetState().history;
    if (cropRevision_ != history.stateRevision || cropRenderedHead_ != history.renderedHead) {
        cropRevision_ = history.stateRevision;
        cropRenderedHead_ = history.renderedHead;
        emit cropHistoryChanged();
    }
}

VtkImageGridSnapshot SessionManager::capturedVolumeSnapshot(QString* err)
{
    if (err) {
        err->clear();
    }

    if (QThread::currentThread() != thread()) {
        setError(err, QStringLiteral("thread must be same one"));
        return {};
    }

    if (!hostSession_ || state_ != State::Ready || cropBuildPending_) {
        setError(err, QStringLiteral("Volume is not ready for capture."));
        return {};
    }

    std::shared_ptr bridge = std::make_shared<VolumeHostBridge>();
    if (!hostSession_->AttachFeature(bridge)) {
        const auto& detail = bridge->GetLastError();
        setError(err, detail.empty()
            ? QStringLiteral("Failed to attach capture feature.")
            : QString::fromStdString(detail));
        return {};
    }
    auto captured = bridge->GetCapturedImage();
    if (!hostSession_->DetachFeature(*bridge)) {
        setError(err, QStringLiteral("Failed to detach volume capture feature."));
        return {};
    }
    if (!captured || !captured->image) {
        setError(err, QStringLiteral("Captured volume is empty."));
        return {};
    }

    return captured;
}

VtkImageGridSnapshot SessionManager::publishVolumeSnapshot(//保存的A/B交给SDK
    const VtkImageGridSnapshot& archive,
    QString* err, bool* isPending)
{
    if (err) err->clear();
    if (isPending) *isPending = false;

    if (QThread::currentThread() != thread()) {
        setError(err, QStringLiteral("Publish must run on the owner thread."));
        return {};
    }
    if (!hostSession_ || isClosing_ || state_ != State::Ready || cropBuildPending_
        || !archive || !archive->image || !archive->data) {
        setError(err, QStringLiteral("Host or archive is not ready."));
        return {};
    }
    if (gapFeature_) {
        setError(err, QStringLiteral(
            "请先关闭孔隙分析，再切换体积。"));
        return {};
    }

    auto fail = [err](const QString& message) -> VtkImageGridSnapshot {
        setError(err, message);
        return {};
        };

    if (!unbindMeasurements()) return fail(QStringLiteral("测量交互尚未释放，请稍后重试。"));

    // 只接受空闲四视图；先安全移除当前裁切上下文。
    if (cropFeature_ && cropFeature_->GetState().isPublishing) {
        setError(err, QStringLiteral("Crop is still publishing."));
        return {};
    }

    cropStateTimer_.stop();
    if (cropFeature_) {
        if (cropFeature_->GetState().isActive) {
            CropHostRequest exit;
            exit.action = CropHostAction::Exit;
            if (!cropFeature_->SendRequest(std::move(exit))) {
                return fail(QStringLiteral("Failed to exit crop editing."));
            }
        }
        if (!hostSession_->DetachFeature(*cropFeature_)) {
            if (isPending) { 
                *isPending = true;
            }  
            SendUpdates();
            SendRender();
            setError(err, QStringLiteral("正在归还裁剪视图，请稍候。"));
            return {};
        }
        cropFeature_.reset();
    }

    clearCropHistory();

    if (!volumePublishBridge_) {
        auto bridge = std::make_shared<VolumePublishBridge>();
        if (!hostSession_->AttachFeature(bridge))
            return fail(QStringLiteral("Failed to attach publish feature."));
        volumePublishBridge_ = std::move(bridge);
    }
    if (!ReleasePublishedVolumes(false, err)) {
        if (isPending) *isPending = true;
        SendUpdates(); SendRender();
        return {};
    }
    const auto scope = volumePublishBridge_->CreateDataEntityId();
    const DataRevisionRef revision{volumePublishBridge_->CreateDataEntityId(), 1};
    std::string detail;
    const auto published = volumePublishBridge_->Publish(archive, scope, revision, detail);
    const bool renderRequested = published && volumePublishBridge_->RequestViewRenders();
    (void)ReleasePublishedVolumes(false);
    if (!published) {
        return fail(QString::fromStdString(detail));
    }
    qDebug() << "[VolumeScope] published scope="
        << QByteArray(reinterpret_cast<const char*>(scope.bytes.data()), 16).toHex()
        << "entity=" << QByteArray(reinterpret_cast<const char*>(revision.entityId.bytes.data()), 16).toHex()
        << "generation=" << revision.generation << "binding=" << published->binding->revision
        << "tracked=" << volumePublishBridge_->GetPublicationCount();
    if (!renderRequested) {
        return fail(QStringLiteral("Failed to request renders for all four views."));
    }

    QString cropError;
    if (!resetCropFeature(&cropError)) return fail(cropError);

    setIsoState();
    SendUpdates();
    return published;
}

bool SessionManager::ReleasePublishedVolumes(bool all, QString* errorOut)
{
    if (!volumePublishBridge_) return true;
    if (!all && volumePublishBridge_->GetPublicationCount() != 0) {
        // 全局换绑不代表异步 3D/切片已采用新输入。旧作用域必须留到四视图
        // 的场景提交完成，否则资源过渡退役会令仍在显示的旧修订不可解析。
        const auto current = hostSession_ ? hostSession_->GetImageDescriptor() : std::nullopt;
        const auto scenes = hostSession_ ? hostSession_->GetSceneViewStates()
                                         : std::vector<HostSceneViewState>{};
        if (!current || scenes.size() != config_.renderViews.size()) return false;
        for (const auto& scene : scenes) {
            if (!scene.isAvailable || !scene.presentation
                || scene.presentation->dataRevision != current->dataRevision
                || scene.presentation->bindingRevision != current->bindingRevision) {
                setError(errorOut, QStringLiteral("正在等待四视图提交新体积，保留旧体积资源。"));
                return false;
            }
        }
    }
    std::string error;
    const bool complete = volumePublishBridge_->ReleasePublications(all, error);
    if (!complete) setError(errorOut, QStringLiteral("旧体积资源尚未归还：%1").arg(QString::fromStdString(error)));
    return complete;
}

bool SessionManager::areRenderViewsSynchronized(
    const DataRevisionRef& expectedRevision,
    DataBindingRevision expectedBindingRevision,
    std::map<std::string, std::uint64_t>& renderTargets,
    QString* report)
{
    if (!hostSession_ || config_.renderViews.size() != 4) {
        if (report) *report = QStringLiteral("Four-view Host is unavailable.");
        return false;
    }

    // 首次调用必须发生在本次激活完成回调中；仅对目标输入的完整显示组冻结
    // epoch。后续无关场景提交不移动这一绘制目标，也不能借用旧输入的 epoch。
    if (renderTargets.empty()) {
        std::map<std::string, std::uint64_t> targets;
        for (const auto& config : config_.renderViews) {
            HostViewTarget target; target.viewId = config.id;
            const auto scene = hostSession_->GetSceneViewState(target);
            if (!scene || !scene->isAvailable || !scene->presentation || !scene->sceneEpoch
                || scene->presentation->dataRevision != expectedRevision
                || scene->presentation->bindingRevision != expectedBindingRevision) {
                if (report) *report = QStringLiteral("激活完成时，显示组尚未提交指定输入。");
                return false;
            }
            targets.emplace(config.id, scene->sceneEpoch);
        }
        renderTargets = std::move(targets);
    }
    bool all = true;
    QStringList parts;
    for (const auto& config : config_.renderViews) {
        HostViewTarget target;
        target.viewId = config.id;
        const auto state = hostSession_->GetSceneViewState(target);
        const QString id = QString::fromStdString(config.id);

        if (!state) {
            all = false;
            parts << (id + QStringLiteral("=unavailable"));
        }
        else {
            const auto expectedEpoch = renderTargets.find(config.id);
            if (!state->isAvailable || !state->presentation
                || state->presentation->dataRevision != expectedRevision
                || state->presentation->bindingRevision != expectedBindingRevision
                || expectedEpoch == renderTargets.end()
                || state->renderedEpoch < expectedEpoch->second) all = false;
            parts << (id + QStringLiteral("=")
                + QString::number(static_cast<qulonglong>(state->presentation ? state->presentation->bindingRevision : 0))
                + QStringLiteral(" target/scene/render=%1/%2/%3")
                    .arg(qulonglong(expectedEpoch == renderTargets.end() ? 0 : expectedEpoch->second))
                    .arg(qulonglong(state->sceneEpoch)).arg(qulonglong(state->renderedEpoch)));
        }
    }
    if (report) *report = parts.join(QStringLiteral(", "));
    return all;
}

void SessionManager::failVolumeActivation(const QString& reason)
{
    qWarning() << "[VolumeActivation] stopped " << reason;
    setState(State::Failed);
    emit isoStateCleared();
}

std::optional<measure::MeasureHostData>
SessionManager::getMeasureHostData(const HostViewTarget& sourceView)
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
    if (!hostSession_ || isClosing_) {
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

    if (!unbindMeasurements()) { setError(errorOut, QStringLiteral("测量工具尚未释放。")); return false; }

    // CT 接管请求存储；旧 Crop 文档的异步退出完成后再交给 SDK，避免丢弃重建 buffer。
    if (auto* file = dynamic_cast<HostLoadRequest*>(&request)) pendingLoad_ = std::make_unique<HostLoadRequest>(std::move(*file));
    else if (auto* memory = dynamic_cast<HostReloadRequest*>(&request)) pendingLoad_ = std::make_unique<HostReloadRequest>(std::move(*memory));
    else { setError(errorOut, QStringLiteral("未知加载请求。")); return false; }
    ++requestGeneration_;
    sourcePath_.clear();
    pendingSourcePath_ = sourcePath;
    setState(State::Loading);
    emit isoStateCleared();
    loadRetryClock_.restart();
    SendPendingLoad();
    return true;
}

void SessionManager::finishLoadRequest(//更新状态
    std::uint64_t generation,
    HostResult result)
{
    const bool isSuccess = result.isSucceeded;
    if (generation != requestGeneration_) {
        return;
    }

    if (isSuccess) {
        (void)ReleasePublishedVolumes(false);
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
        : QString::fromStdString(result.message));
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


void SessionManager::SetViewVisibility(std::function<bool(const std::string&)> getVisible)
{
    getVisible_ = std::move(getVisible);
}


void SessionManager::SendUpdates()
{
    if (!hostSession_ || isClosing_) return;
    const auto result = hostSession_->SendUpdates();
    pendingRender_.insert(result.renderViewIds.begin(), result.renderViewIds.end());
    (void)ReleasePublishedVolumes(false);
    if (result.status == HostUpdateStatus::Failed) emit hostError(QStringLiteral("SDK 视图更新失败，等待后续工作事件重试。"));
    syncCropHistory();
    if (!renderTimer_.isActive()) renderTimer_.start(static_cast<int>(qMax<qint64>(0, nextRenderAt_ - renderClock_.elapsed())));
}


void SessionManager::SendRender()
{
    if (!hostSession_ || isClosing_) return;
    HostRenderRequest request;
    request.desiredUpdateRate = 30.0;
    for (const auto& id : pendingRender_) if (!getVisible_ || getVisible_(id)) request.viewIds.push_back(id);
    if (request.viewIds.empty()) return;
    const auto result = hostSession_->SendRender(request);
    bool wasRendered = false;
    for (const auto& view : result.views) {
        if (view.status == HostRenderStatus::Rendered || view.status == HostRenderStatus::Unchanged) pendingRender_.erase(view.viewId);
        if (view.status == HostRenderStatus::Failed) emit hostError(QStringLiteral("SDK 视图绘制失败：%1").arg(QString::fromStdString(view.viewId)));
        wasRendered = wasRendered || view.status == HostRenderStatus::Rendered;
    }
    (void)ReleasePublishedVolumes(false);
    nextRenderAt_ = renderClock_.elapsed() + 33;
    // GPU effect 完成后再推进一次 owner 更新；空闲和隐藏窗口不持续自激。
    if (wasRendered) QTimer::singleShot(0, this, &SessionManager::SendUpdates);
}


void SessionManager::RequestRender()
{
    SendUpdates();
}


bool SessionManager::Stop(QString* errorOut)
{
    if (!isClosing_) {
        isClosing_ = true; ++requestGeneration_;
        loadRetryTimer_.stop(); pendingLoad_.reset();
    }
    return StopHost(errorOut);
}


bool SessionManager::GetIsStopped() const { return !hostSession_ || hostSession_->GetIsStopped(); }


bool SessionManager::SetCropWidget(CropHostAction shape)
{
    CropHostRequest request;
    request.action = shape;
    request.target = getCropTarget();
    if (!cropFeature_->SendRequest(request)) return false;
    request = {};
    request.action = CropHostAction::Mode;
    request.target = getCropTarget();
    request.removalMode = CropRemovalMode::KeepInside;
    const bool accepted = cropFeature_->SendRequest(request);
    if (!accepted) {
        CropHostRequest exit; exit.action = CropHostAction::Exit;
        cropFeature_->SendRequest(exit);
        emit hostError(QStringLiteral("裁剪模式设置失败，已退出编辑。"));
    }
    syncCropHistory();
    emit cropHistoryChanged();
    return accepted;
}


bool SessionManager::StartCrop(CropHostAction shape)
{
    if (!unbindMeasurements()) return false;
    if (state_ != State::Ready || isClosing_ || !cropFeature_ || cropBuildPending_) return false;
    const auto input = hostSession_->GetImageDescriptor();
    if (!input) return false;
    const auto history = cropFeature_->GetHistory();
    if (history.documentId && history.sourceRevision == input->dataRevision) return SetCropWidget(shape);

    CropDocumentRequest request;
    request.action = CropDocumentAction::CreateDocument;
    request.requestId = CropHostFeature::CreateRequestId();
    request.target = getCropTarget();
    request.sourceRevision = input->dataRevision;
    cropBuildPending_ = true;
    const auto generation = requestGeneration_;
    QPointer<SessionManager> guard(this);
    const auto admission = cropFeature_->SendRequest(request, [guard, generation, shape](CropDocumentOutcome result) {
        if (!guard) return;
        QTimer::singleShot(0, guard, [guard, generation, shape, result] {
            if (!guard || guard->requestGeneration_ != generation || guard->isClosing_) return;
            guard->cropBuildPending_ = false;
            const bool succeeded = result.status == CropEditStatus::Succeeded && guard->SetCropWidget(shape);
            if (!succeeded) emit guard->hostError(QStringLiteral("裁剪启动失败，错误码 %1。").arg(int(result.failureReason)));
            emit guard->cropHistoryChanged();
        });
    });
    if (!admission.isAccepted) cropBuildPending_ = false;
    emit cropHistoryChanged();
    return admission.isAccepted;
}


std::shared_ptr<const GapResultReader> SessionManager::getGapResultReader() const
{
    if (!gapFeature_ || !hostSession_ || state_ != State::Ready
        || hostSession_->GetStopState() != HostStopState::Running) return {};
    const auto state = gapFeature_->GetState();
    const auto source = hostSession_->GetImageDescriptor();
    if (!source || state.analysisState != GapAnalysisState::Succeeded
        || source->dataRevision != state.sourceRevision) return {};
    const auto reader = gapFeature_->GetResultReader(state.resultSet);
    return reader ? reader->CreateObjectReader() : nullptr;
}


bool SessionManager::getIsGapExporting() const { return gapExport_.valid(); }


void SessionManager::finishGapExport()
{
    if (!gapExport_.valid() || gapExport_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    gapExportTimer_.stop();
    std::pair<bool, QString> result;
    try { result = gapExport_.get(); }
    catch (const std::exception& error) { result = {false, QString::fromUtf8(error.what())}; }
    auto callback = std::move(gapExportCallback_);
    gapExportCancelled_.reset();
    if (callback) callback(result.first, std::move(result.second));
}


void SessionManager::SendPendingLoad()
{
    if (!pendingLoad_ || isClosing_ || !hostSession_) { loadRetryTimer_.stop(); return; }
    bool isGapClosed = !gapFeature_;
    if (gapFeature_ && hostSession_->DetachFeature(*gapFeature_)) {
        gapFeature_.reset();
        isGapRestoreCleanupPending_ = false;
        isGapClosed = true;
    }
    if (!isGapClosed || !resetCropFeature()) {
        if (loadRetryClock_.elapsed() >= 30000) {
            pendingLoad_.reset(); loadRetryTimer_.stop();
            HostResult result; result.message = "Previous Gap/crop cleanup timed out; previous resources remain retained.";
            finishLoadRequest(requestGeneration_, std::move(result));
            return;
        }
        SendUpdates();
        if (!loadRetryTimer_.isActive()) loadRetryTimer_.start();
        return;
    }
    loadRetryTimer_.stop();
    const auto generation = requestGeneration_;
    auto request = std::move(pendingLoad_);
    QPointer<SessionManager> guard(this);
    const auto onComplete = [guard,generation](HostResult result) {
        if (!guard) return;
        QTimer::singleShot(0,guard,[guard,generation,result=std::move(result)]() mutable {
            if (guard) guard->finishLoadRequest(generation,std::move(result));
        });
    };
    const auto admission = hostSession_->SendRequest(std::move(*request), onComplete);
    // 请求已转交 SDK；拒绝也必须结束 Loading，不能等待不存在的完成回调。
    if (admission.status != HostAdmissionStatus::Accepted) {
        onComplete({ false, admission.errorCode, admission.message });
    }
}

bool SessionManager::bindMeasurements(std::shared_ptr<measure::MeasurementSession> session)
{
    if (!hostSession_ || state_ != State::Ready || !unbindMeasurements()) return false;
    auto feature = std::make_shared<measure::MainMeasurementFeature>(std::move(session),
        [this](const HostViewTarget& target) { return getRenderViewState(target); });
    if (!hostSession_->AttachFeature(feature)) return false;
    measurementFeature_ = std::move(feature);
    RequestRender();
    return true;
}

bool SessionManager::unbindMeasurements()
{
    if (!measurementFeature_) return true;
    if (hostSession_ && !hostSession_->DetachFeature(*measurementFeature_)) return false;
    measurementFeature_.reset();
    return true;
}

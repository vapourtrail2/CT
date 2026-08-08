#include "c_ui/context/SessionManager.h"
#include "Host/Types/HostRequest.h"

#include <QByteArray>
#include <QMetaObject>
#include <QPointer>

#include <array>
#include <limits>
#include <utility>
#include <cmath>


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
}

SessionManager::~SessionManager() = default;

bool SessionManager::initHost(HostSessionConfig config, QString* err)
{
    if (config.renderViews.empty()) {
        setError(
            err,
            QStringLiteral("Host render view configuration is empty."));
        return false;
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
    hostSession_.reset();

    if (!hasConfig_ || config_.renderViews.empty()) {
        setError(
            errorOut,
            QStringLiteral("Host has not been configured."));
        return false;
    }

    auto newHost =
        std::make_unique<VtkAppHostSession>(config_);

    if (!newHost->BuildSession()) {
        setError(
            errorOut,
            QStringLiteral("Failed to build VTK host session."));
        return false;
    }

    hostSession_ = std::move(newHost);
    return true;
}



bool SessionManager::openFile(const QString& path,
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

    request.geometry.dimensions = { 0,0,0 };
    request.geometry.spacing = spacing;
    request.geometry.origin = origin;

    return sendLoadRequest(std::move(request), p, errorOut);
}

//bool SessionManager::openReconstructedData(
//    const float* data,
//    const std::array<int, 3>& dims,
//    const std::array<float, 3>& spacing,
//    const std::array<float, 3>& origin,
//    const QString& sourcePath,
//    QString* errorOut)
//{
//    if (!data) {
//        setError(
//            errorOut,
//            QStringLiteral("Reconstruction buffer is null."));
//        return false;
//    }
//
//    std::size_t voxelCount = 0;
//    if (!isVoxelCountRight(dims,voxelCount)) {
//        setError(
//            errorOut,
//            QStringLiteral("Invalid reconstruction dimensions."));
//        return false;
//    }
//
//    HostReloadRequest request;
//
//    // HostReloadRequest 自己拥有数据，不能只把外部 float* 传给 core。
//	request.voxels.assign(data, data + voxelCount);//把数据拷贝到 request.voxels 中  复制的 有点问题
//    request.geometry.dimensions = dims;
//    request.geometry.spacing = spacing;
//    request.geometry.origin = origin;
//
//    const QString displayPath = sourcePath.trimmed().isEmpty()
//        ? QStringLiteral("CT reconstruction")
//        : sourcePath.trimmed();
//
//    return sendLoadRequest(
//        std::move(request),
//        displayPath,
//        errorOut);
//}

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

    return hostSession_->SendRequest(
        std::move(request),
        std::move(onComplete));
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

    const std::uint64_t generation = ++requestGeneration_;

    sourcePath_.clear();
    pendingSourcePath_ = sourcePath;
    setState(State::Loading);
	emit isoStateCleared();//清除 isoStateChanged 信号 加载第二个文件第一个文件的iosvalue就不会显示

    QPointer<SessionManager> ptr(this);
    const bool started = hostSession_->SendRequest(
        std::move(request),
        [ptr, generation](bool isSuccess) {
            if (!ptr) {
                return;
            }

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
        // Session 构建失败或目标视图不存在。
        return;
    }

    const double iso = state->isoThreshold;
    const auto scalarRange = state->scalarRange;

    if (!state || !state->isDataReady) {

        emit isoStateCleared();
        return;
    }

    const double scalarMin = state->scalarRange[0];
    const double scalarMax = state->scalarRange[1];
    const double isoValue = state->isoValue;

    if (!std::isfinite(isoValue)
        || !std::isfinite(scalarMin)
        || !std::isfinite(scalarMax)
        || scalarMax <= scalarMin) {
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
    // == 和 =
    
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

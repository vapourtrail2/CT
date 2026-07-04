#include "c_ui/context/SessionManager.h"
#include <QFileInfo>
#include <qapplication.h>
#include <QDir>

//会话协调层，负责管理 AppSession 的生命周期和状态转换
namespace {
std::shared_ptr<AbstractDataManager> CreateManagerForPath(const QString& path)
{
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    if (info.isDir() || suffix == QStringLiteral("tif") || suffix == QStringLiteral("tiff")) {
        return std::make_shared<TiffVolumeDataManager>();
    }
    return std::make_shared<RawVolumeDataManager>();
}
}

SessionManager::SessionManager(QObject* parent)
    : QObject(parent)
{
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
    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = createDataManagerForPath(p);

	newSession->sharedStateBroadcaster = std::make_shared<SharedStateBroadcaster>();
    newSession->sharedState = std::make_shared<SharedInteractionState>(newSession->sharedStateBroadcaster);
	newSession->service = std::make_shared<MedicalVizService>(newSession->dataMgr, newSession->sharedState,newSession->sharedStateBroadcaster);
    newSession->sourcePath = p;
	newSession->analysisService = std::make_shared<VolumeAnalysisService>(newSession->dataMgr);
    
	auto weakSession = std::weak_ptr<AppSession>(newSession);
    
	const QString nativePath = QDir::toNativeSeparators(p);//分隔符转换，确保在不同平台上路径格式正确
	const QByteArray localPath = nativePath.toLocal8Bit();//这句话的目的是将 QString 转换为本地编码的字节数组，toLocal8Bit() 会根据当前系统的编码设置将 QString 转换为适当的字节序列，确保路径字符串在不同平台上都能正确处理。

    newSession->service->SetFileLoadedAsync(
        std::string(localPath.constData(), localPath.size()),
        spacing,
        origin,
        [](bool) {}
    );
    m_session = newSession;
    emit sessionChanged(m_session);
	return true;
}

bool SessionManager::openReconstructedData(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    QString* errorOut)
{
    auto rawDataManager = std::make_shared<RawVolumeDataManager>();
	auto sharedStateBroadcaster = std::make_shared<SharedStateBroadcaster>();
    auto sharedState = std::make_shared<SharedInteractionState>(sharedStateBroadcaster);
	auto service = std::make_shared<MedicalVizService>(rawDataManager, sharedState, sharedStateBroadcaster);

    auto newSession = std::make_shared<AppSession>();
    newSession->dataMgr = rawDataManager;
	newSession->sharedStateBroadcaster = sharedStateBroadcaster;
    newSession->sharedState = sharedState;
    newSession->service = service;
    newSession->sourcePath = sourcePath.trimmed().isEmpty()
        ? QStringLiteral("CT reconstruction")
        : sourcePath.trimmed();
	newSession->analysisService = std::make_shared<VolumeAnalysisService>(rawDataManager);
    
	// 弱引用，用于托管回调中的 this 指针，避免循环引用导致内存泄漏
	auto weakSession = std::weak_ptr<AppSession>(newSession);
    const bool started = newSession->service->SetReloadFromBufferAsync(data, dims, spacing, origin, 
        [](bool){}
    );

    if (!started)
    {
        if (errorOut) *errorOut = QStringLiteral("Service is busy or initialization failed.");
        return false;
    }

    m_session = newSession;
    emit sessionChanged(m_session);

    return true;
}

std::shared_ptr<AbstractDataManager> SessionManager::createDataManagerForPath(const QString& path) const
{
    return CreateManagerForPath(path);
}

void SessionManager::clearSession()
{
    m_session.reset();
    emit sessionChanged(nullptr);
}

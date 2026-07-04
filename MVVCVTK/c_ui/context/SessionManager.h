#pragma once
#include <QObject>
#include <array>
#include <memory>
#include <QString>
#include "Data/DataManager.h"
#include "App/AppInterfaces.h"
#include "App/AppState.h"  
#include "Service/AppService.h"
#include "Service/VolumeAnalysisService.h"
//把这一套后端对象，也就是后端的原子操作组起来 变成session 保持同步 供ui使用 其实也就是会话层
//打开文件
//创建当前这次加载所需要的后端对象
//保存“当前数据会话”
//把这次会话交给各个页面
struct AppSession
{
    std::shared_ptr<AbstractDataManager> dataMgr;// 用抽象接口
    std::shared_ptr<SharedStateBroadcaster> sharedStateBroadcaster;
    std::shared_ptr<SharedInteractionState> sharedState; // ban makeshare
    std::shared_ptr<VolumeAnalysisService> analysisService; // std::function<void(bool)> 
	std::shared_ptr<MedicalVizService> service;
    QString sourcePath; // ban
};

class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(QObject* parent = nullptr);

    std::shared_ptr<AppSession> getSession() const { return m_session; }

    bool openFile(const QString& path,
        const std::array<float, 3>& spacing,
        const std::array<float,3>& origin ,
        QString* errorOut = nullptr
        );
    bool openReconstructedData(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        const QString& sourcePath,  
        QString* errorOut = nullptr);

    void clearSession();

signals:
    void sessionChanged(std::shared_ptr<AppSession> session);

private:
    std::shared_ptr<AbstractDataManager> createDataManagerForPath(const QString& path) const;

    std::shared_ptr<AppSession> m_session;
};


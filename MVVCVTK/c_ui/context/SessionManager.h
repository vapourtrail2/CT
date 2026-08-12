#pragma once

#include "Host/Types/HostRequestTypes.h"
#include "Host/VtkAppHostSession.h"
#include "App/AppInterfaces.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <QObject>
#include <QString>

struct HostRequest;
class VtkAppHostSession;

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

    ImageSnapshot getImageSnapshot();
    std::optional<HostRenderViewState> getRenderViewState(
        const HostViewTarget& target);
    std::optional<std::array<double, 16>> getRenderViewModelMatrix(
        const std::string& viewId);

    void clearSession();

signals:
    void sessionChanged(SessionManager::State state);
    void loadFinished(bool issucc, QString message);
    void isoStateChanged(
        double isoValue,
        double scalarMin,
        double scalarMax);
    void isoStateCleared();

private:
    bool resetHost(QString* errorOut);
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
    State state_ = State::Empty;
    std::uint64_t requestGeneration_ = 0;//什么意思
    QString sourcePath_;
    QString pendingSourcePath_;//什么意思
};

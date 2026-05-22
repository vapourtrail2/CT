#pragma once

#include <array>
#include <QString>
#include <memory>

class AppController;
struct AppSession;

class ReconstructPage;
class SceneTreePanel;
class RenderPanel;

class WorkspaceFlow
{
public:
    explicit WorkspaceFlow(AppController* controller);

    bool openFile(const QString& path, const std::array<float,3> &spacing, const std::array<float, 3>& origin, QString* error);
    bool openReconstructedData(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        const QString& sourcePath,
        QString* err);
    std::shared_ptr<AppSession> session() const;
    bool hasData() const;

    bool bindSession(
        ReconstructPage* reconstructPage,
        SceneTreePanel* scenePanel,
        RenderPanel* renderPanel,
        QString* err);

    bool openAndBind(
        const QString& path,
        ReconstructPage* reconstructPage,
        SceneTreePanel* scenePanel,
        RenderPanel* renderPanel,
        QString* err);
    bool openReconstructedAndBind(
        const float* data,
        const std::array<int, 3>& dims,
        const std::array<float, 3>& spacing,
        const std::array<float, 3>& origin,
        const QString& sourcePath,
        ReconstructPage* reconstructPage,
        SceneTreePanel* scenePanel,
        RenderPanel* renderPanel,
        QString* err);

private:
    AppController* controller_ = nullptr;
};
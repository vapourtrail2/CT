#include "c_ui/nav/WorkspaceFlow.h"
#include "AppController.h"
#include "c_ui/workbenches/ReconstructPage.h"
#include "c_ui/panels/SceneTreePanel.h"
#include "c_ui/panels/RenderPanel.h"

WorkspaceFlow::WorkspaceFlow(AppController* controller)
    : controller_(controller)
{
}

bool WorkspaceFlow::openFile(const QString& path ,const std::array<float,3> & spacing, const std::array<float, 3>& origin,QString* err)
{
    if (!controller_) {
        if (err) {
            *err = QStringLiteral("Invalid AppController.");
        }
        return false;
    }
    return controller_->openFile(path,spacing, origin, err);
}

bool WorkspaceFlow::openReconstructedData(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    QString* err)
{
    if (!controller_) {
        if (err) {
            *err = QStringLiteral("Invalid AppController.");
        }
        return false;
    }
    return controller_->openReconstructedData(data, dims, spacing, origin, sourcePath, err);
}

std::shared_ptr<AppSession> WorkspaceFlow::session() const
{
    if (!controller_) {
        return nullptr;
    }
    return controller_->session();
}

bool WorkspaceFlow::hasData() const
{
    const auto s = session();
    return s && s->dataMgr && s->sharedState;
}

bool WorkspaceFlow::bindSession(
    ReconstructPage* reconstructPage,
    SceneTreePanel* scenePanel,
    RenderPanel* renderPanel,
    QString* err)
{
    const auto s = session();
    if (!s || !s->dataMgr || !s->sharedState) {
        if (err) {
            *err = QStringLiteral("Invalid session.");
        }
        return false;
    }

    if (reconstructPage) {
        reconstructPage->initWithData(s->dataMgr, s->sharedState,s->sharedStateBroadcaster);
    }
    if (scenePanel) {
        scenePanel->setSession(s->dataMgr, s->sharedState,s->sharedStateBroadcaster, s->sourcePath);
    }
    if (renderPanel) {
        renderPanel->setSession(s);
    }
    return true;
}

bool WorkspaceFlow::openAndBind(
    const QString& path,
    ReconstructPage* reconstructPage,
    SceneTreePanel* scenePanel,
    RenderPanel* renderPanel,
    QString* err)
{
    /*if (!openFile(path, err,)) {
        return false;
    }*/

    if (!bindSession(reconstructPage, scenePanel, renderPanel, err)) {
        return false;
    }

    return true;
}

bool WorkspaceFlow::openReconstructedAndBind(
    const float* data,
    const std::array<int, 3>& dims,
    const std::array<float, 3>& spacing,
    const std::array<float, 3>& origin,
    const QString& sourcePath,
    ReconstructPage* reconstructPage,
    SceneTreePanel* scenePanel,
    RenderPanel* renderPanel,
    QString* err)
{
    if (!openReconstructedData(data, dims, spacing, origin, sourcePath, err)) {
        return false;
    }

    if (!bindSession(reconstructPage, scenePanel, renderPanel, err)) {
        return false;
    }

    return true;
}


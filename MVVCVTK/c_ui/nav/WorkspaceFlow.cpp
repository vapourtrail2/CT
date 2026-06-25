#include "c_ui/nav/WorkspaceFlow.h"
#include "AppController.h"

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




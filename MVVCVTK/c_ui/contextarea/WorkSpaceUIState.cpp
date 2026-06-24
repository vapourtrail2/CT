#include "c_ui/contextarea/WorkSpaceUIState.h"

WorkSpaceUIState::WorkSpaceUIState(QObject* parent)
    :QObject(parent)
{
}

VizMode WorkSpaceUIState::getPrimary3DMode() const
{
    return primary3DMode_;
}

void WorkSpaceUIState::setPrimary3DMode(VizMode mode)
{
    if (primary3DMode_ == mode) {
        return;
    }

    primary3DMode_ = mode;
    emit primary3DModeChanged(mode);
}


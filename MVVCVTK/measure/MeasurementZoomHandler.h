#pragma once

#include "Interaction/IInteractionHandler.h"

#include <memory>

class AbstractInteractiveService;
class vtkRenderer;
class Viewer2DHandler;

namespace measure {

// 测量对话框专用交互：只允许右键拖动缩放，并吞掉其它原生 2D 鼠标交互。
class MeasurementZoomHandler : public IInteractionHandler {
public:
    MeasurementZoomHandler(
        AbstractInteractiveService* service,
        vtkRenderer* renderer);
    ~MeasurementZoomHandler() override;

    InteractionResult GetHandleResult(const InteractionEvent& event) override;

private:
    std::unique_ptr<Viewer2DHandler> m_viewer2D;
    bool m_zooming = false;
};

} // namespace measure

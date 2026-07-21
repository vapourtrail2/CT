#pragma once

#include "Interaction/IInteractionHandler.h"

class AbstractInteractiveService;
class vtkRenderer;

namespace measure {

// 测量对话框专用交互：只允许右键拖动缩放，并吞掉其它原生 2D 鼠标交互。
class MeasurementZoomHandler : public IInteractionHandler {
public:
    MeasurementZoomHandler(
        AbstractInteractiveService* service,
        vtkRenderer* renderer);

    InteractionResult GetHandleResult(const InteractionEvent& event) override;

private:
    AbstractInteractiveService* m_service = nullptr;
    vtkRenderer* m_renderer = nullptr;
    bool m_zooming = false;
    int m_startY = 0;
    double m_startScale = 1.0;
};

} // namespace measure

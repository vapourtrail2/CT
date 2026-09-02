#pragma once

#include "MVVCVTK/SPI/Interaction/InteractionTypes.h"

class vtkRenderer;

namespace measure {

class MeasureViewAdapter;

// 测量对话框专用交互：只允许右键拖动缩放，并吞掉其它原生 2D 鼠标交互。
class MeasurementZoomHandler final {
public:
    MeasurementZoomHandler(
        MeasureViewAdapter* adapter,
        vtkRenderer* renderer);

    InteractionResult Send(const InteractionEvent& event);

private:
    MeasureViewAdapter* m_adapter = nullptr;
    vtkRenderer* m_renderer = nullptr;
    bool m_zooming = false;
    int m_lastY = 0;
};

} 

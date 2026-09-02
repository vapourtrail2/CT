#pragma once

#include "MVVCVTK/SPI/Interaction/InteractionTypes.h"
#include "measure/MeasurementTypes.h"

#include <memory>
#include <optional>

class vtkRenderer;

namespace measure {

class MeasurementSession;
class MeasureViewAdapter;

class MeasurementInteractionHandler final {
public:
    MeasurementInteractionHandler(  
        const std::shared_ptr<MeasurementSession>& session,
        MeasureViewAdapter* adapter,
        vtkRenderer* renderer,
        MeasureView view);

    InteractionResult Send(const InteractionEvent& event);

private:
    std::optional<Point3> GetDisplayPoint(int x, int y) const;
    MeasurementPlane GetMeasurePlane() const;
    bool GetIsInsideImage(const Point3& physicalPoint) const;

    std::shared_ptr<MeasurementSession> m_session;
    MeasureViewAdapter* m_adapter = nullptr;
    vtkRenderer* m_renderer = nullptr;
    MeasureView m_view = MeasureView::Axial;
    bool m_consumingLeftButton = false;
};

} // namespace measure

#pragma once

#include "Interaction/IInteractionHandler.h"
#include "measure/MeasurementTypes.h"

#include <memory>
#include <optional>

class AbstractDataManager;
class AbstractInteractiveService;
class vtkRenderer;

namespace measure {

class MeasurementSession;

class MeasurementInteractionHandler : public IInteractionHandler {
public:
    MeasurementInteractionHandler(
        const std::shared_ptr<MeasurementSession>& session,
        const std::shared_ptr<AbstractDataManager>& dataManager,
        AbstractInteractiveService* service,
        vtkRenderer* renderer,
        MeasureView view);

    InteractionResult GetHandleResult(const InteractionEvent& event) override;

private:
    std::optional<Point3> DisplayToPhysical(int x, int y) const;
    MeasurementPlane SnapshotPhysicalPlane() const;
    bool IsInsideImage(const Point3& physicalPoint) const;

    std::shared_ptr<MeasurementSession> m_session;
    std::shared_ptr<AbstractDataManager> m_dataManager;
    AbstractInteractiveService* m_service = nullptr;
    vtkRenderer* m_renderer = nullptr;
    MeasureView m_view = MeasureView::Axial;
    bool m_consumingLeftButton = false;
};

} // namespace measure

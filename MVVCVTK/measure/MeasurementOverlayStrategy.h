#pragma once

#include "Render/Strategies/BaseVisualStrategy.h"
#include "measure/MeasurementTypes.h"

#include <memory>
#include <vector>

class InteractiveService;
class vtkProp;

namespace measure {

class MeasurementSession;

class MeasurementOverlayStrategy : public BaseVisualStrategy {
public:
    MeasurementOverlayStrategy(
        const std::shared_ptr<MeasurementSession>& session,
        InteractiveService* service,
        MeasureView view);

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void AttachRenderer(vtkSmartPointer<vtkRenderer> renderer) override;
    void DetachRenderer(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetVisualState(
        const RenderParams& params,
        UpdateFlags flags = UpdateFlags::All) override;

    void SetView(MeasureView view);
    void Refresh();

private:
    std::vector<Point3> ProjectToCurrentView(const std::vector<Point3>& physicalPath) const;
    Point3 PhysicalToWorld(const Point3& physical) const;
    bool SourcePlaneMatches(const MeasurementEntity& entity) const;
    void AddPath(const std::vector<Point3>& worldPath, bool dashed, bool draft);
    void AddControlPoints(const std::vector<Point3>& worldPoints, bool draft);
    void AddLabel(const MeasurementEntity& entity, const std::vector<Point3>& worldPath);
    void RemoveProps();
        
    std::shared_ptr<MeasurementSession> m_session;
    InteractiveService* m_service = nullptr;
    MeasureView m_view = MeasureView::Axial;
    vtkSmartPointer<vtkRenderer> m_renderer;
    std::vector<vtkSmartPointer<vtkProp>> m_props;
};

} // namespace measure

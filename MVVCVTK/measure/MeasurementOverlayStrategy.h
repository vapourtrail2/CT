#pragma once

#include "measure/MeasurementTypes.h"

#include <memory>
#include <vector>
#include <vtkSmartPointer.h>

class vtkProp;
class vtkRenderer;

namespace measure {

class MeasurementSession;
class MeasureViewAdapter;

class MeasurementOverlayStrategy final {
public:
    MeasurementOverlayStrategy(
        const std::shared_ptr<MeasurementSession>& session,
        MeasureViewAdapter* adapter,
        MeasureView view);

    void AttachRenderer(vtkRenderer* renderer);
    void DetachRenderer();

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
    MeasureViewAdapter* m_adapter = nullptr;
    MeasureView m_view = MeasureView::Axial;
    vtkSmartPointer<vtkRenderer> m_renderer;
    std::vector<vtkSmartPointer<vtkProp>> m_props;
};

} // namespace measure

#pragma once

#include "App/AppInterfaces.h"
#include "measure/MeasurementTypes.h"

#include <memory>
#include <vector>

class AbstractInteractiveService;
class vtkProp;

namespace measure {

class MeasurementSession;

class MeasurementOverlayStrategy : public AbstractVisualStrategy {
public:
    MeasurementOverlayStrategy(
        const std::shared_ptr<MeasurementSession>& session,
        AbstractInteractiveService* service,
        MeasureView view);

    void SetInputData(vtkSmartPointer<vtkDataObject> data) override;
    void SetRendererAttached(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetRendererDetached(vtkSmartPointer<vtkRenderer> renderer) override;
    void SetVisualState(
        const RenderParams& params,
        UpdateFlags flags = UpdateFlags::All) override;

    void SetView(MeasureView view);
    void Refresh();

private:
    std::vector<Point3> EntityPath(const MeasurementEntity& entity) const;
    std::vector<Point3> DraftPath(const MeasurementDraft& draft) const;
    std::vector<Point3> ProjectToCurrentView(const std::vector<Point3>& physicalPath) const;
    Point3 PhysicalToWorld(const Point3& physical) const;
    bool SourcePlaneMatches(const MeasurementEntity& entity) const;
    Point3 CurrentWorldNormal() const;
    void AddPath(const std::vector<Point3>& worldPath, bool dashed, bool draft);
    void AddControlPoints(const std::vector<Point3>& worldPoints, bool draft);
    void AddLabel(const MeasurementEntity& entity, const std::vector<Point3>& worldPath);
    void RemoveProps();

    std::shared_ptr<MeasurementSession> m_session;
    AbstractInteractiveService* m_service = nullptr;
    MeasureView m_view = MeasureView::Axial;
    vtkSmartPointer<vtkRenderer> m_renderer;
    std::vector<vtkSmartPointer<vtkProp>> m_props;
};

} // namespace measure

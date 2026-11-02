#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementToolDefinition.h"
#include "measure/MeasurementView.h"
#include "measure/MeasureViewAdapter.h"

#include <vtkActor.h>
#include <vtkBillboardTextActor3D.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

#include <algorithm>
#include <cmath>

namespace measure {

MeasurementOverlayStrategy::MeasurementOverlayStrategy(
    const std::shared_ptr<MeasurementSession>& session,
    MeasureViewAdapter* adapter,
    MeasureView view)
    : m_session(session)
    , m_adapter(adapter)
    , m_view(view)
{
}

void MeasurementOverlayStrategy::AttachRenderer(
    vtkRenderer* renderer)
{
    if (!renderer) {
        return;
    }

    if (m_renderer == renderer) {
        Refresh();
        return;
    }

    RemoveProps();
    m_renderer = renderer;
    Refresh();
}

void MeasurementOverlayStrategy::DetachRenderer()
{
    RemoveProps();
    m_renderer = nullptr;
}

void MeasurementOverlayStrategy::SetView(MeasureView view)
{
    if (m_view == view) {
        return;
    }
    m_view = view;
    Refresh();
}

void MeasurementOverlayStrategy::Refresh()
{
    if (!m_renderer) {
        return;
    }
    RemoveProps();
    if (!m_session || !m_adapter) {
        return;
    }

    for (const auto& entity : m_session->Entities()) {
        if (!entity.visible) continue;
        m_highlight = entity.id == m_session->SelectedId();
        const auto& definition = GetMeasurementToolDefinition(entity.type);
        if (!definition.buildPath) {
            continue;
        }
        const auto physicalPath = entity.capturedPath.empty()
            ? definition.buildPath(entity.physicalPoints, entity.plane, &entity.result) : entity.capturedPath;
        const auto worldPath = ProjectToCurrentView(physicalPath);
        if (worldPath.size() < 2) {
            continue;
        }
        if (m_is3D) AddPath(worldPath, false, false);
        else AddSlicePath(physicalPath, m_adapter->GetPlane().sliceTolerance);
        AddControlPoints(ProjectToCurrentView(entity.physicalPoints), false);
        AddLabel(entity, worldPath);
    }

    m_highlight = false;
    if (const auto& draft = m_session->Draft();
        draft && (m_is3D || draft->request.view == m_view)) {
        std::vector<Point3> candidate = draft->physicalPoints;
        if (draft->previewPoint) {
            candidate.push_back(*draft->previewPoint);
        }
        std::vector<Point3> physicalPath = candidate;
        const auto& definition = GetMeasurementToolDefinition(draft->request.tool);
        if (draft->plane && definition.buildPath) {
            physicalPath = definition.buildPath(candidate, *draft->plane, nullptr);
        }
        const auto worldPath = ProjectToCurrentView(physicalPath);
        if (worldPath.size() >= 2) {
            AddPath(worldPath, false, true);
        }
        std::vector<Point3> draftControls = draft->physicalPoints;
        if (draft->previewPoint) {
            draftControls.push_back(*draft->previewPoint);
        }
        AddControlPoints(ProjectToCurrentView(draftControls), true);
    }
}

std::vector<Point3> MeasurementOverlayStrategy::ProjectToCurrentView(
    const std::vector<Point3>& physicalPath) const
{
    std::vector<Point3> projected;
    projected.reserve(physicalPath.size());
    const Point3 normal = m_adapter->GetWorldVector(
        GetSliceViewDescriptor(m_view).normal);
    const auto cursor = m_adapter->GetCursorWorld();
    const Point3 origin{ cursor[0], cursor[1], cursor[2] };
    constexpr double safeOffset = 0.01;
    for (const auto& physical : physicalPath) {
        Point3 world = PhysicalToWorld(physical);
        if (m_is3D) { projected.push_back(world); continue; }
        const double distance = geometry::Dot(normal, geometry::Subtract(world, origin));
        world = geometry::Subtract(world, geometry::Scale(normal, distance));
        world = geometry::Add(world, geometry::Scale(normal, safeOffset));
        projected.push_back(world);
    }
    return projected;
}

Point3 MeasurementOverlayStrategy::PhysicalToWorld(const Point3& physical) const
{
    return m_adapter->GetWorldPoint(physical);
}

bool MeasurementOverlayStrategy::SourcePlaneMatches(const MeasurementEntity& entity) const
{
    const auto cursor = m_adapter->GetCursorWorld();
    const Point3 currentPhysical = m_adapter->GetModelPoint({
        cursor[0], cursor[1], cursor[2]
    });
    return std::abs(geometry::Dot(
        entity.plane.normal,
        geometry::Subtract(currentPhysical, entity.plane.origin))) <= entity.plane.sliceTolerance;
}

void MeasurementOverlayStrategy::AddPath(
    const std::vector<Point3>& worldPath,
    bool dashed,
    bool draft)
{
    if (!m_renderer || worldPath.size() < 2) {
        return;
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto lines = vtkSmartPointer<vtkCellArray>::New();
    if (!dashed) {
        for (const auto& point : worldPath) {
            points->InsertNextPoint(point.data());
        }
        lines->InsertNextCell(static_cast<int>(worldPath.size()));
        for (vtkIdType i = 0; i < static_cast<vtkIdType>(worldPath.size()); ++i) {
            lines->InsertCellPoint(i);
        }
    }
    else {
        constexpr int piecesPerSegment = 6;
        for (std::size_t segment = 1; segment < worldPath.size(); ++segment) {
            const auto& a = worldPath[segment - 1];
            const auto& b = worldPath[segment];
            for (int piece = 0; piece < piecesPerSegment; piece += 2) {
                const double t0 = static_cast<double>(piece) / piecesPerSegment;
                const double t1 = static_cast<double>(piece + 1) / piecesPerSegment;
                const Point3 p0 = geometry::Add(a, geometry::Scale(geometry::Subtract(b, a), t0));
                const Point3 p1 = geometry::Add(a, geometry::Scale(geometry::Subtract(b, a), t1));
                const vtkIdType id0 = points->InsertNextPoint(p0.data());
                const vtkIdType id1 = points->InsertNextPoint(p1.data());
                vtkIdType ids[2] = { id0, id1 };
                lines->InsertNextCell(2, ids);
            }
        }
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(lines);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetLineWidth(draft ? 2.5 : 2.0);
    if (m_highlight && !draft) {
        actor->GetProperty()->SetColor(1.0, 0.3, 0.9);
        actor->GetProperty()->SetLineWidth(4.0);
    }
    else if (draft) {
        actor->GetProperty()->SetColor(1.0, 0.65, 0.1);
    }
    else if (dashed) {
        actor->GetProperty()->SetColor(0.3, 0.85, 1.0);
        actor->GetProperty()->SetOpacity(0.35);
    }
    else {
        actor->GetProperty()->SetColor(0.1, 1.0, 0.35);
    }
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void MeasurementOverlayStrategy::AddLabel(
    const MeasurementEntity& entity,
    const std::vector<Point3>& worldPath)
{
    if (!m_renderer || worldPath.empty()) {
        return;
    }
    auto label = vtkSmartPointer<vtkBillboardTextActor3D>::New();
    std::string text = FormatMeasurementLabel(/*entity.id, */entity.result);
    if (!m_is3D) {
        const auto plane = m_adapter->GetPlane();
        const bool outside = std::any_of(entity.physicalPoints.begin(), entity.physicalPoints.end(), [&plane](const auto& point) {
            return std::abs(geometry::Dot(plane.normal, geometry::Subtract(point, plane.origin))) > plane.sliceTolerance;
        });
    }
    label->SetInput(text.c_str());
    const auto& labelPosition = worldPath[worldPath.size() / 2];
    label->SetPosition(labelPosition[0], labelPosition[1], labelPosition[2]);
    label->PickableOff();
    label->GetTextProperty()->SetFontSize(15);
    label->GetTextProperty()->SetColor(1.0, 1.0, 0.2);
    label->GetTextProperty()->SetBackgroundColor(0.05, 0.05, 0.05);
    label->GetTextProperty()->SetBackgroundOpacity(0.65);
    m_renderer->AddActor(label);
    m_props.push_back(label);
}

void MeasurementOverlayStrategy::AddControlPoints(
    const std::vector<Point3>& worldPoints,
    bool draft)
{
    if (!m_renderer || worldPoints.empty()) {
        return;
    }

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto vertices = vtkSmartPointer<vtkCellArray>::New();
    for (const auto& point : worldPoints) {
        const vtkIdType id = points->InsertNextPoint(point.data());
        vertices->InsertNextCell(1, &id);
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetVerts(vertices);
    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->PickableOff();
    actor->GetProperty()->LightingOff();
    actor->GetProperty()->SetPointSize(draft ? 9.0 : 7.0);
    actor->GetProperty()->RenderPointsAsSpheresOn();
    actor->GetProperty()->SetColor(
        draft ? 1.0 : 0.1,
        draft ? 0.65 : 1.0,
        draft ? 0.1 : 0.35);
    m_renderer->AddActor(actor);
    m_props.push_back(actor);
}

void MeasurementOverlayStrategy::RemoveProps()
{
    if (m_renderer) {
        for (const auto& prop : m_props) {
            m_renderer->RemoveViewProp(prop);
        }
    }
    m_props.clear();
}

// Split every segment at the two slice boundaries before projecting it.
void MeasurementOverlayStrategy::AddSlicePath(const std::vector<Point3>& path, double tolerance)
{
    const auto plane = m_adapter->GetPlane();
    std::vector<Point3> run;
    bool runOutside = false;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const auto& a = path[i - 1];
        const auto& b = path[i];
        const double da = geometry::Dot(plane.normal, geometry::Subtract(a, plane.origin));
        const double db = geometry::Dot(plane.normal, geometry::Subtract(b, plane.origin));
        std::vector<double> cuts{0.0, 1.0};
        if (std::abs(db - da) > 1e-12) {
            for (double boundary : {-tolerance, tolerance}) {
                const double t = (boundary - da) / (db - da);
                if (t > 0.0 && t < 1.0) cuts.push_back(t);
            }
        }
        std::sort(cuts.begin(), cuts.end());
        for (std::size_t j = 1; j < cuts.size(); ++j) {
            const double lo = cuts[j - 1], hi = cuts[j];
            const bool outside = std::abs(da + (db - da) * (lo + hi) * 0.5) > tolerance;
            const auto p = geometry::Add(a, geometry::Scale(geometry::Subtract(b, a), lo));
            const auto q = geometry::Add(a, geometry::Scale(geometry::Subtract(b, a), hi));
            if (!run.empty() && outside != runOutside) {
                AddPath(ProjectToCurrentView(run), runOutside, false);
                run.clear();
            }
            if (run.empty()) run.push_back(p);
            run.push_back(q);
            runOutside = outside;
        }
    }
    if (run.size() >= 2) AddPath(ProjectToCurrentView(run), runOutside, false);
}

} // namespace measure

#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementSession.h"

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
#include <sstream>
#include <type_traits>
#include <variant>

namespace measure {
namespace {
constexpr double kPi = 3.14159265358979323846;

std::string LabelText(const MeasurementEntity& entity)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream << "M" << entity.id << " ";
    std::visit([&stream](const auto& result) {
        using Result = std::decay_t<decltype(result)>;
        if constexpr (std::is_same_v<Result, LineResult>) {
            stream << "L=" << result.length;
        }
        else if constexpr (std::is_same_v<Result, CircleResult>) {
            stream << "D=" << result.diameter << " R=" << result.radius;
        }
        else {
            stream << "Arc=" << result.length
                << " R=" << result.radius
                << " A=" << std::abs(result.sweepRadians) * 180.0 / kPi << "deg";
        }
    }, entity.result);
    return stream.str();
}
}

MeasurementOverlayStrategy::MeasurementOverlayStrategy(
    const std::shared_ptr<MeasurementSession>& session,
    AbstractInteractiveService* service,
    MeasureView view)
    : m_session(session)
    , m_service(service)
    , m_view(view)
{
}

void MeasurementOverlayStrategy::SetInputData(vtkSmartPointer<vtkDataObject> data)
{
    (void)data;
}

void MeasurementOverlayStrategy::SetRendererAttached(vtkSmartPointer<vtkRenderer> renderer)
{
    if (m_renderer == renderer) {
        Refresh();
        return;
    }
    RemoveProps();
    m_renderer = renderer;
    Refresh();
}

void MeasurementOverlayStrategy::SetRendererDetached(vtkSmartPointer<vtkRenderer> renderer)
{
    if (m_renderer == renderer) {
        RemoveProps();
        m_renderer = nullptr;
    }
}

void MeasurementOverlayStrategy::SetVisualState(
    const RenderParams& params,
    UpdateFlags flags)
{
    (void)params;
    (void)flags;
    Refresh();
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
    if (!m_session || !m_service) {
        return;
    }

    for (const auto& entity : m_session->Entities()) {
        if (entity.sourceView != m_view || !SourcePlaneMatches(entity)) {
            continue;
        }
        const auto worldPath = ProjectToCurrentView(EntityPath(entity));
        if (worldPath.size() < 2) {
            continue;
        }
        AddPath(worldPath, false, false);
        AddControlPoints(ProjectToCurrentView(entity.physicalPoints), false);
        AddLabel(entity, worldPath);
    }

    if (const auto& draft = m_session->Draft();
        draft && draft->request.view == m_view) {
        const auto worldPath = ProjectToCurrentView(DraftPath(*draft));
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

std::vector<Point3> MeasurementOverlayStrategy::EntityPath(const MeasurementEntity& entity) const
{
    if (entity.type == MeasureTool::Line) {
        return entity.physicalPoints;
    }

    std::vector<Point3> path;
    if (entity.type == MeasureTool::Circle3Point) {
        const auto* circle = std::get_if<CircleResult>(&entity.result);
        if (!circle) return path;
        constexpr int samples = 128;
        path.reserve(samples + 1);
        for (int i = 0; i <= samples; ++i) {
            path.push_back(geometry::CirclePoint(
                circle->center,
                entity.plane,
                circle->radius,
                2.0 * kPi * static_cast<double>(i) / samples));
        }
        return path;
    }

    const auto* arc = std::get_if<ArcResult>(&entity.result);
    if (!arc || entity.physicalPoints.empty()) return path;
    const Point3 relative = geometry::Subtract(entity.physicalPoints.front(), arc->center);
    const double start = std::atan2(
        geometry::Dot(relative, entity.plane.v),
        geometry::Dot(relative, entity.plane.u));
    constexpr int samples = 64;
    path.reserve(samples + 1);
    for (int i = 0; i <= samples; ++i) {
        const double angle = start + arc->sweepRadians * static_cast<double>(i) / samples;
        path.push_back(geometry::CirclePoint(arc->center, entity.plane, arc->radius, angle));
    }
    return path;
}

std::vector<Point3> MeasurementOverlayStrategy::DraftPath(const MeasurementDraft& draft) const
{
    std::vector<Point3> candidate = draft.physicalPoints;
    if (draft.previewPoint) {
        candidate.push_back(*draft.previewPoint);
    }
    if (candidate.size() < 2 || !draft.plane) {
        return candidate;
    }

    if (draft.request.tool == MeasureTool::Circle3Point && candidate.size() == 3) {
        if (const auto circle = geometry::ComputeCircle(candidate, *draft.plane)) {
            std::vector<Point3> path;
            constexpr int samples = 128;
            path.reserve(samples + 1);
            for (int i = 0; i <= samples; ++i) {
                path.push_back(geometry::CirclePoint(
                    circle->center,
                    *draft.plane,
                    circle->radius,
                    2.0 * kPi * static_cast<double>(i) / samples));
            }
            return path;
        }
    }
    if (draft.request.tool == MeasureTool::Arc3Point && candidate.size() == 3) {
        if (const auto arc = geometry::ComputeArc(candidate, *draft.plane)) {
            const Point3 relative = geometry::Subtract(candidate.front(), arc->center);
            const double start = std::atan2(
                geometry::Dot(relative, draft.plane->v),
                geometry::Dot(relative, draft.plane->u));
            std::vector<Point3> path;
            constexpr int samples = 64;
            path.reserve(samples + 1);
            for (int i = 0; i <= samples; ++i) {
                const double angle = start + arc->sweepRadians * static_cast<double>(i) / samples;
                path.push_back(geometry::CirclePoint(arc->center, *draft.plane, arc->radius, angle));
            }
            return path;
        }
    }
    return candidate;
}

std::vector<Point3> MeasurementOverlayStrategy::ProjectToCurrentView(
    const std::vector<Point3>& physicalPath) const
{
    std::vector<Point3> projected;
    projected.reserve(physicalPath.size());
    const Point3 normal = CurrentWorldNormal();
    const auto cursor = m_service->GetCursorWorld();
    const Point3 origin{ cursor[0], cursor[1], cursor[2] };
    constexpr double safeOffset = 0.01;
    for (const auto& physical : physicalPath) {
        Point3 world = PhysicalToWorld(physical);
        const double distance = geometry::Dot(normal, geometry::Subtract(world, origin));
        world = geometry::Subtract(world, geometry::Scale(normal, distance));
        world = geometry::Add(world, geometry::Scale(normal, safeOffset));
        projected.push_back(world);
    }
    return projected;
}

Point3 MeasurementOverlayStrategy::PhysicalToWorld(const Point3& physical) const
{
    double source[3] = { physical[0], physical[1], physical[2] };
    double target[3] = { 0.0, 0.0, 0.0 };
    m_service->GetWorldPositionFromModel(source, target);
    return { target[0], target[1], target[2] };
}

bool MeasurementOverlayStrategy::SourcePlaneMatches(const MeasurementEntity& entity) const
{
    const auto cursor = m_service->GetCursorWorld();
    double world[3] = { cursor[0], cursor[1], cursor[2] };
    double physical[3] = { 0.0, 0.0, 0.0 };
    m_service->GetModelPositionFromWorld(world, physical);
    const Point3 currentPhysical{ physical[0], physical[1], physical[2] };
    return std::abs(geometry::Dot(
        entity.plane.normal,
        geometry::Subtract(currentPhysical, entity.plane.origin))) <= entity.plane.sliceTolerance;
}

Point3 MeasurementOverlayStrategy::CurrentWorldNormal() const
{
    switch (m_view) {
    case MeasureView::Axial: return { 0.0, 0.0, 1.0 };
    case MeasureView::Coronal: return { 0.0, 1.0, 0.0 };
    case MeasureView::Sagittal: return { 1.0, 0.0, 0.0 };
    }
    return { 0.0, 0.0, 1.0 };
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
    if (draft) {
        actor->GetProperty()->SetColor(1.0, 0.65, 0.1);
    }
    else if (dashed) {
        actor->GetProperty()->SetColor(0.3, 0.85, 1.0);
        actor->GetProperty()->SetOpacity(0.8);
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
    label->SetInput(LabelText(entity).c_str());
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

} // namespace measure

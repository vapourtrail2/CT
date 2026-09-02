#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementView.h"
#include "measure/MeasureViewAdapter.h"

#include <vtkImageData.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

namespace measure {

MeasurementInteractionHandler::MeasurementInteractionHandler(
    const std::shared_ptr<MeasurementSession>& session,
    MeasureViewAdapter* adapter,
    vtkRenderer* renderer,
    MeasureView view)
    : m_session(session)
    , m_adapter(adapter)
    , m_renderer(renderer)
    , m_view(view)
{
}

InteractionResult MeasurementInteractionHandler::Send(const InteractionEvent& event)
{   
    if (!m_session || !m_session->IsActive() || !m_adapter || !m_renderer) {
        if (event.eventKind == InteractionEventKind::PrimaryRelease
            && m_consumingLeftButton) {
            m_consumingLeftButton = false;
            return { true, true };
        }
        if (event.eventKind == InteractionEventKind::PrimaryPress
            || event.eventKind == InteractionEventKind::PrimaryRelease
            || event.eventKind == InteractionEventKind::WheelForward
            || event.eventKind == InteractionEventKind::WheelBackward) {
            return { true, true };
        }
        return {};
    }

    const auto& draft = m_session->Draft();
    if (!draft || draft->request.view != m_view) {
        return {};
    }

    if (event.eventKind == InteractionEventKind::PrimaryPress) {
        m_consumingLeftButton = true;

		const auto physical = GetDisplayPoint(event.x, event.y);//鼠标点击位置转为物理坐标

        if (physical) {
            std::string error;
            m_session->AppendPoint(*physical, GetMeasurePlane(), &error);//记录当前二维切片所在的物理平面
        }   
        else {
            m_session->SetStatusMessage("点击位置不在图像有效范围内，请在图像上选择点。");
        }
        return { true, true };
    }

    if (event.eventKind == InteractionEventKind::PrimaryRelease && m_consumingLeftButton) {
        m_consumingLeftButton = false;
        return { true, true };
    }

    if (event.eventKind == InteractionEventKind::PointerMove) {
        if (!draft->physicalPoints.empty()) {
            const auto preview = GetDisplayPoint(event.x, event.y);
            m_session->UpdatePreview(preview);
            return { true, true };
        }
        return {};
    }

    if ((event.eventKind == InteractionEventKind::WheelForward|| event.eventKind == InteractionEventKind::WheelBackward)
        && !draft->physicalPoints.empty()) {
        return { true, true };
    }

    return {};
}

std::optional<Point3> MeasurementInteractionHandler::GetDisplayPoint(
    int x,
    int y) const
{
    if (!m_renderer || !m_adapter) {
        return std::nullopt;
    }

    const auto point = m_adapter->GetDisplayModel(x, y);
    return point && GetIsInsideImage(*point)
        ? point
        : std::nullopt;
}

MeasurementPlane MeasurementInteractionHandler::GetMeasurePlane() const
{
    const auto cursor = m_adapter->GetCursorWorld();
    const auto& view = GetSliceViewDescriptor(m_view);

    MeasurementPlane plane;
    plane.origin = m_adapter->GetModelPoint({
        cursor[0], cursor[1], cursor[2]
    });
    plane.u = view.u;
    plane.v = view.v;
    plane.normal = view.normal;

    if (const auto image = m_adapter->GetImageSnapshot()) {
        const auto spacing = image->spacing;
        const double minimumSpacing = std::min({
            std::abs(spacing[0]), std::abs(spacing[1]), std::abs(spacing[2]) });
        plane.tolerance = std::max(1e-9, minimumSpacing * 1e-6);
        plane.sliceTolerance = std::max(1e-6, minimumSpacing * 0.51);
    }
    return plane;
}

bool MeasurementInteractionHandler::GetIsInsideImage(
    const Point3& physicalPoint) const
{
    if (!m_adapter) {
        return false;
    }
    const auto snapshot = m_adapter->GetImageSnapshot();
    if (!snapshot || !snapshot->image) {
        return false;
    }
    const auto image = snapshot->image;

    double source[3] = { physicalPoint[0], physicalPoint[1], physicalPoint[2] };
    double continuousIndex[3] = { 0.0, 0.0, 0.0 };
    image->TransformPhysicalPointToContinuousIndex(source, continuousIndex);
    int extent[6];
    image->GetExtent(extent);
    constexpr double margin = 0.5;
    for (int axis = 0; axis < 3; ++axis) {
        if (continuousIndex[axis] < extent[2 * axis] - margin
            || continuousIndex[axis] > extent[2 * axis + 1] + margin) {
            return false;
        }
    }
    return true;
}

} // namespace measure

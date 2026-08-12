#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementView.h"

#include "App/AppInterfaces.h"
#include <vtkCommand.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>

namespace measure {

MeasurementInteractionHandler::MeasurementInteractionHandler(
    const std::shared_ptr<MeasurementSession>& session,
    const std::shared_ptr<AbstractDataManager>& dataManager,
    InteractiveService* service,
    vtkRenderer* renderer,
    MeasureView view)
    : m_session(session)
    , m_dataManager(dataManager)
    , m_service(service)
    , m_renderer(renderer)
    , m_view(view)
{
}

InteractionResult MeasurementInteractionHandler::Send(const InteractionEvent& event)
{
    if (!m_session || !m_session->IsActive() || !m_service || !m_renderer) {
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

		const auto physical = DisplayToPhysical(event.x, event.y);//鼠标点击位置转为物理坐标

        if (physical) {
            std::string error;
            m_session->AppendPoint(*physical, SnapshotPhysicalPlane(), &error);//SnapshotPhysicalPlane 当前二维切片所在的物理平面
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
            const auto preview = DisplayToPhysical(event.x, event.y);
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

std::optional<Point3> MeasurementInteractionHandler::DisplayToPhysical(int x, int y) const
{
    if (!m_renderer || !m_service) {
        return std::nullopt;
    }

    auto displayToWorld = [this, x, y](double z) -> std::optional<Point3> {
        m_renderer->SetDisplayPoint(static_cast<double>(x), static_cast<double>(y), z);
        m_renderer->DisplayToWorld();
        double* worldpoint = m_renderer->GetWorldPoint();
        if (!worldpoint || std::abs(worldpoint[3]) <= 1e-12) {
            return std::nullopt;
        }
        return Point3{
            worldpoint[0] / worldpoint[3],
            worldpoint[1] / worldpoint[3],
            worldpoint[2] / worldpoint[3]
        };
    };

    const auto nearPoint = displayToWorld(0.0);//近 远裁剪面 相机能显示的最近和最远边界 别的没必要显示
    const auto farPoint = displayToWorld(1.0);// 计算射线用的
    if (!nearPoint || !farPoint) {
        return std::nullopt;
    }

    const Point3 direction = geometry::Subtract(*farPoint, *nearPoint);//表示鼠标射线从近裁剪面指向远裁剪面的方向

    const Point3 normal = GetSliceViewDescriptor(m_view).normal;
    /*   normal = { 0, 0, 1 }; //法向量朝Z xy平面
    u = { 1, 0, 0 };
    v = { 0, 1, 0 };*/

    const auto cursor = m_service->GetCursorWorld();
    const Point3 planeOrigin{ cursor[0], cursor[1], cursor[2] };
    
    //判断射线是否平行于平面  点积= 0 射线和平面平行 不相交
    //计算射线在“垂直切片方向”上的移动量
    const double denominator = geometry::Dot(normal, direction);
    if (std::abs(denominator) <= 1e-12) {
        return std::nullopt;
    }

    //计算从 nearPoint 出发，沿 direction 走多少，才能到达当前切片平面
    const Point3 v1 =
        geometry::Subtract(planeOrigin, *nearPoint);

    const double v2 =   
        geometry::Dot(normal, v1); // 射线到达切片所需要的法线方向移动量

    const double v3 = 
        geometry::Dot(normal, direction);   //射线参数每前进 1，能在法线方向前进多少 

    const double v4 =
        v2 / v3;//射线需要前进的倍率

    const Point3 worldPoint = geometry::Add(*nearPoint, geometry::Scale(direction, v4));

    double world[3] = { worldPoint[0], worldPoint[1], worldPoint[2] };
    double physical[3] = { 0.0, 0.0, 0.0 }; 

    m_service->GetModelPositionFromWorld(world, physical);
    const Point3 result{ physical[0], physical[1], physical[2] };
    return IsInsideImage(result) ? std::optional<Point3>(result) : std::nullopt;
}

MeasurementPlane MeasurementInteractionHandler::SnapshotPhysicalPlane() const
{
    const auto cursor = m_service->GetCursorWorld();
    Point3 worldOrigin{ cursor[0], cursor[1], cursor[2] };
    const auto& view = GetSliceViewDescriptor(m_view);
    const Point3 worldU = view.u;
    const Point3 worldV = view.v;

    auto toPhysical = [this](const Point3& world) {
        double source[3] = { world[0], world[1], world[2] };
        double target[3] = { 0.0, 0.0, 0.0 };
        m_service->GetModelPositionFromWorld(source, target);
        return Point3{ target[0], target[1], target[2] };
    };

    MeasurementPlane plane;
    plane.origin = toPhysical(worldOrigin);
    const Point3 physicalUPoint = toPhysical(geometry::Add(worldOrigin, worldU));
    const Point3 physicalVPoint = toPhysical(geometry::Add(worldOrigin, worldV));
    const auto normalizedU = geometry::Normalized(geometry::Subtract(physicalUPoint, plane.origin));
    Point3 rawV = geometry::Subtract(physicalVPoint, plane.origin);
    if (normalizedU) {
        rawV = geometry::Subtract(rawV, geometry::Scale(*normalizedU, geometry::Dot(rawV, *normalizedU)));
    }
    const auto normalizedV = geometry::Normalized(rawV);
    if (normalizedU) plane.u = *normalizedU;
    if (normalizedV) plane.v = *normalizedV;
    if (const auto normal = geometry::Normalized(geometry::Cross(plane.u, plane.v))) {
        plane.normal = *normal;
        if (const auto correctedV = geometry::Normalized(geometry::Cross(plane.normal, plane.u))) {
            plane.v = *correctedV;
        }
    }

    if (m_dataManager) {
        const auto spacing = m_dataManager->GetSpacing();
        const double minimumSpacing = std::min({
            std::abs(spacing[0]), std::abs(spacing[1]), std::abs(spacing[2]) });
        plane.tolerance = std::max(1e-9, minimumSpacing * 1e-6);
        plane.sliceTolerance = std::max(1e-6, minimumSpacing * 0.51);
    }
    return plane;
}

bool MeasurementInteractionHandler::IsInsideImage(const Point3& physicalPoint) const
{
    if (!m_dataManager) {
        return false;
    }
    const auto snapshot = m_dataManager->GetImageSnapshot();
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

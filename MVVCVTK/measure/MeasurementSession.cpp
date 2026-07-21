#include "measure/MeasurementSession.h"
#include "measure/MeasurementGeometry.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace measure {

void MeasurementSession::SetChangedCallback(ChangedCallback callback)
{
    m_changed = std::move(callback);
}

void MeasurementSession::Begin(const MeasureRequest& request)
{
    m_lastRequest = request;
    m_draft = MeasurementDraft{ request, std::nullopt, {}, std::nullopt };
    m_statusMessage = request.tool == MeasureTool::Line
        ? "直线：点击起点和终点，或者按住左键拖动。"
        : (request.tool == MeasureTool::Circle3Point
            ? "圆：依次点击圆周上的三个点。"
            : "圆弧：依次点击起点、经过点和终点。");
    NotifyChanged();
}

bool MeasurementSession::AppendPoint(
    const Point3& physicalPoint,
    const MeasurementPlane& plane,
    std::string* error)
{
    if (!m_draft) {
        if (error) *error = "No active measurement.";
        return false;
    }

    if (!m_draft->plane) {
        m_draft->plane = plane;
    }

    std::vector<Point3> candidate = m_draft->physicalPoints;
    candidate.push_back(physicalPoint);
    const int required = RequiredPointCount(m_draft->request.tool);
    if (static_cast<int>(candidate.size()) < required) {
        m_draft->physicalPoints = std::move(candidate);
        m_draft->previewPoint.reset();
        std::ostringstream message;
        message << "已选择第 " << m_draft->physicalPoints.size()
            << " 个点，还需要 "
            << (required - static_cast<int>(m_draft->physicalPoints.size()))
            << " 个点。";
        m_statusMessage = message.str();
        NotifyChanged();
        return true;
    }

    MeasurementResult result = LineResult{};
    bool valid = false;
    switch (m_draft->request.tool) {
    case MeasureTool::Line:
        if (const auto value = geometry::ComputeLine(candidate)) {
            result = *value;
            valid = true;
        }
        break;
    case MeasureTool::Circle3Point:
        if (const auto value = geometry::ComputeCircle(candidate, *m_draft->plane)) {
            result = *value;
            valid = true;
        }
        break;
    case MeasureTool::Arc3Point:
        if (const auto value = geometry::ComputeArc(candidate, *m_draft->plane)) {
            result = *value;
            valid = true;
        }
        break;
    default:
        break;
    }

    if (!valid) {
        if (error) {
            *error = m_draft->request.tool == MeasureTool::Line
                ? "The two points are too close."
                : "The three points are collinear or too close.";
        }
        m_statusMessage = m_draft->request.tool == MeasureTool::Line
            ? "两个点太近，请重新选择终点。"
            : "三个点过近或接近共线，请重新选择最后一个点。";
        NotifyChanged();
        return false;
    }

    MeasurementEntity entity;
    entity.id = m_nextId++;
    entity.type = m_draft->request.tool;
    entity.sourceView = m_draft->request.view;
    entity.plane = *m_draft->plane;
    entity.physicalPoints = std::move(candidate);
    entity.result = std::move(result);
    m_entities.push_back(std::move(entity));

    const auto& completed = m_entities.back();
    std::ostringstream message;
    message << std::fixed << std::setprecision(3);
    if (const auto* line = std::get_if<LineResult>(&completed.result)) {
        message << "完成：长度 = " << line->length << "（数据物理单位）";
    }
    else if (const auto* circle = std::get_if<CircleResult>(&completed.result)) {
        message << "完成：直径 = " << circle->diameter
            << "，半径 = " << circle->radius << "（数据物理单位）";
    }
    else if (const auto* arc = std::get_if<ArcResult>(&completed.result)) {
        constexpr double radiansToDegrees = 57.29577951308232;
        message << "完成：弧长 = " << arc->length
            << "，半径 = " << arc->radius
            << "，圆心角 = " << std::abs(arc->sweepRadians) * radiansToDegrees << "°";
    }
    m_statusMessage = message.str();

    m_draft.reset();
    NotifyChanged();
    return true;
}

void MeasurementSession::UpdatePreview(const std::optional<Point3>& physicalPoint)
{
    if (!m_draft || m_draft->physicalPoints.empty()) {
        return;
    }
    m_draft->previewPoint = physicalPoint;
    NotifyChanged();
}

void MeasurementSession::UndoDraftPoint()
{
    if (!m_draft || m_draft->physicalPoints.empty()) {
        return;
    }
    m_draft->physicalPoints.pop_back();
    m_draft->previewPoint.reset();
    if (m_draft->physicalPoints.empty()) {
        m_draft->plane.reset();
    }
    NotifyChanged();
}

void MeasurementSession::CancelDraft()
{
    if (!m_draft) {
        return;
    }
    m_draft.reset();
    m_statusMessage = "已取消当前测量。";
    NotifyChanged();
}

void MeasurementSession::SetStatusMessage(std::string message)
{
    m_statusMessage = std::move(message);
    NotifyChanged();
}

bool MeasurementSession::Remove(std::uint64_t id)
{
    const auto oldSize = m_entities.size();
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [id](const MeasurementEntity& entity) { return entity.id == id; }),
        m_entities.end());
    if (m_entities.size() == oldSize) {
        return false;
    }
    NotifyChanged();
    return true;
}

void MeasurementSession::ClearView(MeasureView view)
{
    const auto oldSize = m_entities.size();
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [view](const MeasurementEntity& entity) { return entity.sourceView == view; }),
        m_entities.end());
    const bool removedDraft = m_draft && m_draft->request.view == view;
    if (removedDraft) {
        m_draft.reset();
    }
    if (oldSize != m_entities.size() || removedDraft) {
        NotifyChanged();
    }
}

bool MeasurementSession::IsActive() const
{
    return m_draft.has_value();
}

const std::optional<MeasurementDraft>& MeasurementSession::Draft() const
{
    return m_draft;
}

const std::vector<MeasurementEntity>& MeasurementSession::Entities() const
{
    return m_entities;
}

const MeasureRequest& MeasurementSession::LastRequest() const
{
    return m_lastRequest;
}

const std::string& MeasurementSession::StatusMessage() const
{
    return m_statusMessage;
}

void MeasurementSession::NotifyChanged()
{
    if (m_changed) {
        m_changed();
    }
}

int MeasurementSession::RequiredPointCount(MeasureTool tool) const
{
    return tool == MeasureTool::Line ? 2 : 3;
}

} // namespace measure

#include "measure/MeasurementSession.h"
#include "measure/MeasurementToolDefinition.h"

#include <algorithm>
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
    m_statusMessage = GetMeasurementToolDefinition(request.tool).beginMessage;
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
    const auto& definition = GetMeasurementToolDefinition(m_draft->request.tool);
    const int required = definition.requiredPointCount;
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

    const auto result = definition.compute
        ? definition.compute(candidate, *m_draft->plane)
        : std::nullopt;

    if (!result) {
        if (error) {
            *error = definition.invalidError;
        }
        m_statusMessage = definition.invalidStatus;
        NotifyChanged();
        return false;
    }

    MeasurementEntity entity;
    entity.id = m_nextId++;
    entity.type = m_draft->request.tool;
    entity.sourceView = m_draft->request.view;
    entity.plane = *m_draft->plane;
    entity.physicalPoints = std::move(candidate);
    entity.result = *result;
    m_entities.push_back(std::move(entity));

    m_statusMessage = FormatCompletedMeasurement(m_entities.back().result);

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

} // namespace measure

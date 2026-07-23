#include <iostream>
#include <QDebug>
#include "measure/MeasurementSession.h"
#include "measure/MeasurementToolDefinition.h"


#include <algorithm>
#include <sstream>

namespace measure {
namespace {
constexpr std::size_t kMaxHistorySize = 100;
}

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
    /*for (size_t i = 0; i < candidate.size(); i++)
    {
        Point3& point = candidate[i];

        std::cout << point[0] << "," << point[1] << "," << point[2] << std::endl;
    }*/
    candidate.push_back(physicalPoint);//放在候选者里 ，而不是立即写回草稿 是因为最后一个点加入后还要验证结果是否有效
    const auto& definition = GetMeasurementToolDefinition(m_draft->request.tool);//返回线工具的配置
	const int required = definition.requiredPointCount;//需要的点数 线是2点
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
    entity.where2DViewer = m_draft->request.view;
    entity.plane = *m_draft->plane;
    entity.physicalPoints = std::move(candidate);
    entity.result = *result;
    RecordHistory();
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

bool MeasurementSession::Undo()
{
    if (m_draft && !m_draft->physicalPoints.empty()) {
        m_draft->physicalPoints.pop_back();
        m_draft->previewPoint.reset();
        if (m_draft->physicalPoints.empty()) {
            m_draft->plane.reset();
        }
        m_statusMessage = "已撤销最后一个取点。";
        NotifyChanged();
        return true;
    }

    if (m_undoHistory.empty()) {
        return false;
    }

    m_draft.reset();
    m_redoHistory.push_back(m_entities);
    m_entities = std::move(m_undoHistory.back());
    m_undoHistory.pop_back();
    m_statusMessage = "已撤销上一次测量。";
    NotifyChanged();
    return true;
}

bool MeasurementSession::Redo()
{
    if (m_draft || m_redoHistory.empty()) {
        return false;
    }

    m_undoHistory.push_back(m_entities);
    m_entities = std::move(m_redoHistory.back());
    m_redoHistory.pop_back();
    m_statusMessage = "已重做上一次测量。";
    NotifyChanged();
    return true;
}

bool MeasurementSession::CanUndo() const
{
    return (m_draft && !m_draft->physicalPoints.empty())
        || !m_undoHistory.empty();
}

bool MeasurementSession::CanRedo() const
{
    return !m_draft && !m_redoHistory.empty();
}

bool MeasurementSession::Remove(std::uint64_t id)
{
    const auto found = std::find_if(
        m_entities.begin(), m_entities.end(),
        [id](const MeasurementEntity& entity) { return entity.id == id; });
    if (found == m_entities.end()) {
        return false;
    }

    RecordHistory();
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [id](const MeasurementEntity& entity) { return entity.id == id; }),
        m_entities.end());
    NotifyChanged();
    return true;
}

void MeasurementSession::ClearView(MeasureView view)
{
    const bool removesEntities = std::any_of(
        m_entities.begin(), m_entities.end(),
        [view](const MeasurementEntity& entity) { return entity.where2DViewer == view; });
    if (removesEntities) {
        RecordHistory();
    }
    m_entities.erase(
        std::remove_if(m_entities.begin(), m_entities.end(),
            [view](const MeasurementEntity& entity) { return entity.where2DViewer == view; }),
        m_entities.end());
    const bool removedDraft = m_draft && m_draft->request.view == view;
    if (removedDraft) {
        m_draft.reset();
    }
    if (removesEntities || removedDraft) {
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

void MeasurementSession::RecordHistory()
{
    if (m_undoHistory.size() >= kMaxHistorySize) {
        m_undoHistory.erase(m_undoHistory.begin());
    }
    m_undoHistory.push_back(m_entities);
    m_redoHistory.clear();
}

void MeasurementSession::NotifyChanged()
{
    if (m_changed) {
        m_changed();
    }
}

} 

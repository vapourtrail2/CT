#pragma once

#include "measure/MeasurementTypes.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace measure {

class MeasurementSession {
public:
    enum class Change { Draft, Objects, Selection };
    using ObserverId = std::uint64_t;
    ObserverId Subscribe(std::function<void(Change)> callback);
    void Unsubscribe(ObserverId id);
    std::uint64_t Revision() const { return m_revision; }
    bool SetVisible(std::uint64_t id, bool visible);
    bool Rename(std::uint64_t id, std::string name);
    void Select(std::uint64_t id);
    std::uint64_t SelectedId() const { return m_selectedId; }
    bool CommitCaptured(MeasurementEntity entity, std::string* error = nullptr);
    static bool MatchesPlane(const MeasurementPlane& source, const MeasurementPlane& current);

    void SetChangedCallback(std::function<void()> callback);

    void Begin(const MeasureRequest& request);
    bool AppendPoint(
        const Point3& physicalPoint,
        const MeasurementPlane& plane,
        std::string* error = nullptr);
    void UpdatePreview(const std::optional<Point3>& physicalPoint);
    void UndoDraftPoint();
    void CancelDraft();
    void SetStatusMessage(std::string message);
    bool Undo();
    bool Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    bool Remove(std::uint64_t id);
    void ClearView(MeasureView view);

    bool IsActive() const;
    const std::optional<MeasurementDraft>& Draft() const;
    const std::vector<MeasurementEntity>& Entities() const;
    const MeasureRequest& LastRequest() const;
    const std::string& StatusMessage() const;

private:
    using EntitySnapshot = std::vector<MeasurementEntity>;

    void RecordHistory();
    void NotifyChanged(Change change = Change::Draft);

    std::function<void()> m_callback;
    MeasureRequest m_lastRequest;
    std::optional<MeasurementDraft> m_draft;
    std::vector<MeasurementEntity> m_entities;
    std::vector<EntitySnapshot> m_undoHistory;
    std::vector<EntitySnapshot> m_redoHistory;
    std::string m_statusMessage;
    std::uint64_t m_nextId = 1;
    std::uint64_t m_revision = 0;
    std::uint64_t m_selectedId = 0;
    ObserverId m_nextObserverId = 1;
    std::map<ObserverId, std::function<void(Change)>> m_observers;
};

} // namespace measure

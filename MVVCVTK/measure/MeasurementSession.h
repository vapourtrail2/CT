#pragma once

#include "measure/MeasurementTypes.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace measure {

class MeasurementSession {
public:
    using ChangedCallback = std::function<void()>;

    void SetChangedCallback(ChangedCallback callback);

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
    void NotifyChanged();

    ChangedCallback m_changed;
    MeasureRequest m_lastRequest;
    std::optional<MeasurementDraft> m_draft;
    std::vector<MeasurementEntity> m_entities;
    std::vector<EntitySnapshot> m_undoHistory;
    std::vector<EntitySnapshot> m_redoHistory;
    std::string m_statusMessage;
    std::uint64_t m_nextId = 1;
};

} // namespace measure

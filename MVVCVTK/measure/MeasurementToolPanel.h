#pragma once
#include <QWidget>
#include "measure/MeasurementTypes.h"
#include "measure/EdgeCaptureController.h"
#include "measure/MeasurementSession.h"
#include <functional>
#include <memory>

class QLabel;
class QPushButton;

namespace measure {
// UI only: commands are supplied by the main window, data belongs to the volume.
class MeasurementToolPanel final : public QWidget {
public:
    explicit MeasurementToolPanel(QWidget* parent = nullptr);
    ~MeasurementToolPanel() override;
    void Bind(std::shared_ptr<MeasurementSession> session);
    void SetVolumeName(const QString& name);
    std::function<void(MeasureTool)> beginTool;
    std::function<void(EdgeCaptureShape)> beginCapture;
    std::function<void()> confirmCapture;
    std::function<void()> cancel;
    std::function<void()> requestRender;
private:
    void Refresh();
    std::shared_ptr<MeasurementSession> m_session;
    MeasurementSession::ObserverId m_subscription = 0;
    QLabel* m_volume = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_redo = nullptr;
};
}

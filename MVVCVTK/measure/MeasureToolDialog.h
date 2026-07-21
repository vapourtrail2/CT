#pragma once

#include "measure/MeasurementTypes.h"
#include "c_ui/viewport/Viewport.h"

#include <QDialog>
#include <memory>

class AbstractDataManager;
class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QVTKOpenGLNativeWidget;
class SharedInteractionState;
class SharedStateBroadcaster;

namespace measure {

class MeasurementOverlayStrategy;
class MeasurementSession;

class MeasureToolDialog : public QDialog {
public:
    MeasureToolDialog(
        const std::shared_ptr<AbstractDataManager>& dataManager,
        const std::shared_ptr<SharedInteractionState>& sourceState,
        QWidget* parent = nullptr);
    ~MeasureToolDialog() override;

private:
    void BuildUi();
    void BuildMeasurementViewport(
        const std::shared_ptr<SharedInteractionState>& sourceState);
    void PublishCurrentImage();
    void AttachOverlayAfterPipelineReady();
    void ConfigureMeasurementInteraction(MeasureView view);
    void SetView(MeasureView view);
    void BeginTool(MeasureTool tool);
    void ClearCheckedTool();

    std::shared_ptr<AbstractDataManager> m_dataManager;
    std::shared_ptr<SharedStateBroadcaster> m_localBroadcaster;
    std::shared_ptr<SharedInteractionState> m_localState;
    std::shared_ptr<MeasurementSession> m_session;
    std::shared_ptr<MeasurementOverlayStrategy> m_overlay;
    Viewport m_viewport;

    QVTKOpenGLNativeWidget* m_vtkWidget = nullptr;
    QComboBox* m_viewCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    QButtonGroup* m_toolGroup = nullptr;
    QPushButton* m_lineButton = nullptr;
    QPushButton* m_circleButton = nullptr;
    QPushButton* m_arcButton = nullptr;
    MeasureView m_currentView = MeasureView::Axial;
};

} // namespace measure

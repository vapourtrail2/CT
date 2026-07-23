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
    void UndoMeasurement();
    void RedoMeasurement();
    void UpdateHistoryButtons();
    void ClearCheckedTool();

    std::shared_ptr<AbstractDataManager> m_dataManager;//被测量的体数据
    std::shared_ptr<SharedStateBroadcaster> m_localBroadcaster;
	std::shared_ptr<SharedInteractionState> m_localState;//测量窗口的自己的状态 避免和主窗口的状态冲突
    std::shared_ptr<MeasurementSession> m_session;//测量什么 点了哪些点  测量结果 Redo Undo
	std::shared_ptr<MeasurementOverlayStrategy> m_overlay;//画线 和 文字之类的东西
    Viewport m_viewport;//显示二维图像 接收鼠标事件

    QVTKOpenGLNativeWidget* m_vtkWidget = nullptr;
    QComboBox* m_viewCombo = nullptr;
    QLabel* m_statusLabel = nullptr;
    QButtonGroup* m_toolGroup = nullptr;
    QPushButton* m_lineButton = nullptr;
    QPushButton* m_circleButton = nullptr;
    QPushButton* m_arcButton = nullptr;
    QPushButton* m_undoButton = nullptr;
    QPushButton* m_redoButton = nullptr;
    MeasureView m_currentView = MeasureView::Axial;
};

} // namespace measure

#pragma once
#include "measure/MeasurementTypes.h"
#include "measure/MeasurementViewInitState.h"
#include "measure/MeasurementViewport.h"

#include <QDialog>
#include <memory>

class QButtonGroup;
class QComboBox;
class QLabel;
class QPushButton;
class QVTKOpenGLNativeWidget;
class QTimer;

namespace measure {

class MeasurementSession;

class MeasureToolDialog : public QDialog {
public:
    MeasureToolDialog(
        const ImageSnapshot& imageSnapshot,
        const MeasurementViewInitState& initialState,
        QWidget* parent = nullptr);
    ~MeasureToolDialog() override;

private:
    void BuildUi();
    void BuildMeasurementViewport(  
        const ImageSnapshot& imageSnapshot,
        const MeasurementViewInitState& initialState);
    void SetView(MeasureView view);
    void BeginTool(MeasureTool tool);
    void UndoMeasurement();
    void RedoMeasurement();
    void UpdateHistoryButtons();
    void ClearCheckedTool();

    std::shared_ptr<MeasurementSession> m_session;//测量什么 点了哪些点  测量结果 Redo Undo
    MeasurementViewport m_viewport;

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
    QTimer* m_refreshTimer = nullptr;
};

} // namespace measure

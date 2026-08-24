#include "measure/MeasureToolDialog.h"
#include "measure/MeasurementSession.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>

namespace measure {
    MeasureToolDialog::MeasureToolDialog(
        const ImageSnapshot& imageSnapshot,
        const MeasurementViewInitState& initialState,
        QWidget* parent)
        : QDialog(parent)
    {
        BuildUi();
        BuildMeasurementViewport(
            imageSnapshot,
            initialState);
    }

    MeasureToolDialog::~MeasureToolDialog()
    {
        if (m_session) {
            m_session->SetChangedCallback({});
        }

        m_viewport.Reset();
        m_session.reset();
    }

void MeasureToolDialog::BuildUi()
{
    setWindowTitle(QStringLiteral("二维测量"));
    setModal(true);
    setWindowModality(Qt::ApplicationModal);
    resize(980, 760);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    m_vtkWidget = new QVTKOpenGLNativeWidget(this);
    m_vtkWidget->setMinimumSize(760, 560);
    m_vtkWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_vtkWidget, 1);

    auto* controls = new QHBoxLayout();
    controls->setSpacing(8);
    m_statusLabel = new QLabel(QStringLiteral("请选择线、圆或圆弧工具。"), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#e6d66a; padding-left:4px;"));   
    controls->addWidget(m_statusLabel, 1);

    auto* viewLabel = new QLabel(QStringLiteral("二维视图："), this);
    m_viewCombo = new QComboBox(this);
    m_viewCombo->addItem(QStringLiteral("轴向"), static_cast<int>(MeasureView::Axial));
    m_viewCombo->addItem(QStringLiteral("冠状"), static_cast<int>(MeasureView::Coronal));
    m_viewCombo->addItem(QStringLiteral("矢状"), static_cast<int>(MeasureView::Sagittal));
    m_viewCombo->setMinimumWidth(110);

    m_lineButton = new QPushButton(QStringLiteral("线"), this);
    m_circleButton = new QPushButton(QStringLiteral("圆"), this);
    m_arcButton = new QPushButton(QStringLiteral("圆弧"), this);
    m_undoButton = new QPushButton(QStringLiteral("撤销"), this);
    m_redoButton = new QPushButton(QStringLiteral("重做"), this);

    for (auto button : {
        m_lineButton,
        m_circleButton,
        m_arcButton,
        m_undoButton,
        m_redoButton }) 
    {
		button->setAutoDefault(false);//取消默认按钮 避免失去焦点后蓝框恢复到第一个按钮
    }

    for (auto* button : { m_lineButton, m_circleButton, m_arcButton }) {
        button->setCheckable(true);
        button->setMinimumSize(72, 32); 
    }
    for (auto* button : { m_undoButton, m_redoButton }) {
        button->setMinimumSize(72, 32);
        button->setEnabled(false);
    }
    m_undoButton->setToolTip(QStringLiteral("撤销最后一次测量或最后一个取点"));
    m_redoButton->setToolTip(QStringLiteral("重做最后一次撤销的测量"));

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);
    m_toolGroup->addButton(m_lineButton, static_cast<int>(MeasureTool::Line));
    m_toolGroup->addButton(m_circleButton, static_cast<int>(MeasureTool::Circle3Point));
    m_toolGroup->addButton(m_arcButton, static_cast<int>(MeasureTool::Arc3Point));

    controls->addWidget(viewLabel);
    controls->addWidget(m_viewCombo);
    controls->addSpacing(10);
    controls->addWidget(m_undoButton);
    controls->addWidget(m_redoButton);
    controls->addSpacing(10);
    controls->addWidget(m_lineButton);
    controls->addWidget(m_circleButton);
    controls->addWidget(m_arcButton);
    root->addLayout(controls);

    connect(m_viewCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (index < 0) return;
            SetView(static_cast<MeasureView>(m_viewCombo->itemData(index).toInt()));
        });
    connect(m_lineButton, &QPushButton::clicked, this,
        [this]() {
            BeginTool(MeasureTool::Line);
        });
    connect(m_circleButton, &QPushButton::clicked, this,
        [this]() { 
            BeginTool(MeasureTool::Circle3Point); 
        });
    connect(m_arcButton, &QPushButton::clicked, this,
        [this]() { 
            BeginTool(MeasureTool::Arc3Point);
        });
    connect(m_undoButton, &QPushButton::clicked, this,
        [this]() { 
            UndoMeasurement(); 
        });
    connect(m_redoButton, &QPushButton::clicked, this,
        [this]() { 
            RedoMeasurement(); 
        });

    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this,
        [this]() { 
            UndoMeasurement();
        });
    auto* redoShortcut = new QShortcut(QKeySequence::Redo, this);
    connect(redoShortcut, &QShortcut::activated, this,
        [this]() { 
            RedoMeasurement(); 
        });
}

void MeasureToolDialog::BuildMeasurementViewport(
    const ImageSnapshot& imageSnapshot,
    const MeasurementViewInitState& initialState)
{
    if (!m_vtkWidget
        || !imageSnapshot
        || !imageSnapshot->image) {
        return;
    }

    m_session = std::make_shared<MeasurementSession>();

    if (!m_viewport.Build(
        m_vtkWidget,
        imageSnapshot,
        m_session,
        m_currentView,
        initialState)) {

        m_session.reset();
        return;
    }

    m_session->SetChangedCallback(
        [this]() {
            if (m_statusLabel && m_session) {
                m_statusLabel->setText(
                    QString::fromUtf8(
                        m_session
                        ->StatusMessage()
                        .c_str()));
            }

            if (m_session && !m_session->IsActive()) {
                ClearCheckedTool();
            }

            UpdateHistoryButtons();
            m_viewport.Refresh();
        });

    UpdateHistoryButtons();
}

void MeasureToolDialog::SetView(
    MeasureView view)
{
    if (view == m_currentView || !m_viewport.IsReady()) {
        return;
    }

    if (m_session) {
        m_session->CancelDraft();
    }

    if (!m_viewport.SetView(view)) {
        return;
    }

    ClearCheckedTool();

    m_currentView = view;
}

void MeasureToolDialog::BeginTool(MeasureTool tool)
{
    if (!m_session || !m_viewport.IsReady()) 
    {
        ClearCheckedTool();
        return;
    }

    m_session->Begin({
        tool,
        m_currentView
        });

    if (m_vtkWidget) {
        m_vtkWidget->setFocus(
            Qt::MouseFocusReason);
    }   
}

void MeasureToolDialog::UndoMeasurement()
{
    if (m_session) {
        m_session->Undo();
    }
}

void MeasureToolDialog::RedoMeasurement()
{
    if (m_session) {
        m_session->Redo();
    }
}

void MeasureToolDialog::UpdateHistoryButtons()
{
    if (m_undoButton) {
        m_undoButton->setEnabled(m_session && m_session->CanUndo());
    }
    if (m_redoButton) {
        m_redoButton->setEnabled(m_session && m_session->CanRedo());
    }
}

void MeasureToolDialog::ClearCheckedTool()
{
    if (!m_toolGroup) {
        return;
    }
    m_toolGroup->setExclusive(false);
    for (auto* button : m_toolGroup->buttons()) {
        button->setChecked(false);
    }
    m_toolGroup->setExclusive(true);
}

} 

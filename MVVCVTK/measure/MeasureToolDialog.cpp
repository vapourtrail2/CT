#include "measure/MeasureToolDialog.h"

#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementSession.h"
#include "measure/MeasurementView.h"
#include "measure/MeasurementZoomHandler.h"
#include "App/AppState.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeySequence>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkImageData.h>

#include <array>
#include <vector>

namespace measure {
MeasureToolDialog::MeasureToolDialog(
    const std::shared_ptr<AbstractDataManager>& dataManager,
    const std::shared_ptr<SharedInteractionState>& sourceState,
    QWidget* parent)
    : QDialog(parent)
    , m_dataManager(dataManager)
{
    BuildUi();
    BuildMeasurementViewport(sourceState);
}

MeasureToolDialog::~MeasureToolDialog()
{
    if (m_session) {
        m_session->SetChangedCallback({});
    }
    if (const auto& service = m_viewport.getService(); service && m_overlay) {
        service->SetOverlayStrategyRemoved(m_overlay);
    }
    m_viewport.reset();
    m_overlay.reset();
    m_session.reset();
    m_localState.reset();
    m_localBroadcaster.reset();
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
    for (auto* button : { m_lineButton, m_circleButton, m_arcButton }) {
        button->setCheckable(true);
        button->setMinimumSize(72, 32);
    }
    for (auto* button : { m_undoButton, m_redoButton }) {
        button->setMinimumSize(72, 32);
        button->setEnabled(false);
    }
    m_undoButton->setToolTip(QStringLiteral("撤销最后一次测量或最后一个取点（Ctrl+Z）"));
    m_redoButton->setToolTip(QStringLiteral("重做最后一次撤销的测量（Ctrl+Y）"));

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
    const std::shared_ptr<SharedInteractionState>& sourceState)
{
    if (!m_dataManager || !m_vtkWidget || !m_dataManager->GetVtkImage()) {
        return;
    }

    m_localBroadcaster = std::make_shared<SharedStateBroadcaster>();
    m_localState = std::make_shared<SharedInteractionState>(m_localBroadcaster);

    if (sourceState) {
        m_localState->SetModelMatrix(sourceState->GetModelMatrix());
        m_localState->SetMaterial(sourceState->GetMaterial());
        m_localState->SetBackground(sourceState->GetBackground());
        const auto windowLevel = sourceState->GetWindowLevel();
        m_localState->SetWindowLevel(windowLevel.windowWidth, windowLevel.windowCenter);
        std::vector<TFNode> nodes;
        sourceState->GetTFNodes(nodes);
        m_localState->SetTFNodes(nodes);

        const uint32_t visibility = sourceState->GetVisibilityMask();
        m_localState->SetElementVisible(
            VisFlags::Crosshair, (visibility & VisFlags::Crosshair) != 0);
        m_localState->SetElementVisible(
            VisFlags::Ruler, (visibility & VisFlags::Ruler) != 0);
    }

    m_session = std::make_shared<MeasurementSession>();
    m_viewport.setAttach(m_vtkWidget);
    m_viewport.setOrientationAxesVisible(false);
    m_viewport.setMode(GetSliceViewDescriptor(m_currentView).vizMode);
    ConfigureMeasurementInteraction(m_currentView);
    m_viewport.setBuild(m_dataManager, m_localState, m_localBroadcaster);

    const auto& service = m_viewport.getService();
    if (!service) {
        return;
    }

    m_overlay = std::make_shared<MeasurementOverlayStrategy>(
        m_session, service.get(), m_currentView);

    m_session->SetChangedCallback([this]() {
        if (m_overlay) m_overlay->Refresh();
        if (const auto& currentService = m_viewport.getService()) {
            currentService->SetDirtyMarked();
        }
        if (m_statusLabel && m_session) {
            m_statusLabel->setText(
                QString::fromUtf8(m_session->StatusMessage().c_str()));
        }
        if (m_session && !m_session->IsActive()) {
            ClearCheckedTool();
        }
        UpdateHistoryButtons();
        QTimer::singleShot(0, this, [this]() {
            m_viewport.render();
        });
    });

    PublishCurrentImage();
    m_viewport.start();
    m_viewport.processPendingUpdates();
    m_viewport.processPendingUpdates();
    AttachOverlayAfterPipelineReady();
    m_viewport.render();
}

void MeasureToolDialog::PublishCurrentImage()
{
    if (!m_dataManager || !m_localState) {
        return;
    }
    auto image = m_dataManager->GetVtkImage();
    if (!image) {
        return;
    }

    double range[2] = { 0.0, 0.0 };
    double spacing[3] = { 1.0, 1.0, 1.0 };
    image->GetScalarRange(range);
    image->GetSpacing(spacing);
    const auto windowLevel = m_localState->GetWindowLevel();
    m_localState->SetReloadDataReady(
        range[0], range[1], { spacing[0], spacing[1], spacing[2] });
    m_localState->SetWindowLevel(windowLevel.windowWidth, windowLevel.windowCenter);
}

void MeasureToolDialog::AttachOverlayAfterPipelineReady()//管线完成之后再添加测量叠加层，避免在管线未完成时就添加叠加层导致画线出不来的问题
{
    const auto& service = m_viewport.getService();
    if (!service || !m_overlay) {
        return;
    }

    service->SetOverlayStrategyAdded(m_overlay);
    m_viewport.processPendingUpdates();
    m_overlay->Refresh();
    service->SetDirtyMarked();
}

void MeasureToolDialog::ConfigureMeasurementInteraction(MeasureView view)
{
    const auto session = m_session;
    const auto dataManager = m_dataManager;
    m_viewport.setInteractionHandlerFactory(
        [session, dataManager, view](
            AbstractInteractiveService* service,
            vtkRenderer* renderer) {
            std::vector<std::unique_ptr<IInteractionHandler>> handlers;
            handlers.push_back(std::make_unique<MeasurementZoomHandler>(
                service, renderer));
            handlers.push_back(std::make_unique<MeasurementInteractionHandler>(
                session, dataManager, service, renderer, view));
            return handlers;
        });
}

void MeasureToolDialog::SetView(MeasureView view)
{
    const auto& service = m_viewport.getService();
    if (view == m_currentView || !service || !m_viewport.getContext() || !m_overlay) {
        return;
    }

    if (m_session) {
        m_session->CancelDraft();
    }
    ClearCheckedTool();

    service->SetOverlayStrategyRemoved(m_overlay);
    m_currentView = view;

    m_viewport.setMode(GetSliceViewDescriptor(view).vizMode);
    ConfigureMeasurementInteraction(view);
    m_overlay->SetView(view);
    PublishCurrentImage();
    m_viewport.processPendingUpdates();
    m_viewport.processPendingUpdates();
    AttachOverlayAfterPipelineReady();
    m_viewport.render();
}

void MeasureToolDialog::BeginTool(MeasureTool tool)
{
    const auto& service = m_viewport.getService();
    if (!m_session || !service) {
        ClearCheckedTool();
        return;
    }
    m_session->Begin({ tool, m_currentView });
    service->SetDirtyMarked();
    if (m_vtkWidget) {
        m_vtkWidget->setFocus(Qt::MouseFocusReason);
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

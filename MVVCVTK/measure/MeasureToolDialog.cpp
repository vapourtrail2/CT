#include "measure/MeasureToolDialog.h"

#include "measure/MeasurementOverlayStrategy.h"
#include "measure/MeasurementSession.h"
#include "c_ui/qt/QtRenderContext.h"
#include "App/AppState.h"
#include "Service/AppService.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkImageData.h>

#include <array>
#include <vector>

namespace measure {
namespace {
VizMode ToVizMode(MeasureView view)
{
    switch (view) {
    case MeasureView::Axial: return VizMode::SliceTop_down;
    case MeasureView::Coronal: return VizMode::SliceFront_back;
    case MeasureView::Sagittal: return VizMode::SliceLeft_right;
    }
    return VizMode::SliceTop_down;
}
}

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
    if (m_service && m_overlay) {
        m_service->SetOverlayStrategyRemoved(m_overlay);
    }
    m_context.reset();
    m_overlay.reset();
    m_service.reset();
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
    for (auto* button : { m_lineButton, m_circleButton, m_arcButton }) {
        button->setCheckable(true);
        button->setMinimumSize(72, 32);
    }

    m_toolGroup = new QButtonGroup(this);
    m_toolGroup->setExclusive(true);
    m_toolGroup->addButton(m_lineButton, static_cast<int>(MeasureTool::Line));
    m_toolGroup->addButton(m_circleButton, static_cast<int>(MeasureTool::Circle3Point));
    m_toolGroup->addButton(m_arcButton, static_cast<int>(MeasureTool::Arc3Point));

    controls->addWidget(viewLabel);
    controls->addWidget(m_viewCombo);
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
        [this]() { BeginTool(MeasureTool::Line); });
    connect(m_circleButton, &QPushButton::clicked, this,
        [this]() { BeginTool(MeasureTool::Circle3Point); });
    connect(m_arcButton, &QPushButton::clicked, this,
        [this]() { BeginTool(MeasureTool::Arc3Point); });
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
    m_service = std::make_shared<MedicalVizService>(
                     m_dataManager, m_localState, m_localBroadcaster);
    m_context = std::make_shared<QtRenderContext>();
    m_context->SetQtWidget(m_vtkWidget);
    m_context->SetServiceBound(m_service);

    const VizMode initialMode = ToVizMode(m_currentView);
    m_service->SetVizMode(initialMode);
    m_context->SetCameraStyleByVizMode(initialMode);
    m_context->SetMeasurementRuntime(m_session, m_dataManager, m_currentView);

    m_overlay = std::make_shared<MeasurementOverlayStrategy>(
        m_session, m_service.get(), m_currentView);

    m_session->SetChangedCallback([this]() {
        if (m_overlay) m_overlay->Refresh();
        if (m_service) m_service->SetDirtyMarked();
        if (m_statusLabel && m_session) {
            m_statusLabel->setText(
                QString::fromUtf8(m_session->StatusMessage().c_str()));
        }
        if (m_session && !m_session->IsActive()) {
            ClearCheckedTool();
        }
        QTimer::singleShot(0, this, [this]() {
            if (m_context) m_context->SetRendered();
        });
    });

    PublishCurrentImage();
    m_context->SetStarted();
    m_service->SetPendingUpdatesProcessed();
    m_service->SetPendingUpdatesProcessed();
    AttachOverlayAfterPipelineReady();
    m_context->SetRendered();
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
    if (!m_service || !m_overlay) {
        return;
    }

    m_service->SetOverlayStrategyAdded(m_overlay);
    m_service->SetPendingUpdatesProcessed();
    m_overlay->Refresh();
    m_service->SetDirtyMarked();
}

void MeasureToolDialog::SetView(MeasureView view)
{
    if (view == m_currentView || !m_service || !m_context || !m_overlay) {
        return;
    }

    if (m_session) {
        m_session->CancelDraft();
    }
    ClearCheckedTool();

    m_service->SetOverlayStrategyRemoved(m_overlay);
    m_currentView = view;

    const VizMode mode = ToVizMode(view);
    m_service->SetVizMode(mode);
    m_context->SetCameraStyleByVizMode(mode);
    m_context->SetMeasurementRuntime(m_session, m_dataManager, view);
    m_overlay->SetView(view);
    PublishCurrentImage();
    m_service->SetPendingUpdatesProcessed();
    m_service->SetPendingUpdatesProcessed();
    AttachOverlayAfterPipelineReady();
    m_context->SetRendered();
}

void MeasureToolDialog::BeginTool(MeasureTool tool)
{
    if (!m_session || !m_service) {
        ClearCheckedTool();
        return;
    }
    m_session->Begin({ tool, m_currentView });
    m_service->SetDirtyMarked();
    if (m_vtkWidget) {
        m_vtkWidget->setFocus(Qt::MouseFocusReason);
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

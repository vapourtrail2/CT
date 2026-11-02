#include "measure/MainMeasurementFeature.h"
#include "measure/MeasureViewAdapter.h"
#include "measure/MeasurementInteractionHandler.h"
#include "measure/MeasurementOverlayStrategy.h"
#include "MVVCVTK/SPI/App/Services/FeatureViewService.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vtkImageData.h>

namespace measure {
namespace {
constexpr const char* kFeature = "ct.measurement";
constexpr const char* kPick = "ct.measurement.pick";
constexpr const char* kForward = "ct.measurement.sliceLock.forward";
constexpr const char* kBackward = "ct.measurement.sliceLock.backward";

InteractionActionInfo MakeAction(const char* id, InputTriggerKind trigger)
{
    InteractionActionInfo action;
    action.actionId = id;
    action.ownerId = kFeature;
    InteractionActionBinding binding;
    binding.bindingKey = id;
    binding.defaultBinding.trigger = trigger;
    binding.defaultBinding.button = trigger == InputTriggerKind::Drag ? InputMouseButton::Primary : InputMouseButton::None;
    binding.defaultBinding.forbiddenModifiers = trigger == InputTriggerKind::Drag ? 7 : 0;
    binding.scope = BindingScope::Slices();
    binding.scene = kFeature;
    binding.supportedTriggers = {trigger};
    action.bindings.push_back(std::move(binding));
    return action;
}
}

struct MainMeasurementFeature::View {
    HostViewTarget target;
    std::string id;
    MeasureView slice = MeasureView::Axial;
    bool is3D = false;
    std::unique_ptr<MeasureViewAdapter> adapter;
    std::unique_ptr<MeasurementOverlayStrategy> overlay;
    std::unique_ptr<MeasurementInteractionHandler> handler;
    std::unique_ptr<EdgeCaptureController> edge;
    std::optional<std::array<double, 16>> matrix;
    std::array<double, 3> cursor{};
};

MainMeasurementFeature::MainMeasurementFeature(std::shared_ptr<MeasurementSession> session, ViewStateReader readState)
    : m_session(std::move(session)), m_readState(std::move(readState)) {}

MainMeasurementFeature::~MainMeasurementFeature()
{
    // Normal cleanup is performed by DetachHost while SDK leases are valid.
    if (m_subscription) m_session->Unsubscribe(m_subscription);
}

std::string_view MainMeasurementFeature::GetFeatureId() const noexcept { return kFeature; }

bool MainMeasurementFeature::AttachHost(const HostFeatureContext& context)
{
    if (!context.data || !context.views || !context.host || !m_session) return false;
    m_context = context;
    m_image = context.data->GetPrimaryImage();
    if (!m_image || !m_image->image) return false;
    const std::array<HostRenderViewRole, 4> roles{HostRenderViewRole::TopDownSlice,
        HostRenderViewRole::FrontBackSlice, HostRenderViewRole::LeftRightSlice, HostRenderViewRole::Primary3D};
    for (std::size_t i = 0; i < roles.size(); ++i) {
        auto view = std::make_unique<View>();
        view->target.isViewRoleUsed = true;
        view->target.viewRole = roles[i];
        view->is3D = i == 3;
        view->slice = i < 3 ? static_cast<MeasureView>(i) : MeasureView::Axial;
        const auto input = context.views->GetInputView(view->target);
        const auto state = m_readState(view->target);
        if (!input || !state) { DetachHost(); return false; }
        view->id = input->view.id;
        view->adapter = std::make_unique<MeasureViewAdapter>();
        if (!view->adapter->AttachHostView(*input, m_image, context.views->GetFeaturePort(view->id), *state, view->slice)) {
            DetachHost();
            return false;
        }
        view->overlay = std::make_unique<MeasurementOverlayStrategy>(m_session, view->adapter.get(), view->slice);
        view->overlay->SetThreeDimensional(view->is3D);
        view->overlay->AttachRenderer(input->renderer);
        if (!view->is3D) {
            view->handler = std::make_unique<MeasurementInteractionHandler>(m_session, view->adapter.get(), input->renderer, view->slice);
            view->edge = std::make_unique<EdgeCaptureController>(view->adapter.get(), input->renderer, view->slice);
            view->edge->SetStatusCallback([this](const std::string& message) { m_session->SetStatusMessage(message); });
        }
        m_views.push_back(std::move(view));
    }

    HostInputBinding binding;
    binding.featureId = kFeature;
    binding.targetViews.viewRoles = {HostRenderViewRole::TopDownSlice, HostRenderViewRole::FrontBackSlice, HostRenderViewRole::LeftRightSlice};
    binding.actions = {MakeAction(kPick, InputTriggerKind::Drag), MakeAction(kForward, InputTriggerKind::WheelForward), MakeAction(kBackward, InputTriggerKind::WheelBackward)};
    binding.onAction = [this](std::string_view, const InteractionEvent& event, const auto&) { return OnInput(event); };
    binding.onInput = [this](const InteractionEvent& event) {
        return event.eventKind == InteractionEventKind::Cancel ? OnInput(event) : InteractionResult{};
    };
    binding.getIsActionAvailable = [this](std::string_view action, const InteractionEvent& event) { return IsActionAvailable(action, event); };
    binding.getIsCaptureActive = [this](std::string_view) { return !m_dragView.empty(); };
    m_attachedInput = context.host->AttachInput(std::move(binding));
    if (!m_attachedInput) { DetachHost(); return false; }
    m_subscription = m_session->Subscribe([this](MeasurementSession::Change) { MarkDirty(); });
    MarkDirty();
    return true;
}

bool MainMeasurementFeature::DetachHost()
{
    if (m_attachedInput) {
        if (!m_context.host->SetInputOwner(kFeature, false) || !m_context.host->DetachInput(kFeature)) return false;
        m_attachedInput = false;
    }
    if (m_subscription) m_session->Unsubscribe(std::exchange(m_subscription, 0));
    m_session->CancelDraft();
    for (auto& view : m_views) {
        if (view->adapter->HasValidHostLease()) {
            if (view->edge) view->edge->SetEnabled(false);
            view->overlay->DetachRenderer();
            view->adapter->SendRender();
        }
    }
    m_views.clear();
    m_image.reset();
    m_context = {};
    return true;
}

void MainMeasurementFeature::MarkDirty()
{
    m_dirty = true;
    for (const auto& view : m_views) view->adapter->SendRender();
}

bool MainMeasurementFeature::OnHostTick()
{
    const bool wasMatching = m_dataMatches;
    const auto image = m_context.data->GetPrimaryImage();
    m_dataMatches = image && image->data && m_image->data && image->binding && m_image->binding
        && image->data->self == m_image->data->self && image->binding->revision == m_image->binding->revision;
    if (m_dataMatches) {
        for (const auto& view : m_views) {
            const auto state = m_readState(view->target);
            if (!state || state->dataRevision != m_image->data->self || state->bindingRevision != m_image->binding->revision) {
                m_dataMatches = false;
                break;
            }
        }
    }
    if (!m_dataMatches) {
        if (wasMatching) {
            EndEdit();
            for (auto& view : m_views) if (view->adapter->HasValidHostLease()) view->overlay->DetachRenderer();
        }
        return true;
    }
    if (!wasMatching) {
        for (auto& view : m_views) if (view->adapter->HasValidHostLease()) view->overlay->AttachRenderer(view->adapter->GetRenderer());
        MarkDirty();
    }
    for (auto& view : m_views) {
        const auto state = m_readState(view->target);
        if (!state || !view->adapter->SyncHostView(*state)) continue;
        const auto matrix = m_context.views->GetFeaturePort(view->id)->GetModelToWorld();
        if (matrix != view->matrix || state->cursorWorld != view->cursor) m_dirty = true;
        view->matrix = matrix;
        view->cursor = state->cursorWorld;
        if (view->id == m_sourceView && !view->is3D) {
            const auto plane = view->adapter->GetPlane();
            const auto& draft = m_session->Draft();
            const bool changedDraft = draft && draft->plane && !MeasurementSession::MatchesPlane(*draft->plane, plane);
            const bool changedCapture = m_capturePlane && !MeasurementSession::MatchesPlane(*m_capturePlane, plane);
            if (changedDraft || changedCapture) { EndEdit(); m_session->SetStatusMessage("来源切片已改变，已取消未完成测量。 "); }
        }
    }
    if (std::exchange(m_dirty, false)) {
        for (auto& view : m_views) {
            if (!view->adapter->HasValidHostLease()) continue;
            view->overlay->Refresh();
            if (view->edge && view->edge->IsEnabled()) view->edge->Refresh();
            view->adapter->SendRender();
        }
    }
    return true;
}

bool MainMeasurementFeature::EnableInput()
{
    m_inputEnabled = m_context.host && m_context.host->SetInputOwner(kFeature, true);
    return m_inputEnabled;
}

void MainMeasurementFeature::EndEdit()
{
    m_session->CancelDraft();
    for (auto& view : m_views) if (view->edge && view->adapter->HasValidHostLease()) view->edge->SetEnabled(false);
    m_tool = MeasureTool::None;
    m_captureShape.reset();
    m_capturePlane.reset();
    m_sourceView.clear();
    m_dragView.clear();
    MarkDirty();
}

void MainMeasurementFeature::Cancel()
{
    EndEdit();
    if (m_context.host) m_context.host->SetInputOwner(kFeature, false);
    m_inputEnabled = false;
}

bool MainMeasurementFeature::BeginTool(MeasureTool tool)
{
    Cancel();
    if (!m_dataMatches || !EnableInput()) return false;
    m_tool = tool;
    m_session->Begin({tool, MeasureView::Axial});
    return true;
}

bool MainMeasurementFeature::BeginCapture(EdgeCaptureShape shape)
{
    Cancel();
    if (!m_dataMatches || !EnableInput()) return false;
    m_captureShape = shape;
    m_session->SetStatusMessage("请先点击来源二维视图，再拖动抓边框；确认结果后才加入场景树。 ");
    return true;
}

bool MainMeasurementFeature::IsWheelLocked(const std::string& viewId) const
{
    if (viewId != m_sourceView) return false;
    const auto& draft = m_session->Draft();
    return m_capturePlane.has_value() || (draft && !draft->physicalPoints.empty());
}

bool MainMeasurementFeature::IsActionAvailable(std::string_view action, const InteractionEvent& event) const
{
    if (!m_inputEnabled || !m_dataMatches) return false;
    const bool slice = std::any_of(m_views.begin(), m_views.end(), [&event](const auto& view) { return view->id == event.viewId && !view->is3D; });
    if (!slice) return false;
    if (action == kForward || action == kBackward) return IsWheelLocked(event.viewId);
    const auto& draft = m_session->Draft();
    const bool hasSource = m_capturePlane || (draft && !draft->physicalPoints.empty());
    if (hasSource && event.viewId != m_sourceView) return false;
    return action == kPick && (m_session->IsActive() || m_captureShape.has_value() || !m_dragView.empty());
}

InteractionResult MainMeasurementFeature::OnInput(const InteractionEvent& event)
{
    if (event.eventKind == InteractionEventKind::Cancel) { EndEdit(); return {true, true}; }
    if (!m_inputEnabled || !m_dataMatches) return {};
    const auto item = std::find_if(m_views.begin(), m_views.end(), [&event](const auto& view) { return view->id == event.viewId; });
    if (item == m_views.end() || (*item)->is3D) return {};
    auto& view = **item;
    const auto state = m_readState(view.target);
    if (!state || !view.adapter->SyncHostView(*state)) return {};
    if (event.eventKind == InteractionEventKind::WheelForward || event.eventKind == InteractionEventKind::WheelBackward) return IsWheelLocked(view.id) ? InteractionResult{true, true} : InteractionResult{};
    if (event.eventKind == InteractionEventKind::PrimaryPress) {
        if (m_sourceView.empty() || (m_session->Draft() && m_session->Draft()->physicalPoints.empty())) {
            m_sourceView = view.id;
            if (m_captureShape) {
                m_capturePlane = view.adapter->GetPlane();
                view.edge->SetShape(*m_captureShape);
                view.edge->SetEnabled(true);
            }
            else if (m_tool != MeasureTool::None) m_session->Begin({m_tool, view.slice});
        }
        if (view.id != m_sourceView) return {}; // Other slices retain navigation.
        m_dragView = view.id;
    }
    if (view.id != m_sourceView) return {};
    const auto result = m_captureShape ? view.edge->Send(event) : view.handler->Send(event);
    if (event.eventKind == InteractionEventKind::PrimaryRelease) m_dragView.clear();
    if (!m_session->IsActive() && !m_captureShape && m_dragView.empty()) m_tool = MeasureTool::None;
    MarkDirty();
    return result;
}

bool MainMeasurementFeature::ConfirmCapture(std::string& error)
{
    const auto item = std::find_if(m_views.begin(), m_views.end(), [this](const auto& view) { return view->id == m_sourceView; });
    if (!m_captureShape || item == m_views.end()) { error = "当前没有待确认的抓边结果。"; return false; }
    const auto state = m_readState((*item)->target);
    if (!state || !(*item)->adapter->SyncHostView(*state) || !m_capturePlane
        || !MeasurementSession::MatchesPlane(*m_capturePlane, (*item)->adapter->GetPlane())) {
        Cancel();
        error = "来源切片已经改变，请重新抓边。";
        return false;
    }
    auto entity = (*item)->edge->TakeResult(error);
    if (!entity || !m_session->CommitCaptured(std::move(*entity), &error)) return false;
    Cancel();
    return true;
}

} // namespace measure

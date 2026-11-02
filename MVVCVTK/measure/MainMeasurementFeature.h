#pragma once

#include "MVVCVTK/SPI/Host/HostFeature.h"
#include "MVVCVTK/API/Host/Types/HostSessionTypes.h"
#include "measure/MeasurementSession.h"
#include "measure/EdgeCaptureController.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace measure {

// Borrows the active volume's collection and the SDK's four views.
class MainMeasurementFeature final : public HostFeature {
public:
    using ViewStateReader = std::function<std::optional<HostRenderViewState>(const HostViewTarget&)>;
    MainMeasurementFeature(std::shared_ptr<MeasurementSession> session, ViewStateReader readState);
    ~MainMeasurementFeature() override;
    std::string_view GetFeatureId() const noexcept override;
    bool AttachHost(const HostFeatureContext& context) override;
    bool DetachHost() override;
    bool OnHostTick() override;

    bool BeginTool(MeasureTool tool);
    bool BeginCapture(EdgeCaptureShape shape);
    bool ConfirmCapture(std::string& error);
    void Cancel();

private:
    struct View;
    bool EnableInput();
    bool IsWheelLocked(const std::string& viewId) const;
    bool IsActionAvailable(std::string_view action, const InteractionEvent& event) const;
    InteractionResult OnInput(const InteractionEvent& event);
    void MarkDirty();
    void EndEdit();

    std::shared_ptr<MeasurementSession> m_session;
    ViewStateReader m_readState;
    HostFeatureContext m_context;
    VtkImageGridSnapshot m_image;
    std::vector<std::unique_ptr<View>> m_views;
    MeasurementSession::ObserverId m_subscription = 0;
    MeasureTool m_tool = MeasureTool::None;
    std::optional<EdgeCaptureShape> m_captureShape;
    std::string m_sourceView;
    std::string m_dragView;
    std::optional<MeasurementPlane> m_capturePlane;
    bool m_dirty = true;
    bool m_attachedInput = false;
    bool m_inputEnabled = false;
    bool m_dataMatches = true;
};

} // namespace measure

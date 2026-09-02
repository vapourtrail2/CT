#include "measure/MeasureHostBridge.h"

#include "MVVCVTK/SPI/App/Services/FeatureViewService.h"

#include <utility>

namespace measure {

MeasureHostBridge::MeasureHostBridge(HostViewTarget sourceView)
    : m_sourceView(std::move(sourceView))
{
}

std::string_view MeasureHostBridge::GetFeatureId() const noexcept
{
    return "ct.measure-host-bridge";
}

bool MeasureHostBridge::AttachHost(const HostFeatureContext& context)
{
    m_hostData.reset();
    if (!context.data || !context.views) {
        return false;
    }

    const auto inputView = context.views->GetInputView(m_sourceView);
    if (!inputView) {
        return false;
    }

    const auto viewPort =
        context.views->GetFeaturePort(inputView->view.id);
    if (!viewPort) {
        return false;
    }

    const auto image = context.data->GetImageSnapshot();
    const auto modelToWorld = viewPort->GetModelToWorld();
    if (!image || !image->image || !modelToWorld) {
        return false;
    }

    m_hostData = MeasureHostData{ image, *modelToWorld };
    return true;
}

bool MeasureHostBridge::DetachHost()
{
    m_hostData.reset();
    return true;
}

bool MeasureHostBridge::OnHostTick()
{
    return true;
}

const std::optional<MeasureHostData>&
MeasureHostBridge::GetHostData() const noexcept
{
    return m_hostData;
}

} // namespace measure

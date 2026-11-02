#include "measure/MeasurementSession.h"
#include "measure/MeasurementGeometry.h"
#include "measure/MainMeasurementFeature.h"
#include "c_ui/context/VolumeManager.h"
#include "c_ui/panels/SceneTreeModel.h"
#include "MVVCVTK/API/Host/VtkAppHostSession.h"

#include <QCoreApplication>
#include <QPersistentModelIndex>
#include <vtkAutoInit.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkRenderer.h>
#include <vtkPropCollection.h>
#include <vtkRenderWindow.h>

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

VTK_MODULE_INIT(vtkRenderingOpenGL2);
VTK_MODULE_INIT(vtkRenderingVolumeOpenGL2);
VTK_MODULE_INIT(vtkInteractionStyle);

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void AddLine(measure::MeasurementSession& session, double z = 0.0)
{
    measure::MeasurementPlane plane;
    plane.origin[2] = z;
    session.Begin({measure::MeasureTool::Line, measure::MeasureView::Axial});
    Require(session.AppendPoint({0, 0, z}, plane), "line first point");
    Require(session.AppendPoint({3, 4, z}, plane), "line second point");
}

void TestVolumeOwnership()
{
    VolumeManager volumes;
    auto image = std::make_shared<VtkImageGridView>();
    const int a = volumes.addVolume("A", image);
    const int b = volumes.addVolume("B", image, a);
    auto first = volumes.getVolume(a).measurements;
    auto second = volumes.getVolume(b).measurements;
    Require(first != second, "each volume must own a different collection");
    AddLine(*first);
    AddLine(*first, 1);
    AddLine(*first, 2);
    Require(first->Entities().size() == 3 && second->Entities().empty(), "one volume supports many objects");
    AddLine(*second);
    Require(first->Entities()[0].id == second->Entities()[0].id, "local IDs may overlap across volumes");
    first.reset();
    first = volumes.getVolume(a).measurements;
    Require(first->Entities().size() == 3, "window lifetime must not own results");
    Require(second->Undo() && second->Entities().empty() && first->Entities().size() == 3, "undo is per volume");
    Require(second->Redo() && second->Entities().size() == 1, "redo survives collection reattachment");
    const auto id = first->Entities()[0].id;
    Require(first->SetVisible(id, false) && !first->Entities()[0].visible, "visibility updates object");
    Require(first->Rename(id, "renamed") && first->Entities()[0].name == "renamed", "rename updates object");
    Require(first->Remove(id) && first->Entities().size() == 2, "delete only selected object");
    Require(first->Undo() && first->Entities().size() == 3, "delete can be undone");
    Require(!second->Entities()[0].name.empty() && second->Entities()[0].visible, "properties do not leak across volumes");
    std::cout << "PASS 1.4 ownership, many objects, properties, independent history\n";
}

void TestSingleSliceAndNotifications()
{
    measure::MeasurementSession session;
    int objects = 0, drafts = 0;
    auto observer = session.Subscribe([&](auto change) {
        if (change == measure::MeasurementSession::Change::Objects) ++objects;
        else ++drafts;
    });
    measure::MeasurementPlane plane;
    session.Begin({measure::MeasureTool::Line, measure::MeasureView::Axial});
    Require(session.AppendPoint({0, 0, 0}, plane), "first point");
    session.UpdatePreview(measure::Point3{1, 1, 0});
    Require(objects == 0 && drafts > 0, "preview must not publish scene nodes");
    std::string error;
    Require(!session.AppendPoint({1, 0, 1}, plane, &error), "off-plane point must be rejected");
    Require(session.IsActive(), "invalid point keeps same-plane draft");
    auto other = plane;
    other.origin[2] = 1;
    Require(!session.AppendPoint({1, 0, 1}, other, &error) && !session.IsActive(), "different slice cancels draft");
    AddLine(session);
    Require(objects == 1, "completion publishes one object change");
    session.Begin({measure::MeasureTool::Circle3Point, measure::MeasureView::Axial});
    session.AppendPoint({0, 0, 0}, plane);
    Require(session.Undo() && session.Draft()->physicalPoints.empty() && !session.Draft()->plane, "undo to zero releases source plane");
    session.CancelDraft();
    Require(session.Entities().size() == 1, "cancel keeps completed objects");
    session.Unsubscribe(observer);
    std::cout << "PASS 3.2 same-slice rule and draft/object notification separation\n";
}

void TestCapturedPhysics()
{
    measure::MeasurementSession session;
    measure::MeasurementEntity captured;
    captured.type = measure::MeasureTool::Line;
    captured.capturedPath = {{0, 0, 0}, {6, 12, 0}}; // Pixel delta (3,4), spacing (2,3).
    captured.physicalPoints = captured.capturedPath;
    captured.result = *measure::geometry::ComputeLine(captured.capturedPath);
    Require(session.CommitCaptured(captured), "commit captured line");
    Require(std::abs(std::get<measure::LineResult>(session.Entities().back().result).length - std::sqrt(180.0)) < 1e-9, "length uses physical coordinates");
    Require(session.CommitCaptured(captured) && session.Entities().size() == 2, "each confirmation creates a distinct result");
    Require(session.Entities()[0].id != session.Entities()[1].id, "capture IDs are unique within a volume");
    Require(session.Undo() && session.Entities().size() == 1, "capture uses normal undo history");
    std::cout << "PASS 4.1 confirmed captures and physical line length\n";
}

void TestTreeIdentity()
{
    SceneTreeModel model;
    int inserted = 0;
    QObject::connect(&model, &QAbstractItemModel::rowsInserted, [&] { ++inserted; });
    SceneNode volume;
    volume.key = "v:1";
    volume.name = "A";
    volume.kind = SceneNodeKind::Volume;
    SceneNode line;
    line.key = "v:1/m:1";
    line.parentKey = volume.key;
    line.name = "line";
    line.kind = SceneNodeKind::Measurement;
    line.canCheck = true;
    model.syncNodes({volume, line});
    QPersistentModelIndex original(model.index(0, 0, model.index(0, 0)));
    Require(original.isValid() && inserted == 2, "model publishes inserted rows");
    auto* item = model.itemFromIndex(original);
    line.name = "renamed";
    line.visible = false;
    model.syncNodes({volume, line});
    Require(original.isValid() && model.itemFromIndex(original) == item, "refresh preserves node identity");
    Require(item->text() == "renamed" && item->checkState() == Qt::Unchecked, "existing node updates");
    SceneNode crop;
    crop.key = "v:1/crop";
    crop.parentKey = volume.key;
    model.syncNodes({volume, line, crop});
    model.syncNodes({volume, line});
    Require(original.isValid() && model.item(0)->rowCount() == 1, "crop history refresh cannot erase measurements");
    std::cout << "PASS 2.1 incremental generic scene tree identity\n";
}

void TestSdkInput()
{
    HostSessionConfig config;
    config.driveMode = HostDriveMode::HostDriven;
    const HostRenderViewRole roles[]{HostRenderViewRole::TopDownSlice, HostRenderViewRole::FrontBackSlice, HostRenderViewRole::LeftRightSlice, HostRenderViewRole::Primary3D};
    const HostRenderMode modes[]{HostRenderMode::SliceTopDown, HostRenderMode::SliceFrontBack, HostRenderMode::SliceLeftRight, HostRenderMode::IsoSurface};
    for (int i = 0; i < 4; ++i) {
        HostRenderViewConfig view;
        view.id = "test-" + std::to_string(i);
        view.role = roles[i];
        view.inputMode = HostInputMode::HostInjected;
        view.window.viewInit.viewMode = modes[i];
        view.renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
        view.renderWindow->SetSize(400, 400);
        view.renderWindow->SetOffScreenRendering(1);
        view.renderWindow->SetShowWindow(false);
        config.renderViews.push_back(view);
    }
    VtkAppHostSession host(config);
    Require(host.BuildSession(), "SDK host builds");
    HostReloadRequest load;
    load.geometry.dimensions = {16, 16, 16};
    load.geometry.spacing = {1, 1, 1};
    load.voxels.assign(16 * 16 * 16, 100.0f);
    load.metadata.identity.datasetId = "measurement-integration-test";
    load.metadata.source.kind = ImageSourceKind::Memory;
    load.metadata.source.uri = "measurement-test:synthetic";
    bool done = false, success = false;
    const auto admission = host.SendRequest(std::move(load), [&](HostResult result) { done = true; success = result.isSucceeded; });
    if (admission.status != HostAdmissionStatus::Accepted) std::cerr << "SDK load rejection: " << admission.message << std::endl;
    Require(admission.status == HostAdmissionStatus::Accepted, "synthetic CT load admitted");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!done && std::chrono::steady_clock::now() < deadline) {
        host.SendUpdates();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(done && success, "synthetic CT activation completes");
    host.SendUpdates();
    auto session = std::make_shared<measure::MeasurementSession>();
    auto feature = std::make_shared<measure::MainMeasurementFeature>(session, [&](const HostViewTarget& target) { return host.GetRenderViewState(target); });
    Require(host.AttachFeature(feature), "measurement feature attaches to real SDK");
    int baseProps[4];
    for (int i = 0; i < 4; ++i) baseProps[i] = host.GetRenderViewEndpoint("test-" + std::to_string(i))->renderer->GetViewProps()->GetNumberOfItems();
    Require(feature->BeginTool(measure::MeasureTool::Line), "SDK grants measurement input ownership");
    const auto bindings = host.GetInteractionController()->GetBindings(BindingScope::View("test-0"));
    bool hasPick = false;
    for (const auto& binding : bindings) {
        if (binding.actionId == "ct.measurement.pick") {
            std::cout << "binding " << binding.viewId << " enabled=" << binding.isEnabled << " available=" << binding.isAvailable << " conflict=" << binding.hasConflict << '\n';
            hasPick |= binding.isEnabled && !binding.hasConflict;
        }
    }
    Require(hasPick, "measurement binding must be active without conflicts");
    auto* input = host.GetInputEndpoint();
    HostInputEvent event;
    event.viewId = "test-0";
    event.kind = HostInputKind::PrimaryPress;
    event.x = 200; event.y = 200;
    const auto press = input->SendInput(event);
    Require(press.isSucceeded && press.isHandled, "measurement receives injected primary input");
    event.kind = HostInputKind::PrimaryRelease;
    input->SendInput(event);
    Require(session->Draft() && session->Draft()->physicalPoints.size() == 1, "first point comes from the SDK slice");
    event.kind = HostInputKind::WheelForward;
    const auto locked = input->SendInput(event);
    Require(locked.isSucceeded && locked.isHandled && locked.isDefaultSuppressed, "first point locks only source slice wheel");
    host.SendUpdates();
    const auto readCursor = [&](const char* id) {
        HostViewTarget target;
        target.viewId = id;
        return host.GetRenderViewState(target)->cursorWorld;
    };
    const auto lockedCursor = readCursor("test-0");
    input->SendInput(event);
    host.SendUpdates();
    Require(readCursor("test-0") == lockedCursor, "source slice must stay at the draft plane");
    event.viewId = "test-1";
    input->SendInput(event);
    host.SendUpdates();
    Require(session->Draft() && session->Draft()->physicalPoints.size() == 1, "other slice navigation does not append draft points");
    session->Undo();
    event.viewId = "test-0";
    const auto beforeWheel = readCursor("test-0");
    input->SendInput(event);
    host.SendUpdates();
    Require(readCursor("test-0") != beforeWheel, "undo to zero unlocks normal slice navigation");
    const auto click = [&](int x, int y) {
        HostInputEvent pick;
        pick.viewId = "test-0";
        pick.x = x; pick.y = y;
        pick.kind = HostInputKind::PrimaryPress;
        Require(input->SendInput(pick).isHandled, "primary press routed to measurement");
        pick.kind = HostInputKind::PrimaryRelease;
        Require(input->SendInput(pick).isHandled, "paired release remains owned by measurement");
        host.SendUpdates();
    };
    click(190, 200);
    click(210, 200);
    Require(session->Entities().size() == 1 && !session->IsActive(), "two same-slice clicks complete one line");
    for (int i = 0; i < 4; ++i) {
        const auto count = host.GetRenderViewEndpoint("test-" + std::to_string(i))->renderer->GetViewProps()->GetNumberOfItems();
        Require(count > baseProps[i], "completed object is displayed in all four views");
    }
    feature->Cancel();
    Require(session->Entities().size() == 1, "leaving the tool keeps completed geometry");
    Require(host.DetachFeature(*feature), "feature detaches and releases input owner");
    for (int i = 0; i < 4; ++i) {
        const auto count = host.GetRenderViewEndpoint("test-" + std::to_string(i))->renderer->GetViewProps()->GetNumberOfItems();
        Require(count == baseProps[i], "detach removes each view's measurement props");
    }
    Require(host.Stop(), "SDK stops without leaked measurement leases");
    std::cout << "PASS 3.1 real SDK input attach, first point, wheel lock, cleanup\n";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        TestVolumeOwnership();
        TestSingleSliceAndNotifications();
        TestCapturedPhysics();
        TestTreeIdentity();
        if (argc > 1 && std::string(argv[1]) == "--sdk") TestSdkInput();
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}




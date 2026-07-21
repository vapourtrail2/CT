#include "measure/MeasurementView.h"

namespace measure {
namespace {
const SliceViewDescriptor kAxial{
    MeasureView::Axial,
    VizMode::SliceTop_down,
    2,
    { 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 }
};

const SliceViewDescriptor kCoronal{
    MeasureView::Coronal,
    VizMode::SliceFront_back,
    1,
    { 0.0, 1.0, 0.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 0.0, 1.0 }
};

const SliceViewDescriptor kSagittal{
    MeasureView::Sagittal,
    VizMode::SliceLeft_right,
    0,
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 },
    { 0.0, 0.0, 1.0 }
};
}

const SliceViewDescriptor& GetSliceViewDescriptor(MeasureView view)
{
    switch (view) {
    case MeasureView::Axial: return kAxial;
    case MeasureView::Coronal: return kCoronal;
    case MeasureView::Sagittal: return kSagittal;
    }
    return kAxial;
}

bool IsSliceVizMode(VizMode mode)
{
    return mode == VizMode::SliceTop_down
        || mode == VizMode::SliceFront_back
        || mode == VizMode::SliceLeft_right;
}

} // namespace measure

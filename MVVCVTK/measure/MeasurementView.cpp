#include "measure/MeasurementView.h"

namespace measure {
namespace {
const SliceViewDescriptor kAxial{
    MeasureView::Axial,
    2,
    { 0.0, 0.0, 1.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 1.0, 0.0 }
};

const SliceViewDescriptor kCoronal{
    MeasureView::Coronal,
    1,
    { 0.0, 1.0, 0.0 },
    { 1.0, 0.0, 0.0 },
    { 0.0, 0.0, 1.0 }
};

const SliceViewDescriptor kSagittal{
    MeasureView::Sagittal,
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

} // namespace measure

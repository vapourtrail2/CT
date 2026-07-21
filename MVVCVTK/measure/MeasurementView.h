#pragma once

#include "App/AppTypes.h"
#include "measure/MeasurementTypes.h"

namespace measure {

struct SliceViewDescriptor {
    MeasureView view = MeasureView::Axial;
    VizMode vizMode = VizMode::SliceTop_down;
    int axis = 2;
    Point3 normal{ 0.0, 0.0, 1.0 };
    Point3 u{ 1.0, 0.0, 0.0 };
    Point3 v{ 0.0, 1.0, 0.0 };
};

const SliceViewDescriptor& GetSliceViewDescriptor(MeasureView view);
bool IsSliceVizMode(VizMode mode);

} // namespace measure

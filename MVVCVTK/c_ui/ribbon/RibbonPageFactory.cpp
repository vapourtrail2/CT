#include "c_ui/ribbon/RibbonPageFactory.h"

#include "c_ui/workbenches/StartPage.h"
#include "c_ui/workbenches/EditPage.h"
#include "c_ui/workbenches/VolumePage.h"
#include "c_ui/workbenches/SelectPage.h"
#include "c_ui/workbenches/AlignmentPage.h"
#include "c_ui/workbenches/GeometryPage.h"
#include "c_ui/workbenches/MeasurePage.h"
#include "c_ui/workbenches/CADAndThen.h"
#include "c_ui/workbenches/AnalysisPage.h"
#include "c_ui/workbenches/ReportPage.h"
#include "c_ui/workbenches/AnimationPage.h"
#include "c_ui/workbenches/WindowPage.h"

QList<RibbonPage*> createAllRibbonPages(QWidget* parent)
{
    return {
        new StartPagePage(parent),
        new EditPage(parent),
        new VolumePage(parent),
        new SelectPage(parent),
        new AlignmentPage(parent),
        new GeometryPage(parent),
        new MeasurePage(parent),
        new CADAndThen(parent),
        new AnalysisPage(parent),
        new ReportPage(parent),
        new AnimationPage(parent),
        new WindowPage(parent),
    };
}

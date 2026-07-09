#include "c_ui/ribbon/RibbonPage.h"

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

void RibbonPageRegister::add(RibbonPage* page)
{
	if (!page) {
		return;
	}

	pagesList_.append(page);
	pagesByTab_.insert(page->tabIndex(), page);
}

QList<RibbonPage*> RibbonPageRegister::pages() const
{
	QList<RibbonPage*> result;

	for (const auto& page : pagesList_) {
		if (page) {
			result.append(page);
		}
	}

	return result;
}

RibbonPage* RibbonPageRegister::pageByTab(int tabIndex) const
{
	return pagesByTab_.value(tabIndex, nullptr);
}

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

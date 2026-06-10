#include "RibbonPageRegister.h"
#include "RibbonPage.h"

void RibbonPageRegister::add(RibbonPage* page)
{
	if(!page)
	{
		return;
	}

	pagesList_.append(page);
	pagesByTab_.insert(page->tabIndex(), page);
}

QList<RibbonPage*> RibbonPageRegister::pageslist() const
{
	QList<RibbonPage*> result;

	for (const auto &page :pagesList_ )
	{
		result.append(page);
	}

	return result;
}

RibbonPage* RibbonPageRegister::pageByTab(int tabIndex) const
{
	return pagesByTab_.value(tabIndex,nullptr);
}

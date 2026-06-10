#pragma once
#include <QHash>
#include <QPointer>
#include <QList>

class RibbonPage;

class RibbonPageRegister
{
public:
	//为什么不需要构造函数
	/*RibbonPageRegister() {}*/
	void add(RibbonPage* page);

	QList<RibbonPage*> pageslist() const;	
	RibbonPage* pageByTab(int tabIndex) const;

private:
	QList<QPointer<RibbonPage>> pagesList_;
	QHash<int, QPointer<RibbonPage>> pagesByTab_;
};
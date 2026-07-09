#pragma once
#include <QHash>
#include <QList>
#include <QPointer>
#include <QWidget>
#include <QString>

class RibbonPage : public QWidget
{
	Q_OBJECT
public:
	explicit RibbonPage(QWidget* parent = nullptr)
		:QWidget(parent)
	{
	}

	virtual int tabIndex() const = 0;
	virtual QString tabName() const = 0;
signals:
	void commandRequested(const QString& name);
};

class RibbonPageRegister
{
public:
	void add(RibbonPage* page);

	QList<RibbonPage*> pages() const;
	RibbonPage* pageByTab(int tabIndex) const;

private:
	QList<QPointer<RibbonPage>> pagesList_;
	QHash<int, QPointer<RibbonPage>> pagesByTab_;
};

QList<RibbonPage*> createAllRibbonPages(QWidget* parent);

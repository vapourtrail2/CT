#pragma once
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QIcon>
#include <QFile>
#include "c_ui/ribbon/RibbonPage.h"
#include "c_ui/comandid/CommandId.h"

class QToolButton;

class StartPagePage : public RibbonPage
{
	Q_OBJECT
public:
	explicit StartPagePage(QWidget* parent = nullptr);
	int tabIndex() const override;
	QString tabName() const override;

signals:
	//void distanceRequested();
	//void angleRequested();
	void commandRequested(CommandId command);

private:
	QWidget* buildRibbon01(QWidget* parent);
};

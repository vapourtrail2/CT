#pragma once
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QIcon>
#include <QDebug>
#include <QFile>
#include "c_ui/ribbon/RibbonPage.h"

class QToolButton;

class MeasurePage : public RibbonPage
{
	Q_OBJECT
public:
	explicit MeasurePage(QWidget* parent = nullptr);
	int tabIndex() const override;
	QString tabName() const override;

private:
	QWidget* buildRibbon06(QWidget* parent);

};

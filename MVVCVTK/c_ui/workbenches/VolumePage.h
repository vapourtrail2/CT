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
#include "c_ui/workbenches/common/RibbonCommon.h"

class QMenu;
class QToolButton;

class VolumePage : public RibbonPage
{
	Q_OBJECT
public:
	explicit VolumePage(QWidget* parent = nullptr);
	int tabIndex() const override;
	QString tabName() const override;

private:
	QWidget* buildRibbon02(QWidget* parent);
	QMenu* createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions);
	QToolButton* createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef);
	static QList<RibbonDef::RibbonButtonDef> createVolumeButtons();
};

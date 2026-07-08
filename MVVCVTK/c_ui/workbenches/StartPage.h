#pragma once
#include <QWidget>
#include <QList>
#include <QString>
#include "c_ui/ribbon/RibbonPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"

class QMenu;
class QToolButton;

class StartPagePage : public RibbonPage
{
	Q_OBJECT
public:
	explicit StartPagePage(QWidget* parent = nullptr);
	int tabIndex() const override;
	QString tabName() const override;

private:
	QWidget* buildRibbon(QWidget* parent);
	QMenu* createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions);
	QToolButton* createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef);
	static QList<RibbonDef::RibbonButtonDef> createStartButtons();
};

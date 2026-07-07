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
	struct PullMeauAction
	{
		QString text;
		QString iconPath;
		QString command;
	};
		
	struct RibbonButtonDef
	{
		QString text;
		QList<PullMeauAction> menuActions;
	};
	
	QWidget* setRibbon01(QWidget* parent);
	QMenu* createMenu(QWidget* parent, const QList<PullMeauAction>& menuActions);
	QToolButton* createButton(QWidget* parent, const RibbonButtonDef& buttonDef);
	static QList<RibbonButtonDef> createStartButtons();
};

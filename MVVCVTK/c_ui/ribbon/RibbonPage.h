#pragma once
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
#pragma once
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QIcon>
#include <QDebug>
#include <QFile>

class QToolButton;

class StartPagePage : public QWidget
{
	Q_OBJECT
public:
	explicit StartPagePage(QWidget* parent = nullptr);

signals:
	void distanceRequested();
	void angleRequested();
	void ctReconRequested();
    void sliceStackSaveRequested();
	void transformedDataSaveRequested();

private:
	QWidget* buildRibbon01(QWidget* parent);
};

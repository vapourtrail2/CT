#pragma once
#include <QList>

class QWidget;
class RibbonPage;

QList<RibbonPage*> createAllRibbonPages(QWidget* parent);

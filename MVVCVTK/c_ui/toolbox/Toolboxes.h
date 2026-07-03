#pragma once
#include <QList>

class QWidget;
class RibbonPage;

// 所有 Ribbon 选项卡（工具箱）的集中登记处。

QList<RibbonPage*> createAll(QWidget* parent);
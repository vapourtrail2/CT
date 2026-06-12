#include "c_ui/nav/TabMap.h"
#include <QWidget>

TabMap::TabMap()
    : tabNames_{
        QStringLiteral("文件"),
        QStringLiteral("开始"),
        QStringLiteral("编辑"),
        QStringLiteral("体积"),
        QStringLiteral("选择"),
        QStringLiteral("对齐"),
        QStringLiteral("几何"),
        QStringLiteral("测量"),
        QStringLiteral("CAD/表面测量"),
        QStringLiteral("分析"),
        QStringLiteral("报告"),
        QStringLiteral("动画"),
        QStringLiteral("窗口"),
    }
{
}

const QStringList& TabMap::tabNames() const
{
    return tabNames_;
}

bool TabMap::isFileTab(int index) const
{
    return index == TabIndex::File;
}

bool TabMap::isValidTab(int index) const
{
    return index >= TabIndex::File && index < TabIndex::Window;
}

void TabMap::bindTabPage(int index, QWidget* page)
{
    tabPages_[index] = page;
}

QWidget* TabMap::tabPage(int index) const
{
    return tabPages_.value(index, nullptr).data();
}
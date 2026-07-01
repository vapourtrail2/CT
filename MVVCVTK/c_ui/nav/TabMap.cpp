#include "c_ui/nav/TabMap.h"
#include <QWidget>

TabMap::TabMap()
{
}
//不管 tab 显示名字
//只负责 tab index->QWidget 页面映射

//tab 显示名字 RibbonPage::tabName()

bool TabMap::isFileTab(int index) const
{
    return index == TabIndex::File;
}

bool TabMap::isValidTab(int index) const
{
    return index >= TabIndex::File && index < TabIndex::Count;
}

void TabMap::bindTabPage(int index, QWidget* page)
{   
    tabPages_[index] = page;
}

QWidget* TabMap::tabPage(int index) const
{
    return tabPages_.value(index, nullptr).data();
}
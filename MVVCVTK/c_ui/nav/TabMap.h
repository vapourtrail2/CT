#pragma once

#include <QStringList>
#include <QHash>
#include <QPointer>

class QWidget;

//常量集中定义 好维护
namespace TabIndex
{
    constexpr int File = 0;
    constexpr int Start = 1;
    constexpr int Edit = 2;
    constexpr int Volume = 3;
    constexpr int Select = 4;
    constexpr int Align = 5;
    constexpr int Geometry = 6;
    constexpr int Measure = 7;
    constexpr int Cad = 8;
    constexpr int Analysis = 9;
    constexpr int Report = 10;
    constexpr int Animation = 11;
    constexpr int Window = 12;
    constexpr int Gauge = 13;
    constexpr int Count = 14;
}

class TabMap
{
public:
    TabMap();

    const QStringList& tabNames() const;
    bool isFileTab(int index) const;
    bool isValidTab(int index) const;

    void bindTabPage(int index, QWidget* page);
    QWidget* tabPage(int index) const;

private:
    QStringList tabNames_;
    QHash<int, QPointer<QWidget>> tabPages_;
};
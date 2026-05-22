#include "CADAndThen.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/CadIconMap.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QPixmap>
#include <QList>
#include <QSize>
#include <QDebug>
#include <QFile>

static QIcon loadIconFor(const QString& text) {
    return RibbonCommon::loadIconByText(text, IconMaps09::kCadIconMap);
}

CADAndThen::CADAndThen(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("CADEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#CADEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout07 = new QVBoxLayout(this);
    layout07->setContentsMargins(0, 0, 0, 0);
    layout07->setSpacing(3);

    // 功能区调用
    layout07->addWidget(buildRibbon07(this));
}

QWidget* CADAndThen::buildRibbon07(QWidget* parent)
{
    // 创建功能区容器
    auto* ribbon07 = new QFrame(parent);
    ribbon07->setObjectName(QStringLiteral("CADRibbon"));
    ribbon07->setStyleSheet(QStringLiteral(
        "QFrame#CADRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout07 = new QHBoxLayout(ribbon07);
    layout07->setContentsMargins(4, 4, 4, 4);
    layout07->setSpacing(1);

    struct RibbonAction07
    {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction07> actions07 = {
        { QStringLiteral("简化表面网格"), 0 },
        { QStringLiteral("删除孤立的分量"), 0 },
        { QStringLiteral("翻转表面方向"), 0 },
        { QStringLiteral("重新计算CAD网格"), 0 },
        { QStringLiteral("合并表面网格对象"), 0 },
        { QStringLiteral("变形网格"), 0 },
        { QStringLiteral("模具修正"), 0 },
        { QStringLiteral("补偿网格"), 0 },
        { QStringLiteral("迭代补偿网格"), 0 },
        { QStringLiteral("变形场"), 0 }
    };

    for (const auto& action : actions07) {
        auto* button = new QToolButton(ribbon07);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        QString afterShiftText = RibbonCommon::shiftNewLine(action.text, button->font(), 51);
        button->setText(afterShiftText);
        button->setIcon(loadIconFor(action.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumSize(QSize(70, 90));
        layout07->addWidget(button);
    }
    layout07->addStretch();
    return ribbon07;
}


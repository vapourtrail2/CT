#include "AlignmentPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/AlignIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps06::kAlignIconMap);
}

AlignmentPage::AlignmentPage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("alignmentEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#alignmentEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout04 = new QVBoxLayout(this);
    layout04->setContentsMargins(0, 0, 0, 0);
    layout04->setSpacing(3);

    // 功能区调用
    layout04->addWidget(buildRibbon04(this));
}

QWidget* AlignmentPage::buildRibbon04(QWidget* parent)
{
    auto* ribbon04 = new QFrame(parent);
    ribbon04->setObjectName(QStringLiteral("alignmentRibbon"));
    ribbon04->setStyleSheet(QStringLiteral(
        "QFrame#alignmentRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout04 = new QHBoxLayout(ribbon04);
    layout04->setContentsMargins(4, 4, 4, 4);
    layout04->setSpacing(1);

    struct RibbonAction04 {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction04> actions04 = {
        { QStringLiteral("最佳拟合对齐"), 0 },
        { QStringLiteral("3-2-1对齐"), 0 },
        { QStringLiteral("基于特征的对齐"),0 },
        { QStringLiteral("按次序对齐"), 0 },
        { QStringLiteral("RPS对齐"), 0 },
        { QStringLiteral("基于几何元素的拟合"), 0 },
        { QStringLiteral("编辑当前对齐"), 0 },
        { QStringLiteral("简单3-2-1对齐"), 0 },
        { QStringLiteral("简单对齐"), 0 },
        { QStringLiteral("将切片图对齐到对象"), 0 },
        { QStringLiteral("坐标系原点"), 0 },
        { QStringLiteral("坐标系编辑器"), 0 },
        { QStringLiteral("存储对齐"), 0 },
        { QStringLiteral("应用对齐"), 0 },
        { QStringLiteral("复制转换"), 0 },
        { QStringLiteral("粘贴转换"), 0 },
        { QStringLiteral("重置转换"), 0 },
        { QStringLiteral("锁定"), 0 },
        { QStringLiteral("解锁"), 0 },
    };
    for (const auto& action : actions04)
    {
        auto* button = new QToolButton(ribbon04);
        button->setIcon(loadIconFor(action.text));
     /*   button->setIconSize(QSize(40, 40));*/
        button->setMinimumSize(QSize(70, 90));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        button->setText(RibbonCommon::shiftNewLine(action.text, button->font(), 60));
        layout04->addWidget(button);
    }
    layout04->addStretch();
    return ribbon04;
}

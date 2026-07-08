#include "AlignmentPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/RibbonIconMaps.h"
#include "c_ui/nav/TabMap.h"
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
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kAlignIconMap);
}

namespace
{
constexpr int kButtonTextMaxWidth = 60;
constexpr int kButtonIconSize = 40;
constexpr int kButtonMinWidth = 70;
constexpr int kButtonMinHeight = 90;

const char* kMenuStyle =
"QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
"QMenu::item{color:#e0e0e0; padding:6px 24px;}"
"QMenu::item:selected{background:#3a3a3a;}";

const char* kRibbonStyle =
"QFrame#alignmentRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:600;}";
}

QList<RibbonDef::RibbonButtonDef> AlignmentPage::createAlignButtons()
{
    return {
        { QStringLiteral("最佳拟合对齐"), {} },
        { QStringLiteral("3-2-1对齐"), {} },
        { QStringLiteral("基于特征的对齐"), {} },
        { QStringLiteral("按次序对齐"), {} },
        { QStringLiteral("RPS对齐"), {} },
        { QStringLiteral("基于几何元素的拟合"), {} },
        { QStringLiteral("编辑当前对齐"), {} },
        { QStringLiteral("简单3-2-1对齐"), {} },
        { QStringLiteral("简单对齐"), {} },
        { QStringLiteral("将切片图对齐到对象"), {} },
        { QStringLiteral("坐标系原点"), {} },
        { QStringLiteral("坐标系编辑器"), {} },
        { QStringLiteral("存储对齐"), {} },
        { QStringLiteral("应用对齐"), {} },
        { QStringLiteral("复制转换"), {} },
        { QStringLiteral("粘贴转换"), {} },
        { QStringLiteral("重置转换"), {} },
        { QStringLiteral("锁定"), {} },
        { QStringLiteral("解锁"), {} },
    };
}

AlignmentPage::AlignmentPage(QWidget* parent)
    : RibbonPage(parent)
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

int AlignmentPage::tabIndex() const
{
    return TabIndex::Align;
}

QString AlignmentPage::tabName() const
{
    return QStringLiteral("对齐");
}

QWidget* AlignmentPage::buildRibbon04(QWidget* parent)
{
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("alignmentRibbon"),
        kRibbonStyle,
        createAlignButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
        });
}

QMenu* AlignmentPage::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [](const QString&) {});
}

QToolButton* AlignmentPage::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
{
    return RibbonCommon::createRibbonButton(
        parent,
        buttonDef,
        loadIconFor,
        [this](QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions) {
            return createMenu(parent, menuActions);
        },
        kButtonTextMaxWidth,
        kButtonIconSize,
        kButtonMinWidth,
        kButtonMinHeight);
}

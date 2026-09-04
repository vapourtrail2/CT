#include "CADAndThen.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/RibbonIconMaps.h"
#include "c_ui/command/commands.h"
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
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kCadIconMap);
}

namespace
{
constexpr int kButtonTextMaxWidth = 51;
constexpr int kButtonIconSize = 40;
constexpr int kButtonMinWidth = 70;
constexpr int kButtonMinHeight = 90;

const char* kMenuStyle =
"QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
"QMenu::item{color:#e0e0e0; padding:6px 24px;}"
"QMenu::item:selected{background:#3a3a3a;}";

const char* kRibbonStyle =
"QFrame#CADRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:400;}";
}

QList<RibbonDef::RibbonButtonDef> CADAndThen::createCadButtons()
{
    return {
        { QStringLiteral("简化表面网格"), {} },
        { QStringLiteral("删除孤立的分量"), {} },
        { QStringLiteral("翻转表面方向"), {} },
        { QStringLiteral("重新计算CAD网格"), {} },
        { QStringLiteral("合并表面网格对象"), {} },
        { QStringLiteral("变形网格"), {} },
        { QStringLiteral("模具修正"), {} },
        { QStringLiteral("补偿网格"), {} },
        { QStringLiteral("迭代补偿网格"), {} },
        { QStringLiteral("变形场"), {} }
    };
}

CADAndThen::CADAndThen(QWidget* parent)
    : RibbonPage(parent)
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

int CADAndThen::tabIndex() const
{
    return TabIndex::Cad;
}

QString CADAndThen::tabName() const
{
    return QStringLiteral("CAD/表面测量");
}

QWidget* CADAndThen::buildRibbon07(QWidget* parent)
{
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("CADRibbon"),
        kRibbonStyle,
        createCadButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
        });
}

QMenu* CADAndThen::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [](const QString&) {});
}

QToolButton* CADAndThen::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
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

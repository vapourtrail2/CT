#include "EditPage.h"
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
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kEditIconMap);
}

namespace
{
constexpr int kButtonTextMaxWidth = 70;
constexpr int kButtonIconSize = 40;
constexpr int kButtonMinWidth = 70;
constexpr int kButtonMinHeight = 90;

const char* kMenuStyle =
"QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
"QMenu::item{color:#e0e0e0; padding:6px 24px;}"
"QMenu::item:selected{background:#3a3a3a;}";

const char* kRibbonStyle =
"QFrame#editRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:600;}";
}

QList<RibbonDef::RibbonButtonDef> EditPage::createEditButtons()
{
    return {
        { QStringLiteral("撤销"), {} },
        { QStringLiteral("重做"), {} },
        { QStringLiteral("释放内存/清除撤销队列"), {} },
        { QStringLiteral("剪切"), {} },
        { QStringLiteral("复制"), {} },
        { QStringLiteral("粘贴"), {} },
        { QStringLiteral("删除"), {} },
        { QStringLiteral("创建对象组"), {} },
        { QStringLiteral("取消对象组"), {} },
        {
            QStringLiteral("转换为"),
            {
                { QStringLiteral("体积"), QStringLiteral(":/icons/icons/trans_pull_down_menu/volume.png"), QString() },
                { QStringLiteral("四面体体积网格"), QStringLiteral(":/icons/icons/trans_pull_down_menu/volume_grid.png"), QString() },
                { QStringLiteral("表面网格"), QStringLiteral(":/icons/icons/trans_pull_down_menu/surface_grid.png"), QString() },
                { QStringLiteral("CAD"), QStringLiteral(":/icons/icons/trans_pull_down_menu/CAD.png"), QString() },
                { QStringLiteral("黄金表面"), QStringLiteral(":/icons/icons/trans_pull_down_menu/golden_surface.png"), QString() },
                { QStringLiteral("分析结果中的有色表面网格"), QStringLiteral(":/icons/icons/trans_pull_down_menu/analysis_surface.png"), QString() },
                { QStringLiteral("来自四面体体积网格的集成网格"), QStringLiteral(":/icons/icons/trans_pull_down_menu/integration_grid.png"), QString() }
            }
        },
        { QStringLiteral("属性"), {} },
        { QStringLiteral("旋转"), {} },
        { QStringLiteral("移动"), {} },
        { QStringLiteral("复制可视状态"), {} },
        { QStringLiteral("粘贴可视状态"), {} },
        { QStringLiteral("复制元信息"), {} },
        { QStringLiteral("粘贴元信息"), {} },
        { QStringLiteral("动态重命名"), {} }
    };
}

EditPage::EditPage(QWidget* parent)
    : RibbonPage(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("pageEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(3);

    // 功能区调用
    layout->addWidget(buildRibbon(this));
}

int EditPage::tabIndex() const
{
    return TabIndex::Edit;
}

QString EditPage::tabName() const
{
    return QStringLiteral("编辑");
}

QWidget* EditPage::buildRibbon(QWidget* parent)
{
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("editRibbon"),
        kRibbonStyle,
        createEditButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
        });
}

QMenu* EditPage::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [](const QString&) {});
}

QToolButton* EditPage::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
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

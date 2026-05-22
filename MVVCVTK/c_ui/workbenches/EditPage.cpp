#include "EditPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/EditIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps02::kEditIconMap);
}

EditPage::EditPage(QWidget* parent)
    : QWidget(parent)
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

QWidget* EditPage::buildRibbon(QWidget* parent)
{
    // 创建功能区容器
    auto* ribbon = new QFrame(parent);
    ribbon->setObjectName(QStringLiteral("editRibbon"));
    ribbon->setStyleSheet(QStringLiteral(
        "QFrame#editRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(1);

    struct RibbonAction
    {
        QString text;
        bool hasMenu;
    };

    const QList<RibbonAction> actions = {
        { QStringLiteral("撤销"), false },
        { QStringLiteral("重做"), false },
        { QStringLiteral("释放内存/清除撤销队列"), false },
        { QStringLiteral("剪切"), false },
        { QStringLiteral("复制"), false },
        { QStringLiteral("粘贴"), false },
        { QStringLiteral("删除"), false },
        { QStringLiteral("创建对象组"), false },
        { QStringLiteral("取消对象组"), false },
        { QStringLiteral("转换为"), true },
        { QStringLiteral("属性"), false },
        { QStringLiteral("旋转"), false },
        { QStringLiteral("移动"), false },
        { QStringLiteral("复制可视状态"), false },
        { QStringLiteral("粘贴可视状态"), false },
        { QStringLiteral("复制元信息"), false },
        { QStringLiteral("粘贴元信息"), false },
        { QStringLiteral("动态重命名"), false }
    };

	for (const auto& action : actions) {
        // 每个功能都使用图标,文字的形式展示
        auto* button = new QToolButton(ribbon); 
        QString afterShiftText = RibbonCommon::shiftNewLine(action.text, button->font(), 70,1.2);
        button->setText(afterShiftText);
        button->setIcon(loadIconFor(action.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumSize(QSize(70, 90));

        if (action.hasMenu) {
            // 转换为功能 需要后期拓展
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
				"QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/volume.png"), QStringLiteral("体积"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/volume_grid.png"), QStringLiteral("四面体体积网格"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/surface_grid.png"), QStringLiteral("表面网格"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/CAD.png"), QStringLiteral("CAD"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/golden_surface.png"), QStringLiteral("黄金表面"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/analysis_surface.png"), QStringLiteral("分析结果中的有色表面网格"));
            menu->addAction(QIcon(":/icons/icons/trans_pull_down_menu/integration_grid.png"), QStringLiteral("来自四面体体积网格的集成网格"));
            button->setMenu(menu);
			button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        layout->addWidget(button);
    }
    layout->addStretch();
    return ribbon;
}
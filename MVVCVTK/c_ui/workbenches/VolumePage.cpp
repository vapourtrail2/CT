#include "VolumePage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/VolumeIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps04::kVolumeIconMap);
}

VolumePage::VolumePage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("volumeEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout02 = new QVBoxLayout(this);
    layout02->setContentsMargins(0, 0, 0, 0);
    layout02->setSpacing(3);

    // 功能区调用
    layout02->addWidget(buildRibbon02(this));
}

QWidget* VolumePage::buildRibbon02(QWidget* parent)
{
    // 创建功能区容器
    auto* ribbon02 = new QFrame(parent);
    ribbon02->setObjectName(QStringLiteral("volumeRibbon"));
    ribbon02->setStyleSheet(QStringLiteral(
        "QFrame#volumeRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout02 = new QHBoxLayout(ribbon02);
    layout02->setContentsMargins(4, 4, 4, 4);
    layout02->setSpacing(1);

    struct RibbonAction02
    {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction02> actions02 = {
        { QStringLiteral("拆分体积"), 0 },
        { QStringLiteral("表面测定"), 1 },
        { QStringLiteral("删除表面测定"), 0 },
        { QStringLiteral("体积数据"), 2 },
        { QStringLiteral("基于特征的缩放"), 0 },
        { QStringLiteral("手动缩放"), 0 },
        { QStringLiteral("绘制数据"), 0 },
        { QStringLiteral("选择颜色"), 0 },
        { QStringLiteral("填充"), 0 },
        { QStringLiteral("自适应高斯"), 0 },
        { QStringLiteral("非局部均值"), 0 },
        { QStringLiteral("卷积"), 0 },
        { QStringLiteral("高斯"), 0 },
        { QStringLiteral("框"), 0 },
        { QStringLiteral("偏差"), 0 },
        { QStringLiteral("中值"), 0 },
        { QStringLiteral("侵蚀"), 0 },
        { QStringLiteral("膨胀"), 0 },
        { QStringLiteral("应用不透明映射"), 0 },
        { QStringLiteral("FIB-SEM 修正"), 0 },
        { QStringLiteral("合并和重新采样"), 0 },
        { QStringLiteral("体积投影器"), 0 }
    };


    for (const auto& action : actions02) {
        // 每个功能都使用图标,文字的形式展示
        auto* button = new QToolButton(ribbon02);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        QString afterShiftText = RibbonCommon::shiftNewLine(action.text, button->font(), 51);
        button->setText(afterShiftText);
        button->setIcon(loadIconFor(action.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumSize(QSize(70, 90));


        if (action.hasMenu == 1) {
            // 转换为功能 需要后期拓展
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/surface_measure_and_based_on_iosvalue.png"), QStringLiteral("基于等值"));
            menu->addAction(QIcon(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/advanced_classic.png"), QStringLiteral("高级(经典)"));
            menu->addAction(QIcon(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/advanced_multi_material.png"), QStringLiteral("高级(多材料)"));
            menu->addAction(QIcon(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/fixed_contour.png"), QStringLiteral("固定轮廓"));
            menu->addAction(QIcon(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/edit_surface_measure.png"), QStringLiteral("编辑表面测定"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 2) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/volume_icons_2/icons_other/volume_icons/volume_data_pull_down_menu/volume_data.png"), QStringLiteral("体积数据"));
            menu02->addAction(QIcon(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/create_synthetic_volume_data.png"), QStringLiteral("创建合成体积数据"));
            menu02->addAction(QIcon(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/delete_volume_data.png"), QStringLiteral("删除体积数据"));
            menu02->addAction(QIcon(":/volume_icons_2/icons_other/volume_icons/volume_data_pull_down_menu/uninstall_volume_data.png"), QStringLiteral("卸载体积数据"));
            menu02->addAction(QIcon(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/reload_volume_data.png"), QStringLiteral("重新加载体积数据"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        layout02->addWidget(button);
    }
    layout02->addStretch();
    return ribbon02;
}
#include "VolumePage.h"
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
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kVolumeIconMap);
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
"QFrame#volumeRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:400;}";
}

QList<RibbonDef::RibbonButtonDef> VolumePage::createVolumeButtons()
{
    return {
        { QStringLiteral("拆分体积"), {} },
        {
            QStringLiteral("表面测定"),
            {
                { QStringLiteral("基于等值"), QStringLiteral(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/surface_measure_and_based_on_iosvalue.png"), QString() },
                { QStringLiteral("高级(经典)"), QStringLiteral(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/advanced_classic.png"), QString() },
                { QStringLiteral("高级(多材料)"), QStringLiteral(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/advanced_multi_material.png"), QString() },
                { QStringLiteral("固定轮廓"), QStringLiteral(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/fixed_contour.png"), QString() },
                { QStringLiteral("编辑表面测定"), QStringLiteral(":/volume_icons/icons_other/volume_icons/surface_measure_pull_down_menu/edit_surface_measure.png"), QString() }
            }
        },
        { QStringLiteral("删除表面测定"), {} },
        {
            QStringLiteral("体积数据"),
            {
                { QStringLiteral("体积数据"), QStringLiteral(":/volume_icons_2/icons_other/volume_icons/volume_data_pull_down_menu/volume_data.png"), QString() },
                { QStringLiteral("创建合成体积数据"), QStringLiteral(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/create_synthetic_volume_data.png"), QString() },
                { QStringLiteral("删除体积数据"), QStringLiteral(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/delete_volume_data.png"), QString() },
                { QStringLiteral("卸载体积数据"), QStringLiteral(":/volume_icons_2/icons_other/volume_icons/volume_data_pull_down_menu/uninstall_volume_data.png"), QString() },
                { QStringLiteral("重新加载体积数据"), QStringLiteral(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/reload_volume_data.png"), QString() }
            }
        },
        { QStringLiteral("基于特征的缩放"), {} },
        { QStringLiteral("手动缩放"), {} },
        { QStringLiteral("绘制数据"), {} },
        { QStringLiteral("选择颜色"), {} },
        { QStringLiteral("填充"), {} },
        { QStringLiteral("自适应高斯"), {} },
        { QStringLiteral("非局部均值"), {} },
        { QStringLiteral("卷积"), {} },
        { QStringLiteral("高斯"), {} },
        { QStringLiteral("框"), {} },
        { QStringLiteral("偏差"), {} },
        { QStringLiteral("中值"), {} },
        { QStringLiteral("侵蚀"), {} },
        { QStringLiteral("膨胀"), {} },
        { QStringLiteral("应用不透明映射"), {} },
        { QStringLiteral("FIB-SEM 修正"), {} },
        { QStringLiteral("合并和重新采样"), {} },
        { QStringLiteral("体积投影器"), {} }
    };
}

VolumePage::VolumePage(QWidget* parent)
    : RibbonPage(parent)
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

int VolumePage::tabIndex() const
{
    return TabIndex::Volume;
}

QString VolumePage::tabName() const
{
    return QStringLiteral("体积");
}

QWidget* VolumePage::buildRibbon02(QWidget* parent)
{
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("volumeRibbon"),
        kRibbonStyle,
        createVolumeButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
        });
}

QMenu* VolumePage::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [](const QString&) {});
}

QToolButton* VolumePage::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
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

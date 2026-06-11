#include "StartPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/StartIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps::kStartIconMap);
}

StartPagePage::StartPagePage(QWidget* parent)
    :RibbonPage(parent)//在创建StartPagePage之前 必须先创建他的父类RibbonPage 并把parent传给父类
{
    // 设置页面外观
    setObjectName(QStringLiteral("pageStart"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageStart{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout01 = new QVBoxLayout(this);
    layout01->setContentsMargins(0, 0, 0, 0);
    layout01->setSpacing(3);

    //布局调用
    layout01->addWidget(buildRibbon01(this));
}

int StartPagePage::tabIndex() const
{
    return TabIndex::Start;
}

QString StartPagePage::tabName() const
{
    return QStringLiteral("开始");
}

QWidget* StartPagePage::buildRibbon01(QWidget* parent)
{
    // 创建功能区容器
    auto* ribbon01_ = new QFrame(parent);
    ribbon01_->setObjectName(QStringLiteral("startRibbon"));
    ribbon01_->setStyleSheet(QStringLiteral(
        "QFrame#startRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout01 = new QHBoxLayout(ribbon01_);
    layout01->setContentsMargins(4, 4, 4, 4);
    layout01->setSpacing(1);

    struct RibbonAction01
    {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction01> actions01 = {
        { QStringLiteral("快速导入"), 0 },
        { QStringLiteral("体积导入"), 1 },
        { QStringLiteral("显示模式"), 2 },
        { QStringLiteral("水平/窗口模式"), 0 },
        { QStringLiteral("厚板"), 0 },
        { QStringLiteral("裁剪当前切片图"), 0 },
        { QStringLiteral("对齐"), 3 },
        { QStringLiteral("指示器"), 0 },
        { QStringLiteral("距离"), 0 },
        { QStringLiteral("角度(4个点)"), 0 },
        { QStringLiteral("角度(3个点)"), 0 },
        { QStringLiteral("折线长度"), 0 },
        { QStringLiteral("最小/最大距离"), 0 },
        { QStringLiteral("卡尺"), 0 },
        { QStringLiteral("捕捉模式"), 4 },
        { QStringLiteral("重新捕捉量具控点"), 0 },
        { QStringLiteral("创建报告"), 0 },
        { QStringLiteral("创建书签"), 0 },
        { QStringLiteral("书签编辑器"), 0 },
        { QStringLiteral("保存图像/影片"), 5 },
    };

    for (const auto& action : actions01) {
        // 每个功能都使用图标,文字的形式展示
        auto* button = new QToolButton(ribbon01_);
        // 换行规则统一到公共函数，后续调规则只改一处。
        QString afterShiftText = RibbonCommon::shiftNewLine(action.text, button->font(), 51);
        button->setText(afterShiftText);
        button->setIcon(loadIconFor(action.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumSize(QSize(70, 90));

        if (action.text == QStringLiteral("距离")) {
            connect(button, &QToolButton::clicked, this, [this]() {
                emit distanceRequested();
                });
        }

        if (action.text == QStringLiteral("角度(3个点)")) {
            connect(button, &QToolButton::clicked, this, [this]() {
                emit angleRequested();
                });
        }

        if (action.hasMenu == 1) {  
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/origin_volume.png"), QStringLiteral("原始体积"));
            menu->addAction(QIcon(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/images_stack.png"), QStringLiteral("图像堆栈"));
            menu->addAction(QIcon(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/merge_obj.png"), QStringLiteral("合并对象"));
            auto* actCtRecon = menu->addAction(
                QIcon(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/CT_rebuild.png"),
                QStringLiteral("CT重建"));

            connect(actCtRecon, &QAction::triggered, this, [this]() {
                emit ctReconRequested();
                });
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 2) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/display_pattern.png"), QStringLiteral("原始"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/color.png"), QStringLiteral("颜色"));
            menu02->addAction(QIcon(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/delete_volume_data.png"), QStringLiteral("颜色和不透明度"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 3) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/best_fit_align.png"), QStringLiteral("最佳拟合对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/3-2-1_align.png"), QStringLiteral("3-2-1对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/based_on_feature_align.png"), QStringLiteral("基于特征的对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/in_order_align.png"), QStringLiteral("按次序对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/RPS_align.png"), QStringLiteral("RPS对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/based_on_geometry_element_best_fit.png"), QStringLiteral("基于几何元素的最佳拟合"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/simple_3-2-1_align.png"), QStringLiteral("简单3-2-1对齐"));
            menu02->addAction(QIcon(":/start_icons02/icons_other/start_icons/align_pull_down_menu/simple_align.png"), QStringLiteral("简单对齐"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 4) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/min.PNG"), QStringLiteral("最小"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/max.PNG"), QStringLiteral("最大"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/gradient.PNG"), QStringLiteral("梯度"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/surface.PNG"), QStringLiteral("表面"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/local.PNG"), QStringLiteral("局部"));
            menu02->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/off.PNG"), QStringLiteral("关"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 5) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));

            auto* actSaveTransformedData = menu02->addAction(
                QIcon(":/start_icons01/icons_other/start_icons/save_image.png"),
                QStringLiteral("保存图像"));

            connect(actSaveTransformedData, &QAction::triggered, this, [this]() {
                emit transformedDataSaveRequested();
                });

            auto* actSaveSliceStack = menu02->addAction(
                QIcon(":/start_icons01/icons_other/start_icons/save_image.png"),
                QStringLiteral("保存影片/图像堆栈"));

            connect(actSaveSliceStack, &QAction::triggered, this, [this]() {
                emit sliceStackSaveRequested();
                });
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        layout01->addWidget(button);
    }
    layout01->addStretch();
    return ribbon01_;
}

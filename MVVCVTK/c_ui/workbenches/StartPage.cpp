#include "c_ui/nav/TabMap.h"
#include "c_ui/workbenches/common/IconMaps/StartIconMap.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "StartPage.h"
#include <QFrame>
#include <QHBoxLayout>
#include <QList>
#include <QMenu>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>
#include <QAction>
#include <qdebug.h>

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

    struct PullMeauAction
    {
        QString text;
		QString iconPath;
		QString command;
    };

    struct RibbonButtonDef
    {
        QString text;
        QString command;
		QList<PullMeauAction> menuActions;
    };

    const QList<RibbonButtonDef> buttons = {
    { QStringLiteral("快速导入"), QString(),  QList<PullMeauAction>{} },

    {
        QStringLiteral("体积导入"), QString(), 
        QList<PullMeauAction>{
            { QStringLiteral("原始体积"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/origin_volume.png"), QString() },
            { QStringLiteral("图像堆栈"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/images_stack.png"), QString() },
            { QStringLiteral("合并对象"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/merge_obj.png"), QString() },
            { QStringLiteral("CT重建"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/CT_rebuild.png"), QStringLiteral("recon.open") }
        }
    },

    {
        QStringLiteral("显示模式"), QString(), 
        QList<PullMeauAction>{
            { QStringLiteral("原始"), QStringLiteral(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/display_pattern.png"), QString() },
            { QStringLiteral("颜色"), QStringLiteral(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/color.png"), QString() },
            { QStringLiteral("颜色和不透明度"), QStringLiteral(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/delete_volume_data.png"), QString() }
        }
    },

    { QStringLiteral("水平/窗口模式"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("厚板"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("裁剪当前切片图"), QString(), QList<PullMeauAction>{} },

    {
        QStringLiteral("对齐"), QString(), 
        QList<PullMeauAction>{
            { QStringLiteral("最佳拟合对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/best_fit_align.png"), QString() },
            { QStringLiteral("3-2-1对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/3-2-1_align.png"), QString() },
            { QStringLiteral("基于特征的对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/based_on_feature_align.png"), QString() },
            { QStringLiteral("按次序对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/in_order_align.png"), QString() },
            { QStringLiteral("RPS对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/RPS_align.png"), QString() },
            { QStringLiteral("基于几何元素的最佳拟合"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/based_on_geometry_element_best_fit.png"), QString() },
            { QStringLiteral("简单3-2-1对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/simple_3-2-1_align.png"), QString() },
            { QStringLiteral("简单对齐"), QStringLiteral(":/start_icons02/icons_other/start_icons/align_pull_down_menu/simple_align.png"), QString() }
        }
    },

    { QStringLiteral("指示器"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("距离"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("角度(4个点)"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("角度(3个点)"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("折线长度"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("最小/最大距离"), QString(),  QList<PullMeauAction>{} },
    { QStringLiteral("卡尺"), QString(),  QList<PullMeauAction>{} },

    {
        QStringLiteral("捕捉模式"), QString(), 
        QList<PullMeauAction>{
            { QStringLiteral("最小"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/min.PNG"), QString() },
            { QStringLiteral("最大"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/max.PNG"), QString() },
            { QStringLiteral("梯度"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/gradient.PNG"), QString() },
            { QStringLiteral("表面"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/surface.PNG"), QString() },
            { QStringLiteral("局部"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/local.PNG"), QString() },
            { QStringLiteral("关"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/off.PNG"), QString() }
        }
    },

    { QStringLiteral("重新捕捉量具控点"), QString(), QList<PullMeauAction>{} },
    { QStringLiteral("创建报告"), QString(), QList<PullMeauAction>{} },
    { QStringLiteral("创建书签"), QString(), QList<PullMeauAction>{} },
    { QStringLiteral("书签编辑器"), QString(), QList<PullMeauAction>{} },

    {
        QStringLiteral("保存图像/影片"), QString(), 
        QList<PullMeauAction>{
            { QStringLiteral("保存图像"), QStringLiteral(":/start_icons01/icons_other/start_icons/save_image.png"), QStringLiteral("image.save") },
            { QStringLiteral("保存影片/图像堆栈"), QStringLiteral(":/start_icons01/icons_other/start_icons/save_image.png"), QStringLiteral("slicestack.save") }
        }
    }
    };

    auto createMenu = [this](QWidget* parent, const QList<PullMeauAction>& menuActions) {
        auto* menu = new QMenu(parent);
        menu->setStyleSheet(QStringLiteral(
            "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
            "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
            "QMenu::item:selected{background:#3a3a3a;}"));

        for (const auto& menuAction : menuActions) {
            auto* qAction = menu->addAction(
                QIcon(menuAction.iconPath),
                menuAction.text);

            if (!menuAction.command.isEmpty()) {
                connect(qAction, &QAction::triggered, this, [this, command = menuAction.command]() {
                    emit commandRequested(command);
                    });
            }
        }

        return menu;
        };

    auto createButton = [this, &createMenu](QWidget* parent, const RibbonButtonDef& buttonDef) {
        auto* button = new QToolButton(parent);
        QString afterShiftText = RibbonCommon::shiftNewLine(buttonDef.text, button->font(), 51);
        button->setText(afterShiftText);
        button->setIcon(loadIconFor(buttonDef.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumSize(QSize(70, 90));

        if (!buttonDef.menuActions.isEmpty()) {
            button->setMenu(createMenu(button, buttonDef.menuActions));
            button->setPopupMode(QToolButton::InstantPopup);
        }
         /*   else if (!buttonDef.command.isEmpty()) {
			qDebug() << "Connecting button '" << buttonDef.text << "' to command: " << buttonDef.command;
            connect(button, &QToolButton::clicked, this, [this, command = buttonDef.command]() {
                emit commandRequested(command);
                });
        }*/

        return button;
        };


    for (const auto& buttonDef : buttons) {
        layout01->addWidget(createButton(ribbon01_, buttonDef));
    }

    layout01->addStretch();
    return ribbon01_;
}

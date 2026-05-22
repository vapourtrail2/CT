#include "WindowPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/WindowIconMap.h"
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
#include <QCheckBox>
#include <QWidgetAction>

static QIcon loadIconFor(const QString& text) {
    return RibbonCommon::loadIconByText(text, IconMaps13::kWindowIconMap);
}

WindowPage::WindowPage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("windowEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageWindow{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout12 = new QVBoxLayout(this);
    layout12->setContentsMargins(0, 0, 0, 0);
    layout12->setSpacing(3);

    // 功能区调用
    layout12->addWidget(buildRibbon12(this));
}

QWidget* WindowPage::buildRibbon12(QWidget* parent)
{
    auto* ribbon12 = new QFrame(parent);//ribbon12是--功能区的容器
    ribbon12->setObjectName(QStringLiteral("measureRibbon"));
    ribbon12->setStyleSheet(QStringLiteral(
        "QFrame#measureRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout12 = new QHBoxLayout(ribbon12);
    layout12->setContentsMargins(4, 4, 4, 4);
    layout12->setSpacing(1);

    struct RibbonAction12 {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction12> actions12 = {
        { QStringLiteral("四分"), 0 },
        { QStringLiteral("切片图靠左"), 0 },
        { QStringLiteral("切片图靠右"), 0 },
        { QStringLiteral("切片图靠下"), 0 },
        { QStringLiteral("旋转"), 0 },
        { QStringLiteral("四分，分布图"), 0 },
        { QStringLiteral("非平面"), 0 },
        { QStringLiteral("布局编辑器"), 0 },
        { QStringLiteral("2D背景"), 0 },
        { QStringLiteral("3D背景"), 0 },
        { QStringLiteral("放大"), 0 },
        { QStringLiteral("缩小"), 0 },
        { QStringLiteral("体素分辨率"), 0 },
        { QStringLiteral("设置缩放比例"), 0 },
        { QStringLiteral("调整对象以适应窗口"), 0 },
        { QStringLiteral("调整场景以适应窗口"), 0 },
        { QStringLiteral("重置"), 0 },
        { QStringLiteral("显示模式"), 1 },
        { QStringLiteral("厚板"), 0 },
        { QStringLiteral("切片图步宽"), 0 },
        { QStringLiteral("水平/窗口模式"), 0 },
        { QStringLiteral("标尺"), 0 },
        { QStringLiteral("注解"), 2 },
        { QStringLiteral("文本叠加层编辑器"), 0 },
        { QStringLiteral("工具"), 3 },
        { QStringLiteral("工具停靠栏"), 4 },
        { QStringLiteral("重置工具"), 0 },
    };

    const QStringList twoRowGroup = {
         QStringLiteral("四分"), 
         QStringLiteral("切片图靠左"), 
         QStringLiteral("切片图靠右"),
         QStringLiteral("切片图靠下"),
         QStringLiteral("旋转"), 
         QStringLiteral("四分，分布图"),
         QStringLiteral("非平面"), 
    };


    QWidget* gridHolder_12 = nullptr;//这个指针的意思是 用来承载那个 2×4 的小方阵
    QGridLayout* grid_12 = nullptr;//这个指针是用来管理那个小方阵的布局
    int groupedCount_12 = 0;//记录已经放进小方阵的按钮数量

    for (const auto& action : actions12)
    {
        const bool inGroup_12 = twoRowGroup.contains(action.text);//contains函数检查某个元素是否在列表中 返回true或false
        auto* button = new QToolButton(ribbon12);
        button->setIcon(loadIconFor(action.text));
        button->setIconSize(QSize(32, 32));
        button->setMinimumSize(QSize(59, 90));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setText(RibbonCommon::shiftNewLine(action.text, button->font(), 43));
        if (inGroup_12)
        {
            if (!gridHolder_12) {//等价于gridholder == nullptr
                gridHolder_12 = new QWidget(ribbon12);
                grid_12 = new QGridLayout(gridHolder_12);
                grid_12->setContentsMargins(4, 2, 4, 2);
                grid_12->setHorizontalSpacing(8);//水平间距
                grid_12->setVerticalSpacing(4);//垂直间距
                layout12->addWidget(gridHolder_12); // 把小方阵插入到主 ribbon
            }
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(20, 20));           // 小 icon
            button->setMinimumSize(QSize(90, 20));
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            button->setText(action.text);

            int row = groupedCount_12 / 4;
            int col = groupedCount_12 % 4;
            grid_12->addWidget(button, row, col);//三个参数的意思是：要添加的控件、行号、列号
            ++groupedCount_12;
            continue;
        
        }

        else {
            // 非该分组：仍旧一行横排
            layout12->addWidget(button);
        }

        if (action.hasMenu == 1) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/display_pattern_pull_down_menu/origin.png"), QStringLiteral("原始"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/display_pattern_pull_down_menu/color.png"), QStringLiteral("颜色"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/display_pattern_pull_down_menu/color_and_transparency.png"), QStringLiteral("颜色和不透明度"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 2) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/annotation_pull_down_menu/sort.png"), QStringLiteral("排列"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/annotation_pull_down_menu/min.png"), QStringLiteral("最小化"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/annotation_pull_down_menu/max.png"), QStringLiteral("最大化"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/annotation_pull_down_menu/set.png"), QStringLiteral("配置"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 3) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("渲染"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("光源"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("摄像机"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("裁剪"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("自动化"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("场景树"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("动画"));
            menu->addAction(QIcon(":/window_icons02/icons_other/window_icons/tool_pull_down_menu/same.PNG"), QStringLiteral("书签"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 4) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QWidget{color:#e0e0e0;}"
                "QCheckBox{padding:6px 24px;}"));

            auto addCheck = [&](const QString& text, bool checked,std::function<void(bool)> onToggled)
            {//onToggled：用户勾/取消时要做的事（回调）
                    auto* cb = new QCheckBox(text, menu);//checkbox控件
                    cb->setChecked(checked);
                    cb->setAttribute(Qt::WA_Hover, true);// 开启悬停事件
					auto* wa = new QWidgetAction(menu);//菜单默认不能放控件  菜单能放QAction  所以用QWidgetAction包装一下
                    //能把QCheckbox显示到菜单栏里
                    wa->setDefaultWidget(cb);
                    menu->addAction(wa);
                    QObject::connect(cb, &QCheckBox::toggled, menu, [=](bool on) {
                        if (onToggled)
                        {
                            onToggled(on);
                        }
                    });
            };

            addCheck(QStringLiteral("显示/隐藏右侧工具停靠栏"), true, [=](bool on) {
                // rightDock->setVisible(on);
                // emit requestToggleDock(Qt::RightDockWidgetArea, on); // 如果你走信号
                });
            addCheck(QStringLiteral("显示/隐藏左侧工具停靠栏"), true, [=](bool on) {
                // leftDock->setVisible(on);
                });
            addCheck(QStringLiteral("显示/隐藏下方工具停靠栏"), false, [=](bool on) {
                // bottomDock->setVisible(on);
                });
            addCheck(QStringLiteral("显示/隐藏上方工具停靠栏"), false, [=](bool on) {
                // topDock->setVisible(on);
                });

            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
    }
    layout12->addStretch();
    return ribbon12;
}

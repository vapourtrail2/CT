#include "AnimationPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/AniIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps12::kAniIconMap);
}

AnimationPage::AnimationPage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("animationEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageAnimation{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout10 = new QVBoxLayout(this);
    layout10->setContentsMargins(0, 0, 0, 0);
    layout10->setSpacing(3);

    // 功能区调用
    layout10->addWidget(buildRibbon11(this));
}

QWidget* AnimationPage::buildRibbon11(QWidget* parent)
{
    auto* ribbon06 = new QFrame(parent);//ribbon03是--功能区的容器
    ribbon06->setObjectName(QStringLiteral("animationRibbon"));
    ribbon06->setStyleSheet(QStringLiteral(
        "QFrame#animationRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout10 = new QHBoxLayout(ribbon06);
    layout10->setContentsMargins(4, 4, 4, 4);
    layout10->setSpacing(1);

    struct RibbonAction06 {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction06> actions06 = {
        { QStringLiteral("尺寸标注"), 0 },
        { QStringLiteral("线性尺寸"), 0 },
        //这个也一列n个
        { QStringLiteral("直线度"), 0 },
        { QStringLiteral("平面度"), 0 },
        { QStringLiteral("圆度"), 0 },
        { QStringLiteral("圆柱度"), 0 },
        { QStringLiteral("圆锥度"), 0 },
        { QStringLiteral("球度"), 0 },
        { QStringLiteral("垂直度"), 0 },
        { QStringLiteral("平行度"), 0 },
        { QStringLiteral("倾斜度"), 0 },
        { QStringLiteral("位置度"), 0 },
        { QStringLiteral("同轴度"), 0 },
        { QStringLiteral("对称度"), 0 },
        { QStringLiteral("线轮廓度"), 0 },
        { QStringLiteral("面轮廓度"), 0 },
        { QStringLiteral("圆跳动"), 0 },
        { QStringLiteral("全跳动"), 0 },

        { QStringLiteral("设置"), 1 },
        { QStringLiteral("指示器"), 0 },
        { QStringLiteral("量具"), 2 },
        { QStringLiteral("捕捉模式"), 3 },
        { QStringLiteral("重新捕捉量具控点"), 0 },
        { QStringLiteral("测量模板"), 4 },
        { QStringLiteral("量具模板"), 5 },
        { QStringLiteral("指示器模板"), 6 },
        { QStringLiteral("公差注解"), 0 },
        { QStringLiteral("CM结果"), 0 },
    };

    // 需要做成两行四列的那 8 个按钮
    const QStringList twoRowGroup = {
        QStringLiteral("直线度"),
        QStringLiteral("平面度"),
        QStringLiteral("圆度"),
        QStringLiteral("圆柱度"),
        QStringLiteral("圆锥度"),
        QStringLiteral("球度"),
        QStringLiteral("垂直度"),
        QStringLiteral("平行度"),
        QStringLiteral("倾斜度"),
        QStringLiteral("位置度"),
        QStringLiteral("同轴度"),
        QStringLiteral("对称度"),
        QStringLiteral("线轮廓度"),
        QStringLiteral("面轮廓度"),
        QStringLiteral("圆跳动"),
        QStringLiteral("全跳动"),
    };

    QWidget* gridHolder_06 = nullptr;//这个指针的意思是 用来承载那个 2×4 的小方阵
    QGridLayout* grid_06 = nullptr;//这个指针是用来管理那个小方阵的布局
    int groupedCount_06 = 0;//记录已经放进小方阵的按钮数量

    for (const auto& action : actions06)
    {
        const bool inGroup_06 = twoRowGroup.contains(action.text);//contains函数检查某个元素是否在列表中 返回true或false
        auto* button = new QToolButton(ribbon06);
        button->setIcon(loadIconFor(action.text));
        button->setIconSize(QSize(32, 32));
        button->setMinimumSize(QSize(59, 90));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        button->setText(RibbonCommon::shiftNewLine(action.text, button->font(), 43));
        if (inGroup_06)
        {
            if (!gridHolder_06) {//等价于gridholder == nullptr
                gridHolder_06 = new QWidget(ribbon06);
                grid_06 = new QGridLayout(gridHolder_06);
                grid_06->setContentsMargins(4, 2, 4, 2);
                grid_06->setHorizontalSpacing(8);//水平间距
                grid_06->setVerticalSpacing(4);//垂直间距
                layout10->addWidget(gridHolder_06); // 把小方阵插入到主 ribbon
            }
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(20, 20));           // 小 icon
            button->setMinimumSize(QSize(90, 20));
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            button->setText(action.text);

            int row = groupedCount_06 / 6;
            int col = groupedCount_06 % 6;
            grid_06->addWidget(button, row, col);//三个参数的意思是：要添加的控件、行号、列号
            ++groupedCount_06;
            continue;
        }

        else {
            // 非该分组：仍旧一行横排
            layout10->addWidget(button);
        }

        if (action.hasMenu == 1) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/option_pull_down_menu/regular_tolerance_preset.png"), QStringLiteral("常规公差预设"));
            menu->addAction(QIcon(":/measure_icons03/icons_other/measure_icons/option_pull_down_menu/tolerance_visible_setting.png"), QStringLiteral("公差可视化设置"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 2) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/distance.PNG"), QStringLiteral("距离"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/angle_4points.PNG"), QStringLiteral("角度(4个点)"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/angle_3points.PNG"), QStringLiteral("角度(3个点)"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/fold_line_length.PNG"), QStringLiteral("折线长度"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/max_min_distance.PNG"), QStringLiteral("最大/最小距离"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_pull_down_menu/caliper.PNG"), QStringLiteral("卡尺"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 3) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/min.PNG"), QStringLiteral("最小"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/max.PNG"), QStringLiteral("最大"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/gradient.PNG"), QStringLiteral("梯度"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/surface.PNG"), QStringLiteral("表面"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/local.PNG"), QStringLiteral("局部"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/off.PNG"), QStringLiteral("关"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 4)
        {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_template_pull_down_menu/input_measure_template.PNG"), QStringLiteral("导入测量模板"));
            menu->addAction(QIcon(":/measure_icons03/icons_other/measure_icons/measure_template_pull_down_menu/output_measure_template.png"), QStringLiteral("导出测量模板"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        if (action.hasMenu == 5)
        {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_template_pull_down_menu/intput_measure_tool_template.PNG"), QStringLiteral("导入量具模板"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/measure_tool_template_pull_down_menu/output_measure_tool_template.PNG"), QStringLiteral("导出量具模板"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
        else if (action.hasMenu == 6) {
            auto menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/indicator_template_pull_down_menu/input_indicator_template.PNG"), QStringLiteral("导入指示器模板"));
            menu->addAction(QIcon(":/new/prefix1/icons_other/measure_icons/indicator_template_pull_down_menu/output_indicator_template.PNG"), QStringLiteral("导出指示器模板"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
    }
    layout10->addStretch();
    return ribbon06;
}

#include "GeometryPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/GeoIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps07::kGeoIconMap);
}

GeometryPage::GeometryPage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("volumeEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout05 = new QVBoxLayout(this);
    layout05->setContentsMargins(0, 0, 0, 0);
    layout05->setSpacing(3);

    // 功能区调用
    layout05->addWidget(buildRibbon05(this));
}

QWidget* GeometryPage::buildRibbon05(QWidget* parent)
{
    auto* ribbon05 = new QFrame(parent);//ribbon0-是--功能区的容器
    ribbon05->setObjectName(QStringLiteral("geometryRibbon"));
    ribbon05->setStyleSheet(QStringLiteral(
        "QFrame#geometryRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout05 = new QHBoxLayout(ribbon05);
    layout05->setContentsMargins(4, 4, 4, 4);
    layout05->setSpacing(1);

    struct RibbonAction05 {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction05> actions05 = {
        { QStringLiteral("自动"), 0 },

        // 下面这段要放成两行×四列（图标左、文字右）
        { QStringLiteral("点"), 0 },
        { QStringLiteral("线"), 0 },
        { QStringLiteral("圆"), 0 },
        { QStringLiteral("角圆"), 0 },
        { QStringLiteral("平面"), 0 },
        { QStringLiteral("球体"), 0 },
        { QStringLiteral("圆柱"), 0 },
        { QStringLiteral("圆锥"), 0 },
        { QStringLiteral("环面"), 0 },
        { QStringLiteral("自由造型线"), 0 },
        { QStringLiteral("自由造型表面"), 0 },

        // 其余继续一行横排
        { QStringLiteral("定义几何元素"), 0 },
        { QStringLiteral("显示拟合点"), 0 },
        { QStringLiteral("隐藏拟合点"), 0 },
        { QStringLiteral("切换拟合点可见性"), 0 },
        { QStringLiteral("更新CM对象"), 0 },
        { QStringLiteral("删除未使用的CM对象"), 0 },
        { QStringLiteral("替换几何元素"), 0 },

        // 下面这段要放成两行×四列（图标左、文字右）
        { QStringLiteral("相交"), 0 },
        { QStringLiteral("对称元素"), 0 },
        { QStringLiteral("修改尺寸"), 0 },
        { QStringLiteral("组合"), 0 },
        { QStringLiteral("转换"), 0 },
        { QStringLiteral("规则元素"), 0 },
        { QStringLiteral("投影"), 0 },
        { QStringLiteral("提取最小/最大点"), 0 },
        { QStringLiteral("提取中轴线"), 0 },

        // 其余继续一行横排
        { QStringLiteral("转化并导出"), 1 },
    };

    // 需要做成两行四列的那 8 个按钮
    const QStringList twoRowGroup = {
         QStringLiteral("点"),
         QStringLiteral("线"), 
         QStringLiteral("圆"),
         QStringLiteral("角圆"),
         QStringLiteral("平面"), 
         QStringLiteral("球体"),
         QStringLiteral("圆柱"), 
         QStringLiteral("圆锥"), 
         QStringLiteral("环面"), 
         QStringLiteral("自由造型线"), 
         QStringLiteral("自由造型表面"),
    };


    const QStringList twoRowGroup02 = {
        QStringLiteral("相交"),
        QStringLiteral("对称元素"),
        QStringLiteral("修改尺寸"),
        QStringLiteral("组合"),
        QStringLiteral("转换"),
        QStringLiteral("规则元素"),
        QStringLiteral("投影"),
        QStringLiteral("提取最小/最大点"),
        QStringLiteral("提取中轴线"),
    };


    QWidget* gridHolder_05 = nullptr;//这个指针的意思是 用来承载那个 2×4 的小方阵
    QGridLayout* grid_05 = nullptr;//这个指针是用来管理那个小方阵的布局
    int groupedCount_05 = 0;//记录已经放进小方阵的按钮数量

    QWidget* gridHolder02_05 = nullptr;
    QGridLayout* grid02_05 = nullptr;
    int groupedCount02_05 = 0;

    QWidget* gridHolder03_05 = nullptr;
    QGridLayout* grid03_05 = nullptr;
    int groupedCount03_05 = 0;

    for (const auto& action : actions05)
    {
        const bool inGroup_05 = twoRowGroup.contains(action.text);//contains函数检查某个元素是否在列表中 返回true或false
        const bool inGroup02_05 = twoRowGroup02.contains(action.text);
     

        auto* button = new QToolButton(ribbon05);
        button->setIcon(loadIconFor(action.text));
        button->setIconSize(QSize(32, 32));
        button->setMinimumSize(QSize(59, 90));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        button->setText(RibbonCommon::shiftNewLine(action.text, button->font(), 43));

        if (inGroup_05)
        {
            if (!gridHolder_05) {//等价于gridholder == nullptr
                gridHolder_05 = new QWidget(ribbon05);
                grid_05 = new QGridLayout(gridHolder_05);
                grid_05->setContentsMargins(4, 2, 4, 2);
                grid_05->setHorizontalSpacing(8);//水平间距
                grid_05->setVerticalSpacing(4);//垂直间距
                layout05->addWidget(gridHolder_05); // 把小方阵插入到主 ribbon
            }
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(20, 20));           // 小 icon
			button->setMinimumSize(QSize(90, 20));       
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);//
            button->setText(action.text);                 // 旁排文字，不再换行

            int row = groupedCount_05 / 4;
            int col = groupedCount_05 % 4;
            grid_05->addWidget(button, row, col);//三个参数的意思是：要添加的控件、行号、列号
            ++groupedCount_05;
            continue;
        }
        if (inGroup02_05) {
            if (!gridHolder02_05)
            {
                gridHolder02_05 = new QWidget(ribbon05);
                grid02_05 = new QGridLayout(gridHolder02_05);//QGridLayout的作用是把控件放在一个网格里
				grid02_05->setContentsMargins(4, 2, 4, 2);
                grid02_05->setHorizontalSpacing(8);
                grid02_05->setVerticalSpacing(4);
                layout05->addWidget(gridHolder02_05);
            }
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(20, 20));           // 小 icon
            button->setMinimumSize(QSize(90, 20));
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            button->setText(action.text);                 // 旁排文字，不再换行

            int row = groupedCount02_05 / 4;
            int col = groupedCount02_05 % 4;

            //布局管理  grid
            grid02_05->addWidget(button, row, col);
            ++groupedCount02_05;
            continue;
        }
        else {
            // 非该分组：仍旧一行横排
            layout05->addWidget(button);
        }
        if (action.hasMenu == 1) {
            auto* menu = new QMenu(button);
            menu->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/ROI_from_geomety_element.png"), QStringLiteral("来自几何元素的ROI"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/surface_mash_from_geomety_element.PNG"), QStringLiteral("来自几何元素的表面网格"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/CAD_from_geomety_element.PNG"), QStringLiteral("来自几何元素的 CAD"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/geometry_element_export_to_surface_mash.PNG"), QStringLiteral("将几何元素导出为表面网格"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/geometry_element_export_to_CAD.PNG"), QStringLiteral("将几何元素导出为 CAD"));
            menu->addAction(QIcon(":/geometry_icons02/icons_other/geometry_icons/transformation_and_export_pull_down_menu/untitle.PNG"), QStringLiteral("保存拟合点信息"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);
        }
    }
    layout05->addStretch();
    return ribbon05;
}

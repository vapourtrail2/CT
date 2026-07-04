#include "AnalysisPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/AnaIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps10::kAnaIconMap);
}

AnalysisPage::AnalysisPage (QWidget* parent)
    : RibbonPage(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("analysisEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#analysisEdit{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout08 = new QVBoxLayout(this);
    layout08->setContentsMargins(0, 0, 0, 0);
    layout08->setSpacing(3);

    // 功能区调用
    layout08->addWidget(buildRibbon08(this));
}

int AnalysisPage::tabIndex() const
{
    return TabIndex::Analysis;
}

QString AnalysisPage::tabName() const
{
    return QStringLiteral("分析");
}

QWidget* AnalysisPage::buildRibbon08(QWidget* parent)
{
    // 创建功能区容器
    auto* ribbon08 = new QFrame(parent);
    ribbon08->setObjectName(QStringLiteral("analysisRibbon"));
    ribbon08->setStyleSheet(QStringLiteral(
        "QFrame#analysisRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout08 = new QHBoxLayout(ribbon08);
    layout08->setContentsMargins(4, 4, 4, 4);
    layout08->setSpacing(1);

    struct RibbonAction08
    {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction08> actions08 = {
        { QStringLiteral("注解"), 0 },
        { QStringLiteral("实时值"), 0 },
        { QStringLiteral("孔隙/夹杂物"), 1 },
        { QStringLiteral("P203"), 0 },
        { QStringLiteral("P202/P201"), 0 },
        { QStringLiteral("设计件/实物对比"), 0 },
        { QStringLiteral("壁厚"), 2 },
        { QStringLiteral("位移"), 0 },
        { QStringLiteral("纤维复合材料"), 0 },
        { QStringLiteral("泡状/粉末结构"), 0 },
        { QStringLiteral("数字体积相关计算"), 0 },
        { QStringLiteral("灰度值"), 0 },
        { QStringLiteral("数据质量"), 0 },
        { QStringLiteral("切片图面积"), 0 },
        { QStringLiteral("OCR"), 0 },
        { QStringLiteral("夹紧模拟"), 3 },
        { QStringLiteral("结构力学模拟"), 0 },
        { QStringLiteral("传递现象"), 4 },
        { QStringLiteral("电池极片对齐分析"), 0 },
        { QStringLiteral("导入集成网格"), 0 },
        { QStringLiteral("创建集成网格"), 5 },
        { QStringLiteral("评估"), 6 },
        { QStringLiteral("更新所有分析"),0 }
    };
    for (const auto& action : actions08) {
        // 每个功能都使用图标,文字的形式展示
        auto* button = new QToolButton(ribbon08);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        QString afterShiftText = RibbonCommon::shiftNewLine(action.text, button->font(), 55);
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
            menu->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/Pores_and_inclusions_pull_down_menu/EasyPore.PNG"), QStringLiteral("EASYPORE算法"));
            menu->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/Pores_and_inclusions_pull_down_menu/defeat_ROI.PNG"), QStringLiteral("来自缺陷的ROI"));
            menu->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/Pores_and_inclusions_pull_down_menu/DefX_threshold.PNG"), QStringLiteral("DefX/仅阈值算法"));
            button->setMenu(menu);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 2) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/analyisis_icons/icons_other/analysis_icons/wall_thickness.PNG"), QStringLiteral("射线法"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/ball.png"), QStringLiteral("球体法"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 3) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/analyisis_icons/icons_other/analysis_icons/clip_simulation.PNG"), QStringLiteral("夹紧模拟"));
            menu02->addAction(QIcon(":/analyisis_icons/icons_other/analysis_icons/clip_simulation.PNG"), QStringLiteral("将夹紧网格放在场景中"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 4) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/absolute_shentou_experiment.PNG"), QStringLiteral("绝对渗透率实验"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/absolute_shentou_tensor.PNG"), QStringLiteral("绝对渗透率张量"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/molecule_diffusion_experical.PNG"), QStringLiteral("分子扩散实验"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/molecule_diffusion_tensor.PNG"), QStringLiteral("分子扩散张量"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/daore_experiment.PNG"), QStringLiteral("导热率实验"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/daore_tensor.PNG"), QStringLiteral("导热率张量"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/diandaolv_experiment.PNG"), QStringLiteral("电导率实验"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/diandaolv_tensor.PNG"), QStringLiteral("电导率张量"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/line.PNG"), QStringLiteral("毛细管压力曲线"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 5) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/create_regular_mesh.PNG"), QStringLiteral("创建规则集成网格"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/geometry_create_integration_mesh.PNG"), QStringLiteral("从几何元素中创建集成网格"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/mesh.PNG"), QStringLiteral("从四面体体积网格创建集成网格"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        if (action.hasMenu == 6) {
            auto* menu02 = new QMenu(button);
            menu02->setStyleSheet(QStringLiteral(
                "QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
                "QMenu::item{color:#e0e0e0; padding:6px 24px;}"
                "QMenu::item:selected{background:#3a3a3a;}"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/input_evaluate_mold.PNG"), QStringLiteral("导入评估模板"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/output_evaluate_mold.PNG"), QStringLiteral("导出评估模板"));
            menu02->addAction(QIcon(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/evaluate_property.PNG"), QStringLiteral("评估属性"));
            button->setMenu(menu02);
            button->setPopupMode(QToolButton::InstantPopup);//点击按钮时直接弹出菜单
        }
        layout08->addWidget(button);
    }
    layout08->addStretch();
    return ribbon08;
}


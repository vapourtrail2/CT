#include "AnalysisPage.h"
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
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kAnaIconMap);
}

namespace
{
constexpr int kButtonTextMaxWidth = 55;
constexpr int kButtonIconSize = 40;
constexpr int kButtonMinWidth = 70;
constexpr int kButtonMinHeight = 90;

const char* kMenuStyle =
"QMenu{background:#2b2b2b; border:1px solid #3a3a3a;}"
"QMenu::item{color:#e0e0e0; padding:6px 24px;}"
"QMenu::item:selected{background:#3a3a3a;}";

const char* kRibbonStyle =
"QFrame#analysisRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:400;}";
}

QList<RibbonDef::RibbonButtonDef> AnalysisPage::createAnalysisButtons()
{
    return {
        { QStringLiteral("注解"), {} },
        { QStringLiteral("实时值"), {} },
        { QStringLiteral("孔隙分析"),{} },
        { QStringLiteral("P203"), {} },
        { QStringLiteral("P202/P201"), {} },
        { QStringLiteral("设计件/实物对比"), {} },
        {
            QStringLiteral("壁厚"),
            {
                { QStringLiteral("射线法"), QStringLiteral(":/analyisis_icons/icons_other/analysis_icons/wall_thickness.PNG"), QString() },
                { QStringLiteral("球体法"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/ball.png"), QString() }
            }
        },
        { QStringLiteral("位移"), {} },
        { QStringLiteral("纤维复合材料"), {} },
        { QStringLiteral("泡状/粉末结构"), {} },
        { QStringLiteral("数字体积相关计算"), {} },
        { QStringLiteral("灰度值"), {} },
        { QStringLiteral("数据质量"), {} },
        { QStringLiteral("切片图面积"), {} },
        { QStringLiteral("OCR"), {} },
        {
            QStringLiteral("夹紧模拟"),
            {
                { QStringLiteral("夹紧模拟"), QStringLiteral(":/analyisis_icons/icons_other/analysis_icons/clip_simulation.PNG"), QString() },
                { QStringLiteral("将夹紧网格放在场景中"), QStringLiteral(":/analyisis_icons/icons_other/analysis_icons/clip_simulation.PNG"), QString() }
            }
        },
        { QStringLiteral("结构力学模拟"), {} },
        {
            QStringLiteral("传递现象"),
            {
                { QStringLiteral("绝对渗透率实验"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/absolute_shentou_experiment.PNG"), QString() },
                { QStringLiteral("绝对渗透率张量"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/absolute_shentou_tensor.PNG"), QString() },
                { QStringLiteral("分子扩散实验"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/molecule_diffusion_experical.PNG"), QString() },
                { QStringLiteral("分子扩散张量"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/molecule_diffusion_tensor.PNG"), QString() },
                { QStringLiteral("导热率实验"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/daore_experiment.PNG"), QString() },
                { QStringLiteral("导热率张量"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/daore_tensor.PNG"), QString() },
                { QStringLiteral("电导率实验"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/diandaolv_experiment.PNG"), QString() },
                { QStringLiteral("电导率张量"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/diandaolv_tensor.PNG"), QString() },
                { QStringLiteral("毛细管压力曲线"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/transfor_phenomenon_pull_down_menu/line.PNG"), QString() }
            }
        },
        { QStringLiteral("电池极片对齐分析"), {} },
        { QStringLiteral("导入集成网格"), {} },
        {
            QStringLiteral("创建集成网格"),
            {
                { QStringLiteral("创建规则集成网格"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/create_regular_mesh.PNG"), QString() },
                { QStringLiteral("从几何元素中创建集成网格"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/geometry_create_integration_mesh.PNG"), QString() },
                { QStringLiteral("从四面体体积网格创建集成网格"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/create_integration_mesh_pull_down_menu/mesh.PNG"), QString() }
            }
        },
        {
            QStringLiteral("评估"),
            {
                { QStringLiteral("导入评估模板"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/input_evaluate_mold.PNG"), QString() },
                { QStringLiteral("导出评估模板"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/output_evaluate_mold.PNG"), QString() },
                { QStringLiteral("评估属性"), QStringLiteral(":/analysis_icons02/icons_other/analysis_icons/evaluate_pull_down_menu/evaluate_property.PNG"), QString() }
            }
        },
        { QStringLiteral("更新所有分析"), {} }
    };
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
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("analysisRibbon"),
        kRibbonStyle,
        createAnalysisButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
        });
}

QMenu* AnalysisPage::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [](const QString&) {});
}

QToolButton* AnalysisPage::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
{
    auto* button = RibbonCommon::createRibbonButton(
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

    if (buttonDef.text == QStringLiteral("孔隙分析")) {
        connect(button, &QToolButton::clicked, this, [this]() {
            emit commandRequested(QStringLiteral("gap.analysis.open"));
            });
    }

    return button;
}

#include "c_ui/command/commands.h"
#include "c_ui/workbenches/common/IconMaps/RibbonIconMaps.h"
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

int StartPagePage::tabIndex() const
{
    return TabIndex::Start;
}

QString StartPagePage::tabName() const
{
    return QStringLiteral("开始");
}

static QIcon loadIconFor(const QString& text) {
    return RibbonCommon::loadIconByText(text, RibbonIconMaps::kStartIconMap);
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
"QFrame#startRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
"QToolButton{color:#e0e0e0; font-weight:400;}";

const char* kPageStyle =
"QWidget#pageStart{background-color:#2b2b2b;}"
"QLabel{color:#f0f0f0;}"
"QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
"QToolButton:hover{background-color:#3a3a3a;}";
}

QList<RibbonDef::RibbonButtonDef> StartPagePage::createStartButtons()
{
    return {
    { QStringLiteral("快速导入"),   QList<RibbonDef::RibbonMenuAction>{} },
    {
        QStringLiteral("体积导入"), 
        QList<RibbonDef::RibbonMenuAction>{
            { QStringLiteral("原始体积"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/origin_volume.png"), QString() },
            { QStringLiteral("图像堆栈"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/images_stack.png"), QString() },
            { QStringLiteral("合并对象"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/merge_obj.png"), QString() },
            { QStringLiteral("CT重建"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/CT_rebuild.png"), QStringLiteral("recon.open") },
            { QStringLiteral("增量CT重建"), QStringLiteral(":/start_icons02/icons_other/start_icons/volume_input_pull_down_menu/CT_rebuild.png"), QStringLiteral("reconn.open") }
        }
    },
    {
        QStringLiteral("显示模式"),
        QList<RibbonDef::RibbonMenuAction>{
            { QStringLiteral("原始"), QStringLiteral(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/display_pattern.png"), QString() },
            { QStringLiteral("颜色"), QStringLiteral(":/start_icons02/icons_other/start_icons/display_pattern_pull_down_menu/color.png"), QString() },
            { QStringLiteral("颜色和不透明度"), QStringLiteral(":/volume_icons/icons_other/volume_icons/volume_data_pull_down_menu/delete_volume_data.png"), QString() }
        }
    },

    { QStringLiteral("水平/窗口模式"), QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("厚板"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("裁剪当前切片图"), QList<RibbonDef::RibbonMenuAction>{} },

    {
        QStringLiteral("对齐"),
        QList<RibbonDef::RibbonMenuAction>{
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

    { QStringLiteral("指示器"),  QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("距离"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("角度(4个点)"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("角度(3个点)"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("折线长度"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("最小/最大距离"),   QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("卡尺"),  QList<RibbonDef::RibbonMenuAction>{} },

    {
        QStringLiteral("捕捉模式"), 
        QList<RibbonDef::RibbonMenuAction>{
            { QStringLiteral("最小"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/min.PNG"), QString() },
            { QStringLiteral("最大"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/max.PNG"), QString() },
            { QStringLiteral("梯度"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/gradient.PNG"), QString() },
            { QStringLiteral("表面"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/surface.PNG"), QString() },
            { QStringLiteral("局部"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/local.PNG"), QString() },
            { QStringLiteral("关"), QStringLiteral(":/new/prefix1/icons_other/measure_icons/capture_pattern_pull_down_menu/off.PNG"), QString() }
        }
    },

    { QStringLiteral("重新捕捉量具控点"),  QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("创建报告"),  QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("创建书签"),  QList<RibbonDef::RibbonMenuAction>{} },
    { QStringLiteral("书签编辑器"),  QList<RibbonDef::RibbonMenuAction>{} },

    {
        QStringLiteral("保存图像/影片"), 
        QList<RibbonDef::RibbonMenuAction>{
            { QStringLiteral("保存图像"), QStringLiteral(":/start_icons01/icons_other/start_icons/save_image.png"), QStringLiteral("image.save") },
            { QStringLiteral("保存影片/图像堆栈"), QStringLiteral(":/start_icons01/icons_other/start_icons/save_image.png"), QStringLiteral("slicestack.save") }
        }
    }
    };
}

StartPagePage::StartPagePage(QWidget* parent)
    :RibbonPage(parent)//在创建StartPagePage之前 必须先创建他的父类RibbonPage 并把parent传给父类
{
    // 设置页面外观
    setObjectName(QStringLiteral("pageStart"));
    setStyleSheet(QString::fromLatin1(kPageStyle));

    auto* layout01 = new QVBoxLayout(this);
    layout01->setContentsMargins(0, 0, 0, 0);
    layout01->setSpacing(3);

    layout01->addWidget(buildRibbon(this));
}

QWidget* StartPagePage::buildRibbon(QWidget* parent)
{
    return RibbonCommon::createRibbonBar(
        parent,
        QStringLiteral("startRibbon"),
        kRibbonStyle,
        createStartButtons(),
        [this](QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef) {
            return createButton(parent, buttonDef);
		});
}

QMenu* StartPagePage::createMenu(QWidget* parent, const QList<RibbonDef::RibbonMenuAction>& menuActions)
{
    return RibbonCommon::createRibbonMenu(
        parent,
        menuActions,
        kMenuStyle,
        this,
        [this](const QString& command) {
            emit commandRequested(command);
        });
}

QToolButton* StartPagePage::createButton(QWidget* parent, const RibbonDef::RibbonButtonDef& buttonDef)
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



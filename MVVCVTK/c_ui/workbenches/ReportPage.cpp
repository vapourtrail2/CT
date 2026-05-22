#include "ReportPage.h"
#include "c_ui/workbenches/common/RibbonCommon.h"
#include "c_ui/workbenches/common/IconMaps/ReportIconMap.h"
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
    return RibbonCommon::loadIconByText(text, IconMaps11::kReportIconMap);
}

ReportPage::ReportPage(QWidget* parent)
    : QWidget(parent)
{
    // 设置页面外观
    setObjectName(QStringLiteral("reportEdit"));
    setStyleSheet(QStringLiteral(
        "QWidget#pageReport{background-color:#2b2b2b;}"
        "QLabel{color:#f0f0f0;}"
        "QToolButton{color:#f7f7f7; border-radius:6px; padding:6px;}"
        "QToolButton:hover{background-color:#3a3a3a;}"));

    auto* layout10 = new QVBoxLayout(this);
    layout10->setContentsMargins(0, 0, 0, 0);
    layout10->setSpacing(3);

    // 功能区调用
    layout10->addWidget(buildRibbon10(this));
}

QWidget* ReportPage::buildRibbon10(QWidget* parent)
{
    auto* ribbon10 = new QFrame(parent);//ribbon03是--功能区的容器
    ribbon10->setObjectName(QStringLiteral("reportRibbon"));
    ribbon10->setStyleSheet(QStringLiteral(
        "QFrame#reportRibbon{background-color:#322F30; border-radius:8px; border:1px solid #2b2b2b;}"
        "QToolButton{color:#e0e0e0; font-weight:600;}"));

    auto* layout10 = new QHBoxLayout(ribbon10);
    layout10->setContentsMargins(4, 4, 4, 4);
    layout10->setSpacing(1);

    struct RibbonAction10 {
        QString text;
        int hasMenu;
    };

    const QList<RibbonAction10> actions10 = {
        { QStringLiteral("创建报告"), 0 },
        { QStringLiteral("打开报告"), 0 },
        { QStringLiteral("更新报告"), 0 },
        { QStringLiteral("删除报告"), 0 },
        { QStringLiteral("导出PDF"), 0 },
        { QStringLiteral("导出HTML"), 0 },
        { QStringLiteral("导出默认布局"), 0 },
        { QStringLiteral("导出特定布局"), 0 },
        { QStringLiteral("创建书签"), 0 },
        { QStringLiteral("书签编辑器"), 0 },
        { QStringLiteral("保存图像"), 0 },
        { QStringLiteral("保存影片/图像堆栈"), 0 },
        { QStringLiteral("导出为Q-DAS文件"), 0 },
    };

    // 需要做成两行四列的那 8 个按钮
    const QStringList twoRowGroup = {
        QStringLiteral("导出PDF"),
        QStringLiteral("导出HTML"),
        QStringLiteral("导出默认布局"),
        QStringLiteral("导出特定布局"),
    };

    QWidget* gridHolder_10 = nullptr;//这个指针的意思是 用来承载那个 2×4 的小方阵
    QGridLayout* grid_10 = nullptr;//这个指针是用来管理那个小方阵的布局
    int groupedCount_10 = 0;//记录已经放进小方阵的按钮数量

    for (const auto& action : actions10)
    {
        const bool inGroup_10 = twoRowGroup.contains(action.text);//contains函数检查某个元素是否在列表中 返回true或false
        auto* button = new QToolButton(ribbon10);
        button->setIcon(loadIconFor(action.text));
        button->setIconSize(QSize(32, 32));
        button->setMinimumSize(QSize(59, 90));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        // Shared wrap rule avoids duplicate text-layout code in each page.
        button->setText(RibbonCommon::shiftNewLine(action.text, button->font(), 43));
        if (inGroup_10)
        {
            if (!gridHolder_10) {//等价于gridholder == nullptr
                gridHolder_10 = new QWidget(ribbon10);
                grid_10 = new QGridLayout(gridHolder_10);
                grid_10->setContentsMargins(4, 2, 4, 2);
                grid_10->setHorizontalSpacing(8);//水平间距
                grid_10->setVerticalSpacing(4);//垂直间距
                layout10->addWidget(gridHolder_10); // 把小方阵插入到主 ribbon
            }
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(20, 20));           // 小 icon
            button->setMinimumSize(QSize(90, 20));
            button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            button->setText(action.text);

            int row = groupedCount_10 / 2;
            int col = groupedCount_10 % 2;
            grid_10->addWidget(button, row, col);//三个参数的意思是：要添加的控件、行号、列号
            ++groupedCount_10;
            continue;
        }

        else {
            // 非该分组：仍旧一行横排
            layout10->addWidget(button);
        }
    }
    layout10->addStretch();
    return ribbon10;
}

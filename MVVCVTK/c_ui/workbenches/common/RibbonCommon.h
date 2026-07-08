#pragma once

#include <cstddef>
#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QList>
#include <QMenu>
#include <QString>
#include <QWidget>
#include <QSize>
#include <QToolButton>
#include <QFrame>
#include <QHBoxLayout>

namespace RibbonDef {
    struct RibbonMenuAction
    {
        QString text;
        QString iconPath;
        QString command;
    };

    struct RibbonButtonDef
    {
        QString text;
        QList<RibbonMenuAction> menuActions;
    };
}

namespace RibbonCommon {

    // 文本和图标路径映射项
    struct IconMapItem {
        QString key;
        const char* iconPath;
    };

    // 按像素宽度给按钮文本换行
    inline QString shiftNewLine(
        const QString& text,
        const QFont& font,
        int maxWidthPx,
        double overflowFactor = 1.0)
    {
        QFontMetrics fm(font);
        QString out;
        int lineWidth = 0;

        auto flushLineBreak = [&]() {
            out += QChar('\n');
            lineWidth = 0;
            };

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            const int w = fm.horizontalAdvance(ch);
            const bool isBreakable =
                (ch.isSpace() || ch == '/' || ch == QChar(0x00B7) || ch == QChar(0x3001));

            if (lineWidth + static_cast<int>(w * overflowFactor) > maxWidthPx && !out.isEmpty()) {
                flushLineBreak();
            }

            out += ch;
            lineWidth += w;

            if (isBreakable && lineWidth > static_cast<int>(maxWidthPx * 0.85)) {
                flushLineBreak();
            }
        }
        return out;
    }

    // 按文本从映射表取图标，没命中时返回默认图标
    template <std::size_t N>
    inline QIcon loadIconByText(
        const QString& text,
        const IconMapItem(&map)[N],
        const char* fallbackPath = ":/icons/icons/move.png")
    {
        for (const auto& item : map) {
            if (text == item.key) {
                QIcon icon(QString::fromUtf8(item.iconPath));
                if (!icon.isNull()) {
                    return icon;
                }
            }
        }
        return QIcon(QString::fromUtf8(fallbackPath));
    }

	//Menu创建函数，使用模板参数CommandHandler来处理命令
    template <typename CommandHandler>
    inline QMenu* createRibbonMenu(
        QWidget* parent,
        const QList<RibbonDef::RibbonMenuAction>& menuActions,
        const char* menuStyle,
        QObject* context,
        CommandHandler onCommand)
    {
        auto* menu = new QMenu(parent);
        menu->setStyleSheet(QString::fromLatin1(menuStyle));

        for (const auto& menuAction : menuActions) {
            auto* qAction = menu->addAction(
                QIcon(menuAction.iconPath),
                menuAction.text);

            if (!menuAction.command.isEmpty()) {
                QObject::connect(qAction, &QAction::triggered, context, [command = menuAction.command, onCommand]() {
                    onCommand(command);
                    });
            }
        }

        return menu;
    }

	// 创建功能区按钮，使用模板参数IconLoader和MenuBuilder来加载图标和创建菜单
    template <typename IconLoader, typename MenuBuilder>
    inline QToolButton* createRibbonButton(
        QWidget* parent,
        const RibbonDef::RibbonButtonDef& buttonDef,
        IconLoader loadIcon,
        MenuBuilder createMenu,
        int textMaxWidth,
        int iconSize,
        int minWidth,
        int minHeight)
    {
        auto* button = new QToolButton(parent);

        const QString afterShiftText =
            shiftNewLine(buttonDef.text, button->font(), textMaxWidth);

        button->setText(afterShiftText);
        button->setIcon(loadIcon(buttonDef.text));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(iconSize, iconSize));
        button->setMinimumSize(QSize(minWidth, minHeight));

        if (!buttonDef.menuActions.isEmpty()) {
            button->setMenu(createMenu(button, buttonDef.menuActions));
            button->setPopupMode(QToolButton::InstantPopup);
        }

        return button;
    }

    template <typename ButtonBuilder>
    inline QWidget* createRibbonBar(
        QWidget* parent,
        const QString& objectName,
        const char* ribbonStyle,
        const QList<RibbonDef::RibbonButtonDef>& buttons,
        ButtonBuilder createButton)
    {
        auto* ribbon = new QFrame(parent);
        ribbon->setObjectName(objectName);
        ribbon->setStyleSheet(QString::fromLatin1(ribbonStyle));

        auto* layout = new QHBoxLayout(ribbon);
        layout->setContentsMargins(4, 4, 4, 4);
        layout->setSpacing(1);

        for (const auto& buttonDef : buttons) {
            layout->addWidget(createButton(ribbon, buttonDef));
        }

        layout->addStretch();
        return ribbon;
    }
}




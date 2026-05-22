#pragma once

#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QString>
#include <cstddef>

//迁移一些通用的功能函数和结构体，避免重复代码，方便维护
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

} // namespace RibbonCommon
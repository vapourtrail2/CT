#include "c_ui/app/app.h"
#include <QApplication>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QLoggingCategory>
#include <QPalette>
#include <QStyleFactory>
#include "c_ui/MainWindow.h"


namespace app {

    App::App() = default;

    int App::run(int argc, char** argv)
    {
        // 创建并持有 QApplication 对象，确保 Qt 事件循环存在
        QApplication qtApp(argc, argv);
		applyGlobalStyle();

        CTViewer mainWindow;
        mainWindow.showMaximized();

        const int exitCode = qtApp.exec();
        return exitCode;
    }

    void App::applyGlobalStyle() const
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

        // 配色表
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(30, 30, 30));
        palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
        palette.setColor(QPalette::Base, QColor(25, 25, 25));
        palette.setColor(QPalette::AlternateBase, QColor(45, 45, 45));
        palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
        palette.setColor(QPalette::ToolTipText, QColor(30, 30, 30));
        palette.setColor(QPalette::Text, QColor(220, 220, 220));
        palette.setColor(QPalette::Button, QColor(45, 45, 45));
        palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
        palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
        palette.setColor(QPalette::Highlight, QColor(66, 133, 244));
        palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
        QApplication::setPalette(palette);

        // 预加载常用字体
        QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/SourceHanSansCN-Regular.otf"));
    }
} // namespace app
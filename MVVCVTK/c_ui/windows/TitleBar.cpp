#include "c_ui/windows/Titlebar.h"
#include <QHBoxLayout>
#include <QToolButton>
#include <QMouseEvent>
#include <QFont>
#include <QRect>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void TitleBar::buildUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("customTitleBar"));
    setFixedHeight(38);
    setStyleSheet(QStringLiteral(
        "QWidget#customTitleBar{background-color:#202020;}"
        "QToolButton{background:transparent; border:none; color:#f5f5f5; padding:6px;}"
        "QToolButton:hover{background-color:rgba(255,255,255,0.12);}"));

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* rightContainer = new QWidget(this);
    auto* rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    const QString titleButtonStyle = QStringLiteral(
        "QToolButton { background:transparent; border:none; padding:6px; color:#f5f5f5; border-radius:4px; }"
        "QToolButton:hover { background-color:rgba(255,255,255,0.12); }"
        "QToolButton:pressed { background-color:rgba(255,255,255,0.20); }");

    auto makeBtn = [&](QPointer<QToolButton>& btn, const QString& text, const QString& tip) {
        btn = new QToolButton(rightContainer);
        btn->setToolTip(tip);
        btn->setText(text);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedSize(32, 32);
        btn->setStyleSheet(titleButtonStyle);
        rightLayout->addWidget(btn);
        };

    makeBtn(btnMinimize_, QStringLiteral("—"), QStringLiteral("最小化"));
    makeBtn(btnMaximize_, QString(QChar(0x2750)), QStringLiteral("最大化"));
    makeBtn(btnClose_, QStringLiteral("×"), QStringLiteral("关闭"));

    QFont f = btnMaximize_->font();
    f.setFamily(QStringLiteral("Segoe UI Symbol"));
    btnMaximize_->setFont(f);

    layout->addStretch(0);
    layout->addWidget(rightContainer, 0);

    connect(btnMinimize_, &QToolButton::clicked, this, [this]() {
        window()->showMinimized();
        });
    connect(btnMaximize_, &QToolButton::clicked, this, [this]() {
        toggleMaximize();
        });
    connect(btnClose_, &QToolButton::clicked, this, [this]() {
        window()->close();
        });

    updateMaximizeIcon();
}

void TitleBar::toggleMaximize()
{
    QWidget* win = window();
    if (win->isMaximized()) {
        win->showNormal();
        QScreen* screen = win->windowHandle() ? win->windowHandle()->screen()
            : QGuiApplication::primaryScreen();
        if (screen) {
            const QRect avail = screen->availableGeometry();
            const int w = avail.width() * 0.75;
            const int h = avail.height() * 0.75;
            win->resize(w, h);
            win->move(avail.x() + (avail.width() - w) / 2,
                avail.y() + (avail.height() - h) / 2);
        }
    }
    else {
        win->showMaximized();
    }
    updateMaximizeIcon();
}

void TitleBar::updateMaximizeIcon()
{
    if (!btnMaximize_) return;
    QFont f = btnMaximize_->font();
    f.setFamily(QStringLiteral("Segoe UI Symbol"));
    btnMaximize_->setFont(f);
    if (window()->isMaximized()) {
        btnMaximize_->setText(QString(QChar(0x2750)));
        btnMaximize_->setToolTip(QStringLiteral("还原"));
    }
    else {
        btnMaximize_->setText(QStringLiteral("□"));
        btnMaximize_->setToolTip(QStringLiteral("最大化"));
    }
}

void TitleBar::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        dragging_ = true;
        dragOffset_ = e->globalPos() - window()->frameGeometry().topLeft();
        e->accept();
        return;
    }
    QWidget::mousePressEvent(e);
}

void TitleBar::mouseMoveEvent(QMouseEvent* e)
{
    if (dragging_) {
        window()->move(e->globalPos() - dragOffset_);
        e->accept();
        return;
    }
    QWidget::mouseMoveEvent(e);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* e)
{
    dragging_ = false;
    QWidget::mouseReleaseEvent(e);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* e)
{
    dragging_ = false;
    toggleMaximize();
    e->accept();
}
#pragma once
#include <QWidget>
#include <QPoint>
#include <QPointer>

class QToolButton;

// 自定义标题栏：最小化/最大化/关闭 + 拖动窗口 + 双击最大化。
// 它只操作“自己所在的顶层窗口”window()，完全不认识 CTViewer。
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private:
    void buildUi();
    void toggleMaximize();
    void updateMaximizeIcon();

    QPointer<QToolButton> btnMinimize_;
    QPointer<QToolButton> btnMaximize_; 
    QPointer<QToolButton> btnClose_;

    bool dragging_ = false;
    QPoint dragOffset_;
};
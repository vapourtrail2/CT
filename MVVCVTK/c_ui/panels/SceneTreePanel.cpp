#include "SceneTreePanel.h"

#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

SceneTreePanel::SceneTreePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setStyleSheet(
        "QTreeWidget{background:#1f1f1f; border:1px solid #333;}");

    root_ = new QTreeWidgetItem(
        tree_,
        QStringList() << QStringLiteral("场景"));
    root_->setExpanded(true);

    layout->addWidget(tree_);

    rebuildTree();
}

void SceneTreePanel::setDataState(
    bool hasData,
    const QString& sourcePath)
{
    hasData_ = hasData;
    sourcePath_ = sourcePath;

    rebuildTree();
}

void SceneTreePanel::rebuildTree()
{
    if (!tree_ || !root_) {
        return;
    }

    const QSignalBlocker blocker(tree_);

    clearTree();

    if (!hasData_) {
        volumeItem_ = new QTreeWidgetItem(
            root_,
            QStringList() << QStringLiteral("(无数据加载)"));
        root_->setExpanded(true);
        return;
    }

    const QString displayName =
        sourcePath_.trimmed().isEmpty()
        ? QStringLiteral("当前数据")
        : sourcePath_;

    volumeItem_ = new QTreeWidgetItem(
        root_,
        QStringList() << displayName);

    root_->setExpanded(true);
    tree_->expandAll();
}

void SceneTreePanel::clearTree()
{
    volumeItem_ = nullptr;

    if (!root_) {
        return;
    }

    while (root_->childCount() > 0) {
        delete root_->takeChild(0);
    }
}

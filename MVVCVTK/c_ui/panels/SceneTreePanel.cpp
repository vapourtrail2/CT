#include "SceneTreePanel.h"

#include <QAbstractItemView>
#include <QSignalBlocker>
#include <QVariant>
#include <QHBoxLayout>
#include <QPushButton>
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
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);

    root_ = new QTreeWidgetItem(
        tree_,
        QStringList() << QStringLiteral("场景"));
    root_->setExpanded(true);

    layout->addWidget(tree_);

    auto* cropControls = new QHBoxLayout();
    cropControls->setContentsMargins(0, 0, 0, 0);
    cropControls->setSpacing(4);

    keepInsideButton_ = new QPushButton(QStringLiteral("保留内"), this);
    removeInsideButton_ = new QPushButton(QStringLiteral("移除内"), this);
    applyButton_ = new QPushButton(QStringLiteral("应用"), this);
    restoreButton_ = new QPushButton(QStringLiteral("恢复原始"), this);
    exitButton_ = new QPushButton(QStringLiteral("退出"), this);

    for (auto* button : {
            keepInsideButton_,
            removeInsideButton_,
            applyButton_,
            restoreButton_,
            exitButton_ }) 
    {
        button->setAutoDefault(false);
        cropControls->addWidget(button);
    }
    layout->addLayout(cropControls);

    connect(
        tree_,
        &QTreeWidget::itemClicked,
        this,
        [this](QTreeWidgetItem* item) {
            if (!item
                || !item->data(0, Qt::UserRole + 1).toBool()) {
                return;
            }
            emit cropNodeActivated(
                item->data(0, Qt::UserRole).toULongLong());
        });
    connect(
        keepInsideButton_,
        &QPushButton::clicked,
        this,
        &SceneTreePanel::cropKeepInsideRequested);
    connect(
        removeInsideButton_,
        &QPushButton::clicked,
        this,
        &SceneTreePanel::cropRemoveInsideRequested);
    connect(
        applyButton_,
        &QPushButton::clicked,
        this,
        &SceneTreePanel::cropApplyRequested);
    connect(
        restoreButton_,
        &QPushButton::clicked,
        this,
        &SceneTreePanel::cropRestoreOriginalRequested);
    connect(
        exitButton_,
        &QPushButton::clicked,
        this,
        &SceneTreePanel::cropExitRequested);

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

void SceneTreePanel::setCropTreeState(const CropTreeState& state)
{
    cropTreeState_ = state;
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
        keepInsideButton_->setEnabled(false);
        removeInsideButton_->setEnabled(false);
        applyButton_->setEnabled(false);
        restoreButton_->setEnabled(false);
        exitButton_->setEnabled(false);
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

    cropRoot_ = new QTreeWidgetItem(
        volumeItem_,
        QStringList() << QStringLiteral("裁切记录"));

    if (cropTreeState_.items.empty()) {
        auto* emptyItem = new QTreeWidgetItem(
            cropRoot_,
            QStringList() << QStringLiteral("(暂无裁切记录)"));
        emptyItem->setDisabled(true);
    }
    else {
        for (const auto& crop : cropTreeState_.items) {
            const QString text = crop.isApplied
                ? QStringLiteral("已应用｜%1").arg(crop.text)
                : crop.text;
            auto* item = new QTreeWidgetItem(
                cropRoot_,
                QStringList() << text);
            item->setData(
                0,
                Qt::UserRole,
                QVariant::fromValue(
                    static_cast<qulonglong>(crop.nodeCount)));
            item->setData(
                0,
                Qt::UserRole + 1,
                crop.isSelectable);
            if (!crop.isSelectable) {
                item->setDisabled(true);
            }
            if (crop.isCurrent) {
                tree_->setCurrentItem(item);
            }
        }
    }

    const bool canEdit =
        hasData_
        && cropTreeState_.isCropping
        && !cropTreeState_.isBuilding;
    keepInsideButton_->setEnabled(canEdit);
    removeInsideButton_->setEnabled(canEdit);
    applyButton_->setEnabled(
        hasData_ && cropTreeState_.canApply);
    restoreButton_->setEnabled(
        hasData_ && cropTreeState_.canRestoreOriginal);
    exitButton_->setEnabled(
        hasData_
        && cropTreeState_.isCropping
        && !cropTreeState_.isBuilding);

    root_->setExpanded(true);
    tree_->expandAll();
}

void SceneTreePanel::clearTree()
{
    volumeItem_ = nullptr;
    cropRoot_ = nullptr;

    if (!root_) {
        return;
    }

    while (root_->childCount() > 0) {
        delete root_->takeChild(0);
    }
}

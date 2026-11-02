#include "SceneTreePanel.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QPushButton>
#include <QShortcut>
#include <QTreeView>
#include <QVBoxLayout>

SceneTreePanel::SceneTreePanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    tree_ = new QTreeView(this);
    model_ = new SceneTreeModel(this);
    tree_->setModel(model_);
    tree_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setEditTriggers(QAbstractItemView::EditKeyPressed);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setStretchLastSection(true);
    layout->addWidget(tree_);

    cropControls_ = new QWidget(this);
    auto* cropLayout = new QVBoxLayout(cropControls_);
    cropLayout->setContentsMargins(0, 0, 0, 0);
    auto* row = new QHBoxLayout;
    keepInsideButton_ = new QPushButton(QStringLiteral("保留内"), cropControls_);
    removeInsideButton_ = new QPushButton(QStringLiteral("移除内"), cropControls_);
    exitButton_ = new QPushButton(QStringLiteral("退出裁切"), cropControls_);
    for (auto* button : {keepInsideButton_, removeInsideButton_, exitButton_}) row->addWidget(button);
    cropLayout->addLayout(row);
    row = new QHBoxLayout;
    applyButton_ = new QPushButton(QStringLiteral("应用"), cropControls_);
    restoreButton_ = new QPushButton(QStringLiteral("激活原始体积"), cropControls_);
    row->addWidget(applyButton_);
    row->addWidget(restoreButton_);
    cropLayout->addLayout(row);
    cropControls_->hide();
    layout->addWidget(cropControls_);

    connect(tree_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        const auto item = index.sibling(index.row(), 0);
        emit objectSelected(item.data(SceneTreeModel::Kind).toInt(), item.data(SceneTreeModel::VolumeId).toInt(), item.data(SceneTreeModel::ObjectId).toULongLong());
    });
    connect(tree_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const auto item = index.sibling(index.row(), 0);
        if (!item.data(SceneTreeModel::CanActivate).toBool()) return;
        const auto kind = static_cast<SceneNodeKind>(item.data(SceneTreeModel::Kind).toInt());
        if (kind == SceneNodeKind::CropHistory) emit cropNodeActivated(item.data(SceneTreeModel::ObjectId).toULongLong());
        else emit volumeActivated(item.data(SceneTreeModel::VolumeId).toInt());
    });
    connect(model_, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
        if (model_->isSyncing()) return;
        if (item->column() != 0 || item->data(SceneTreeModel::Kind).toInt() != int(SceneNodeKind::Measurement)) return;
        const int volume = item->data(SceneTreeModel::VolumeId).toInt();
        const auto object = item->data(SceneTreeModel::ObjectId).toULongLong();
        emit objectVisibilityChanged(volume, object, item->checkState() == Qt::Checked);
        emit objectRenamed(volume, object, item->text());
    });
    auto* remove = new QShortcut(QKeySequence::Delete, tree_);
    remove->setContext(Qt::WidgetWithChildrenShortcut);
    connect(remove, &QShortcut::activated, this, [this] {
        const auto index = tree_->currentIndex().sibling(tree_->currentIndex().row(), 0);
        if (index.data(SceneTreeModel::CanRemove).toBool()) emit objectRemoveRequested(index.data(SceneTreeModel::VolumeId).toInt(), index.data(SceneTreeModel::ObjectId).toULongLong());
    });
    connect(keepInsideButton_, &QPushButton::clicked, this, &SceneTreePanel::cropKeepInsideRequested);
    connect(removeInsideButton_, &QPushButton::clicked, this, &SceneTreePanel::cropRemoveInsideRequested);
    connect(applyButton_, &QPushButton::clicked, this, &SceneTreePanel::cropApplyRequested);
    connect(restoreButton_, &QPushButton::clicked, this, &SceneTreePanel::cropRestoreOriginalRequested);
    connect(exitButton_, &QPushButton::clicked, this, &SceneTreePanel::cropExitRequested);
}

void SceneTreePanel::setDataState(bool hasData, const QString&)
{
    // Session transitions affect controls, not the registered scene objects.
    if (!hasData) cropControls_->hide();
}

void SceneTreePanel::setSceneNodes(const std::vector<SceneNode>& nodes)
{
    const bool firstPopulation = model_->rowCount() == 0;
    model_->syncNodes(nodes);
    if (firstPopulation) tree_->expandToDepth(1);
}

void SceneTreePanel::setCropTreeState(const CropTreeState& state)
{
    cropControls_->setVisible(state.isCropping || state.isBuilding);
    const bool editable = state.isCropping && !state.isBuilding;
    keepInsideButton_->setEnabled(editable);
    removeInsideButton_->setEnabled(editable);
    exitButton_->setEnabled(editable);
    applyButton_->setEnabled(state.canApply && !state.isBuilding);
    restoreButton_->setEnabled(state.canRestoreOriginal && !state.isBuilding);
}

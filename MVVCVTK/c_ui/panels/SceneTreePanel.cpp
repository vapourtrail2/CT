#include "SceneTreePanel.h"

#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QMetaObject>
#include <QThread>
#include <vtkImageData.h>

namespace {
constexpr int kFlagRole = Qt::UserRole + 1;

QTreeWidgetItem* MakeVisibilityItem(QTreeWidgetItem* parent, const QString& label, std::uint32_t flagBit)
{
    auto* item = new QTreeWidgetItem(parent, QStringList() << label);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    item->setData(0, kFlagRole, static_cast<qulonglong>(flagBit));
    item->setCheckState(0, Qt::Unchecked);
    return item;
}
}

SceneTreePanel::SceneTreePanel(QWidget* parent)
    : QWidget(parent)
{
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(4, 4, 4, 4);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setStyleSheet("QTreeWidget{background:#1f1f1f; border:1px solid #333;}");

    root_ = new QTreeWidgetItem(tree_, QStringList() << QStringLiteral("场景"));
    root_->setExpanded(true);

    connect(tree_, &QTreeWidget::itemChanged, this, &SceneTreePanel::onItemChanged);

    v->addWidget(tree_);
    clearTree();
}

void SceneTreePanel::setSession(
    const Dataset& dataset)
{
    state_ = dataset.getState();
	broadcaster_ = dataset.getBroadcaster();
    lifeToken_ = std::make_shared<int>(1);

    rebuildTree(dataset);

    broadcaster_->SetObserver(lifeToken_, [this,dataset](UpdateFlags flags) {

        if (HasFlag(flags,UpdateFlags::DataReady))
        {
            QMetaObject::invokeMethod(this, [this, dataset]() {
                rebuildTree(dataset);
            }, Qt::QueuedConnection);
        }

        if (!HasFlag(flags, UpdateFlags::Visibility) && flags != UpdateFlags::All) {
            return;
        }

        if (QThread::currentThread() == thread()) {
            syncVisibility();
            return;
        }

        QMetaObject::invokeMethod(this, [this]() { syncVisibility(); }, Qt::QueuedConnection);
        });

}

void SceneTreePanel::rebuildTree(
    const Dataset& dataset)
{
    if (!tree_) {
        return;
    }
        
    const QSignalBlocker blocker(tree_);

    clearTree();

    if (!root_) {
        return;
    }

    if (!dataset.getImage()) {
        volumeItem_ = new QTreeWidgetItem(root_, QStringList() << QStringLiteral("(无数据加载)"));
        root_->setExpanded(true);
        return;
    }

	const std::array<int, 3> dims = dataset.getDims();

    const QString name = QStringLiteral(": %1 x %2 x %3  (%4)")
        .arg(dims[0])
        .arg(dims[1])
        .arg(dims[2])
        .arg(dataset.getSourcePath());

    volumeItem_ = new QTreeWidgetItem(root_, QStringList() << name);
    volumeItem_->setExpanded(true);

    helpersItem_ = new QTreeWidgetItem(root_, QStringList() << QStringLiteral("设置"));
    helpersItem_->setExpanded(true);

    clipPlanesItem_ = MakeVisibilityItem(helpersItem_, QStringLiteral("显示 MPR 平面"), VisFlags::Planes3D);
    crosshairItem_ = MakeVisibilityItem(helpersItem_, QStringLiteral("十字线"), VisFlags::Crosshair);
    axesItem_ = MakeVisibilityItem(helpersItem_, QStringLiteral("标量尺"), VisFlags::Ruler);

    root_->setExpanded(true);
    tree_->expandAll();

    syncVisibility();
}

void SceneTreePanel::syncVisibility()
{
    if (!tree_ || !state_) {
        return;
    }

    const QSignalBlocker blocker(tree_);
    const std::uint32_t mask = state_->GetVisibilityMask();

    auto applyCheck = [mask](QTreeWidgetItem* item, std::uint32_t bit) {
        if (!item) {
            return;
        }
        item->setCheckState(0, (mask & bit) ? Qt::Checked : Qt::Unchecked);
    };

    applyCheck(clipPlanesItem_, VisFlags::Planes3D);
    applyCheck(crosshairItem_, VisFlags::Crosshair);
    applyCheck(axesItem_, VisFlags::Ruler);
}

void SceneTreePanel::clearTree()
{
    volumeItem_ = nullptr;
    helpersItem_ = nullptr;
    clipPlanesItem_ = nullptr;
    crosshairItem_ = nullptr;
    axesItem_ = nullptr;

    if (!root_) {
        return;
    }

    root_->takeChildren();
}

void SceneTreePanel::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (!state_ || !item || column != 0) {
        return;
    }

    const QVariant rawFlag = item->data(0, kFlagRole);
    if (!rawFlag.isValid()) {
        return;
    }

    const auto flagBit = static_cast<std::uint32_t>(rawFlag.toULongLong());
    const bool checked = item->checkState(0) == Qt::Checked;
    state_->SetElementVisible(flagBit, checked);
}

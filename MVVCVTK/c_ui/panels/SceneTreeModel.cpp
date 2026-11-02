#include "SceneTreeModel.h"
#include <QFont>
#include <QScopedValueRollback>
#include <QVariant>
#include <map>
#include <set>

SceneTreeModel::SceneTreeModel(QObject* parent) : QStandardItemModel(parent)
{
    setHorizontalHeaderLabels({QStringLiteral("场景对象"), QStringLiteral("结果 / 状态")});
}

void SceneTreeModel::syncNodes(const std::vector<SceneNode>& nodes)
{
    const QScopedValueRollback<bool> syncing(syncing_, true);
    std::map<QString, QStandardItem*> items;
    const auto collect = [&items](const auto& self, QStandardItem* parent) -> void {
        for (int row = 0; row < parent->rowCount(); ++row) {
            auto* item = parent->child(row);
            items[item->data(Key).toString()] = item;
            self(self, item);
        }
    };
    collect(collect, invisibleRootItem());
    std::set<QString> retained;
    for (const auto& node : nodes) retained.insert(node.key);
    const auto remove = [&retained](const auto& self, QStandardItem* parent) -> void {
        for (int row = parent->rowCount() - 1; row >= 0; --row) {
            auto* item = parent->child(row);
            if (!retained.count(item->data(Key).toString())) parent->removeRow(row);
            else self(self, item);
        }
    };
    remove(remove, invisibleRootItem());
    items.clear();
    collect(collect, invisibleRootItem());
    for (const auto& node : nodes) {
        auto* parent = node.parentKey.isEmpty() ? invisibleRootItem() : items.at(node.parentKey);
        auto found = items.find(node.key);
        auto* item = found == items.end() ? nullptr : found->second;
        if (!item) {
            item = new QStandardItem;
            parent->appendRow({item, new QStandardItem});
            items.emplace(node.key, item);
        }
        item->setText(node.name);
        item->setData(node.key, Key);
        item->setData(int(node.kind), Kind);
        item->setData(node.volumeId, VolumeId);
        item->setData(QVariant::fromValue(qulonglong(node.objectId)), ObjectId);
        item->setData(node.canActivate, CanActivate);
        item->setData(node.canRemove, CanRemove);
        item->setEditable(node.canRename);
        item->setCheckable(node.canCheck);
        if (node.canCheck) item->setCheckState(node.visible ? Qt::Checked : Qt::Unchecked);
        auto font = item->font();
        font.setBold(node.active);
        item->setFont(font);
        auto* detail = parent->child(item->row(), 1);
        detail->setText(node.detail);
        detail->setEditable(false);
    }
}

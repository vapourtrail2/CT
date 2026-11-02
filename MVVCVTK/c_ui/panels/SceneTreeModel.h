#pragma once
#include <QStandardItemModel>
#include <QString>
#include <cstdint>
#include <vector>

enum class SceneNodeKind { Group, Volume, Measurement, CropHistory };

struct SceneNode {
    QString key;
    QString parentKey;
    QString name;
    QString detail;
    SceneNodeKind kind = SceneNodeKind::Group;
    int volumeId = 0;
    std::uint64_t objectId = 0;
    bool active = false;
    bool visible = true;
    bool canActivate = false;
    bool canRename = false;
    bool canCheck = false;
    bool canRemove = false;
};

// The model knows node descriptions and capabilities, never SDK/measurement objects.
class SceneTreeModel final : public QStandardItemModel {
public:
    enum Role { Key = Qt::UserRole + 1, Kind, VolumeId, ObjectId, CanActivate, CanRemove };
    explicit SceneTreeModel(QObject* parent = nullptr);
    void syncNodes(const std::vector<SceneNode>& nodes);
    bool isSyncing() const { return syncing_; }
private:
    bool syncing_ = false;
};

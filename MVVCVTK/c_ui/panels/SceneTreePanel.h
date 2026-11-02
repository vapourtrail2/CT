#pragma once
#include "c_ui/context/SessionManager.h"
#include "c_ui/panels/SceneTreeModel.h"
#include <QWidget>

class QTreeView;
class QPushButton;

class SceneTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit SceneTreePanel(QWidget* parent = nullptr);
    void setDataState(bool hasData, const QString& sourcePath);
    void setSceneNodes(const std::vector<SceneNode>& nodes);
    void setCropTreeState(const CropTreeState& state);
signals:
    void volumeActivated(int volumeId);
    void objectSelected(int kind, int volumeId, qulonglong objectId);
    void objectVisibilityChanged(int volumeId, qulonglong objectId, bool visible);
    void objectRenamed(int volumeId, qulonglong objectId, QString name);
    void objectRemoveRequested(int volumeId, qulonglong objectId);
    void cropNodeActivated(qulonglong nodeId);
    void cropKeepInsideRequested();
    void cropRemoveInsideRequested();
    void cropApplyRequested();
    void cropRestoreOriginalRequested();
    void cropExitRequested();
private:
    QTreeView* tree_ = nullptr;
    SceneTreeModel* model_ = nullptr;
    QWidget* cropControls_ = nullptr;
    QPushButton* keepInsideButton_ = nullptr;
    QPushButton* removeInsideButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
    QPushButton* exitButton_ = nullptr;
};

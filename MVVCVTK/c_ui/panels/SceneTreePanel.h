#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <memory>
#include "c_ui/context/DataSet.h"
#include "App/AppState.h"

class SceneTreePanel : public QWidget
{
    Q_OBJECT
public:
    explicit SceneTreePanel(QWidget* parent = nullptr);

    void setSession(const Dataset& dataset);

private:
    void rebuildTree(const Dataset& dataset);
    void syncVisibility();
    void clearTree();
    void onItemChanged(QTreeWidgetItem* item, int column);  

    //需要修改这些对象
    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* root_ = nullptr;
    QTreeWidgetItem* volumeItem_ = nullptr;
    QTreeWidgetItem* helpersItem_ = nullptr;
    QTreeWidgetItem* clipPlanesItem_ = nullptr;
    QTreeWidgetItem* crosshairItem_ = nullptr;
    QTreeWidgetItem* axesItem_ = nullptr;
    std::shared_ptr<SharedInteractionState> state_;
	std::shared_ptr<SharedStateBroadcaster> broadcaster_;
    std::shared_ptr<void> lifeToken_;
};

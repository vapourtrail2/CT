#pragma once

#include "c_ui/context/SessionManager.h"

#include <QString>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

class SceneTreePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneTreePanel(QWidget* parent = nullptr);

    void setDataState(
        bool hasData,
        const QString& sourcePath);
    void setCropTreeState(const CropTreeState& state);

signals:
    void cropNodeActivated(qulonglong nodeCount);
    void cropKeepInsideRequested();
    void cropRemoveInsideRequested();
    void cropApplyRequested();
    void cropRestoreOriginalRequested();
    void cropExitRequested();

private:
    void rebuildTree();
    void clearTree();

private:
    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* root_ = nullptr;
    QTreeWidgetItem* volumeItem_ = nullptr;
    QTreeWidgetItem* cropRoot_ = nullptr;
    QPushButton* keepInsideButton_ = nullptr;
    QPushButton* removeInsideButton_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QPushButton* restoreButton_ = nullptr;
    QPushButton* exitButton_ = nullptr;

    bool hasData_ = false;
    QString sourcePath_;
    CropTreeState cropTreeState_;
};

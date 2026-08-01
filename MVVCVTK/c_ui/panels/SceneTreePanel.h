#pragma once

#include <QString>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

class SceneTreePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SceneTreePanel(QWidget* parent = nullptr);

    void setDataState(
        bool hasData,
        const QString& sourcePath);

private:
    void rebuildTree();
    void clearTree();

private:
    QTreeWidget* tree_ = nullptr;
    QTreeWidgetItem* root_ = nullptr;
    QTreeWidgetItem* volumeItem_ = nullptr;

    bool hasData_ = false;
    QString sourcePath_;
};

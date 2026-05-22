#pragma once
#include <QWidget>
#include <QPointer>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QDebug>
#include <QDialog>
#include <array>

class QVBoxLayout;//为什么要加前置声明
class QStackedWidget;
class QDoubleSpinBox;

class DocumentPage : public QWidget
{
    Q_OBJECT
public:
    explicit DocumentPage(QWidget* parent = nullptr);
    void notifySucc();
	void notifyFail(const QString& reason);
    void closeOpenDialog();

signals:
    void requestSwitchTo(const QString& page);
    void moduleClicked(const QString& info);
    void recentOpenRequested(const QString& projectName);
	void openRequested(const QString& path
                       ,const std::array<float,3>& spacing
                       ,const std::array<float,3>& origin);
    
private:
    void buildUi();
    void buildLeftDock();
    QWidget* buildRightContent(QWidget* parent);
    void wireLeftDockSignals();
	void showOpenDialog();
    void updateStatusLabel(const QString& text, bool isError);
    void loadFilePath(const QString& path);
   
    QPointer<QListWidget> listNav_;
    QPointer<QPushButton> btnUndo_;
    QPointer<QPushButton> btnKeep_;
    QPointer<QPushButton> btnVisCheck_;
    QPointer<QPushButton> btnPorosity_;
    QPointer<QPushButton> btnMetrology_;
    QPointer<QPushButton> btnMaterial_;
    QPointer<QTableWidget> tableRecent_;
    QPointer<QLineEdit> inputPath_;
    QPointer<QPushButton> btnBrowse_;
    QPointer<QPushButton> btnLoad_;
    QPointer<QLabel> dicomStatusLabel_;
    QPointer<QDialog> docDialog_;

    //二级窗口
	QPointer<QStackedWidget> importStack_;

	QPointer<QPushButton> btnNext_;
	QPointer<QPushButton> btnBack_;

	//间距输入
    QPointer<QDoubleSpinBox> spacingX_;
	QPointer<QDoubleSpinBox> spacingY_;
	QPointer<QDoubleSpinBox> spacingZ_;

    //原点输入
	QPointer<QDoubleSpinBox> originX_;
	QPointer<QDoubleSpinBox> originY_;
	QPointer<QDoubleSpinBox> originZ_;
};


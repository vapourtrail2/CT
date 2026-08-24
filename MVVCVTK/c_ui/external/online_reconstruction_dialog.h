#pragma once

#include "reconstruction_result.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QPointer>

class QCloseEvent;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QThread;
class QTimer;
class QToolButton;
class QWidget;

namespace fdkui {

class ReconstructionWorker;

class OnlineReconstructionDialog final : public QDialog {
    Q_OBJECT

public:
    explicit OnlineReconstructionDialog(QWidget* parent = nullptr);
    ~OnlineReconstructionDialog() override;

    void setInitialConfigPath(const QString& path);

signals:
    void reconstructionCompleted(fdkui::ReconstructionResultPtr result);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void browseConfig();
    void startReconstruction();
    void cancelReconstruction();
    void toggleAdvanced(bool expanded);
    void updateVolumeEstimate();
    void updateElapsedDisplay();
    void onProgress(int state, quint32 expected, quint32 received, quint32 processed);
    void onSucceeded(fdkui::ReconstructionResultPtr result);
    void onFailed(const QString& message);
    void onCancelled();
    void onWorkerThreadFinished();

private:
    void buildUi();
    void setInputsEnabled(bool enabled);
    void appendLog(const QString& message);
    void setState(const QString& text, const QString& color);
    quint64 estimatedOutputBytes() const;

    QLineEdit* configPathEdit_{};
    QPushButton* browseButton_{};
    QSpinBox* sizeXSpin_{};
    QSpinBox* sizeYSpin_{};
    QSpinBox* sizeZSpin_{};
    QDoubleSpinBox* fovXSpin_{};
    QDoubleSpinBox* fovYSpin_{};
    QDoubleSpinBox* fovZSpin_{};
    QToolButton* advancedToggle_{};
    QWidget* advancedPanel_{};
    QSpinBox* binningSpin_{};
    QSpinBox* batchViewsSpin_{};
    QSpinBox* latencySpin_{};
    QLabel* estimateLabel_{};
    QLabel* stateLabel_{};
    QLabel* elapsedLabel_{};
    QLabel* frameCountsLabel_{};
    QProgressBar* progressBar_{};
    QTextEdit* logEdit_{};
    QPushButton* startButton_{};
    QPushButton* cancelButton_{};
    QPushButton* closeButton_{};
    QTimer* elapsedTimer_{};
    QElapsedTimer elapsedClock_;

    QPointer<QThread> workerThread_;
    QPointer<ReconstructionWorker> worker_;
    bool running_{false};
    bool cancellationRequested_{false};
    bool closeWhenFinished_{false};
    int collapsedHeight_{0};
};

} // namespace fdkui

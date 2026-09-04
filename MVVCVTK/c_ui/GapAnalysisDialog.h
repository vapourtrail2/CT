#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class SessionManager;

struct GapHostStartParams;

class GapAnalysisDialog final : public QDialog
{
public:
    explicit GapAnalysisDialog(
        SessionManager& sessionManager,
        QWidget* parent = nullptr);

private:
    void buildUi();
    void initializeFromCurrentView();
    void updateEdgeDistanceFilterVisibility();
    void startAnalysis();
    void toggleOverlay();
    void exitAnalysis();
    void refreshState();
    void setStatus(const QString& message, bool isError = false);
    GapHostStartParams buildStartParams() const;

private:
    SessionManager& sessionManager_;
    QComboBox* isoModeCombo_ = nullptr;
    QStackedWidget* isoValueStack_ = nullptr;
    QDoubleSpinBox* ratioSpin_ = nullptr;
    QDoubleSpinBox* absoluteIsoSpin_ = nullptr;
    QDoubleSpinBox* backgroundMeanSpin_ = nullptr;
    QDoubleSpinBox* materialMeanSpin_ = nullptr;
    QGroupBox* filterResultsGroup_ = nullptr;
    QDoubleSpinBox* minVolumeSpin_ = nullptr;
    QCheckBox* edgeDistanceCalculationCheck_ = nullptr;
    QGroupBox* edgeDistanceFilterGroup_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* startButton_ = nullptr;
    QPushButton* overlayButton_ = nullptr;
    QPushButton* exitButton_ = nullptr;
    QTimer* stateTimer_ = nullptr;
    int lastAnalysisState_ = -1;
};

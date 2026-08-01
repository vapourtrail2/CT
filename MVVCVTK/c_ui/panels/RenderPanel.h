#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QWidget>

#include "Host/Types/HostValueTypes.h"

class WorkSpaceUIState;

class RenderPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RenderPanel(QWidget* parent = nullptr);
    ~RenderPanel() override;

    void setWorkSpaceUIState(WorkSpaceUIState* state);

signals:
    void visibilityRequested(
        HostVisibilityParams visibility);
    

protected:
    void resizeEvent(QResizeEvent* e) override;
    

private:
    void applyHistogramPixmap();
    void rebuildHistogramPixmap();
    double currentScalarSpan() const;
    double currentWindowWidthMin() const;   
    double currentWindowWidthMax() const;
    double currentWindowCenterMin() const;
    double currentWindowCenterMax() const;
    double sliderToWindowWidth(int value) const;
    double sliderToWindowCenter(int value) const;
    int windowWidthToSlider(double value) const;
    int windowCenterToSlider(double value) const;

private:
    QLabel* histLabel_ = nullptr;
    QLabel* isoValueLabel_ = nullptr;
    QLabel* windowWidthLabel_ = nullptr;
    QLabel* windowCenterLabel_ = nullptr;
    QSlider* isoSlider_ = nullptr;
    QSlider* windowWidthSlider_ = nullptr;
    QSlider* windowCenterSlider_ = nullptr;
    QComboBox* renderMode_ = nullptr;
    QCheckBox* mprPlanesToggle_ = nullptr;
    QCheckBox* crosshairToggle_ = nullptr;
    QCheckBox* rulerAxesToggle_ = nullptr;

    bool updatingUi_ = false;

    double rangeMin_ = 0.0;
    double rangeMax_ = 1.0;

    QString histCachePath_;
    QPixmap histPixmap_;

    WorkSpaceUIState* workSpaceUISpace_ = nullptr;
};

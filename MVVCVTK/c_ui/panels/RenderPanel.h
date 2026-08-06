#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QWidget>

#include "Host/Types/HostValueTypes.h"
#include "App/AppTypes.h"

class RenderPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RenderPanel(QWidget* parent = nullptr);
    ~RenderPanel() override;

    void setDataState(bool hasData);
    void setWindowLevelState(
        double windowWidth,
        double windowCenter,
        double scalarMin,
        double scalarMax);

signals:
    void primary3DModeRequested(HostRenderMode mode);

    void visibilityRequested(HostVisibilityParams visibility);

    void windowLevelRequested(HostWindowLevelParams windowLevel);

private:
    void setConnect();
    void requestWindowLevelUpdate();
private:
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

    double windowWidth_ = 400.0;
    double windowCenter_ = 40.0;
    double windowWidthMin_ = 0.01;
    double windowWidthMax_ = 1.0;
    double windowCenterMin_ = 0.0;
    double windowCenterMax_ = 1.0;
    bool hasData_ = false;
    bool hasWindowLevelState_ = false;
};

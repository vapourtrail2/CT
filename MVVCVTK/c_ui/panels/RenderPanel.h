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

signals:
    void primary3DModeRequested(HostRenderMode mode);

    void visibilityRequested(HostVisibilityParams visibility);

private:
    void setConnect();
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
};

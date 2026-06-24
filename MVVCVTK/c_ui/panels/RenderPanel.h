#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPixmap>
#include <QSlider>
#include <QWidget>
#include <memory>

#include "App/AppState.h"


struct AppSession;
class VolumeAnalysisService;
class WorkSpaceUIState;

class RenderPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RenderPanel(QWidget* parent = nullptr);
    ~RenderPanel() override;

    void setSession(const std::shared_ptr<AppSession>& session);
    void setSharedState(const std::shared_ptr<SharedInteractionState>& state,
                        const std::shared_ptr<SharedStateBroadcaster>& broadcaster);//是否需要分开 分开成两个函数
    void setAnalysisService(const std::shared_ptr<VolumeAnalysisService>& analysis);
    void setWorkSpaceUIState(WorkSpaceUIState* state);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    void syncFromState(UpdateFlags flags);
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
    QCheckBox* clipPlanesToggle_ = nullptr;
    QCheckBox* crosshairToggle_ = nullptr;
    QCheckBox* rulerAxesToggle_ = nullptr;


    std::shared_ptr<SharedInteractionState> state_;
	std::shared_ptr<SharedStateBroadcaster> broadcaster_;
    std::shared_ptr<VolumeAnalysisService> analysis_;
    std::shared_ptr<void> lifeToken_;

    bool updatingUi_ = false;

    double rangeMin_ = 0.0;
    double rangeMax_ = 1.0;

    QString histCachePath_;
    QPixmap histPixmap_;

    WorkSpaceUIState* workSpaceUISpace_ = nullptr;
};

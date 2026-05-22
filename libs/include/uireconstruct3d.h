#pragma once
#pragma execution_character_set("utf-8")

#include "uireconstruct3d_global.h"
#include <QWidget>
#include <QShowEvent>

namespace Recon3D
{
	class MainWidget;
}

class UIRECONSTRUCT3D_EXPORT UIReconstruct3D : public QWidget
{
	Q_OBJECT

public:
	UIReconstruct3D(QWidget* parent = nullptr);
	~UIReconstruct3D();

	void getReconData(float*& data, std::array<float, 3>& spacing, std::array<float, 3>& origin, std::array<int, 3>& outSize);

protected:
	void showEvent(QShowEvent* event) override;

signals:
	void reconFinished();

private:
	Recon3D::MainWidget* m_mainWidget = nullptr;
};



#pragma once

#include "App/AppTypes.h"
#include <QObject>

class WorkSpaceUIState : public QObject {
	Q_OBJECT
public:

	explicit WorkSpaceUIState(QObject* parent = nullptr);

	VizMode getPrimary3DMode() const;
	void setPrimary3DMode(VizMode mode);


signals:
	void primary3DModeChanged(VizMode mode);


private:
	VizMode primary3DMode_ = VizMode::CompositeIsoSurface;
};
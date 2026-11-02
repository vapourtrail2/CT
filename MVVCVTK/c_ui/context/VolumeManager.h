#pragma once
#include "MVVCVTK/SPI/Host/TrustedDataPort.h"
#include <QString>
#include <map>
#include <memory>
#include <array>
#include <optional>
#include "MVVCVTK/API/Host/Types/HostValueTypes.h"

namespace measure { class MeasurementSession; }

struct VolumeRecord
{
	int id = 0;
	QString name;
	VtkImageGridSnapshot image;
	int VolumeSourceid = 0; // 来源体积
    std::shared_ptr<measure::MeasurementSession> measurements;
    std::array<std::optional<HostWindowLevelParams>, 3> windowLevels;
    std::optional<std::array<double, 3>> cursorWorld;
};

class VolumeManager {
public:
	int addVolume(QString name,VtkImageGridSnapshot image,int VolumeSourceid = 0);
	VolumeRecord& getVolume(int id);//没人会改为什么加const 这个引用又是啥意思，不加引用会如何
	std::map<int, VolumeRecord>& getVolumes();
	int getActiveVolumeid();
	void setActiveVolumeid(int id);

private:
	std::map<int, VolumeRecord> volumes_;
	int nextVolumeid_ = 1;
	int activeVolumeid_ = 0;
};

#include "VolumeManager.h"
#include <utility>
#include "measure/MeasurementSession.h"

int VolumeManager::addVolume(QString name, VtkImageGridSnapshot image, int VolumeSourceid)
{
	int id = nextVolumeid_;
	VolumeRecord record{ id, std::move(name), std::move(image), VolumeSourceid };
    record.measurements = std::make_shared<measure::MeasurementSession>();
	volumes_.emplace(id,std::move (record));
	nextVolumeid_++;
	return id;
}

VolumeRecord& VolumeManager::getVolume(int id)
{
	return volumes_.at(id);
}

std::map<int, VolumeRecord>& VolumeManager::getVolumes()
{
	return volumes_;
}

int VolumeManager::getActiveVolumeid()
{
	return activeVolumeid_;
}

void VolumeManager::setActiveVolumeid(int id)
{
	activeVolumeid_ = id;
}

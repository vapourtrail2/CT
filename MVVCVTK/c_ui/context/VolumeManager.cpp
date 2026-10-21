#include "VolumeManager.h"
#include <utility>

int VolumeManager::addVolume(QString name, TrustedImageSnapshot image, int VolumeSourceid)
{
	int id = nextVolumeid_;
	VolumeRecord record{ id,std::move(name),std::move(image),std::move(VolumeSourceid) };
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

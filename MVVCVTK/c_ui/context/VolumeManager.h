#pragma once
#include "MVVCVTK/SPI/Data/TrustedImageState.h"
#include <QString>
#include <map>

struct VolumeRecord
{
	int id = 0;
	QString name; 
	TrustedImageSnapshot image;
	int VolumeSourceid = 0;//来源体积
};

class VolumeManager {
public:
	int addVolume(QString name,TrustedImageSnapshot image,int VolumeSourceid = 0);
	VolumeRecord& getVolume(int id);//没人会改为什么加const 这个引用又是啥意思，不加引用会如何
	std::map<int, VolumeRecord>& getVolumes();
	int getActiveVolumeid();
	void setActiveVolumeid(int id);
	
private:
	std::map<int, VolumeRecord> volumes_;
	int nextVolumeid_ = 1;
	int activeVolumeid_ = 0;
};


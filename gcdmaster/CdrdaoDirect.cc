#include <exception>

#include "dao/cdrdao.h"
#include "dao/CdrDriver.h"
#include "CdrdaoDirect.h"
#include "trackdb/Toc.h"
#include "CdDevice.h"

FastTocRead::FastTocRead(CdDevice* dev)
    : dev_(dev)
{
    if (!dev_) {
	for (auto* dev : CdDevice::deviceList()) {
	    if (dev->updateStatus() != 0)
		continue;
	    if (dev->status() != CdDevice::DEV_READY)
		continue;
	    dev_ = dev;
	    break;
	}
    }
    if (!dev_)
	throw std::runtime_error("No Available Disks to read");
}

FastTocRead::~FastTocRead()
{
    if (driver_)
	delete driver_;
}

void FastTocRead::run()
{
    Cdrdao cdrdao;

    driver_ = dev_->cdDriver();
    driver_->subChanReadMode(TrackData::SUBCHAN_NONE);
    driver_->rawDataReading(true);
    driver_->mode2Mixed(false);
    driver_->fastTocReading(true);
    driver_->taoSource(false);

    toc_ = driver_->readDiskToc(1);
}

void FastTocRead::completed()
{
    signalDone.emit(toc_);
}

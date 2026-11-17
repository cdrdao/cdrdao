#include <exception>

#include "dao/cdrdao.h"
#include "dao/CdrDriver.h"
#include "CdrdaoDirect.h"
#include "trackdb/Toc.h"
#include "CdDevice.h"

FastTocRead::FastTocRead(CdDevice *dev) : dev_(dev)
{
    if (!dev_) {
        // Use the cached status (kept current by background polling)
        // rather than querying drives from the GUI thread.
        for (auto *dev : CdDevice::deviceList()) {
            if (dev->busy() || dev->status() != CdDevice::DEV_READY)
                continue;
            dev_ = dev;
            break;
        }
    }
    if (!dev_)
        throw std::runtime_error("No Available Disks to read");

    // Use a private driver instance, rather than the device's, so that
    // the settings below don't leak into it and so that it cannot be
    // destroyed under us by the main thread.
    driver_ = CdrDriver::createDriver(dev_->driver().c_str(), dev_->driverOptions(),
                                      dev_->scsiIf());
    if (!driver_)
        throw std::runtime_error("Cannot create driver");
}

FastTocRead::~FastTocRead()
{
    delete driver_;
}

void FastTocRead::run()
{
    Cdrdao cdrdao;

    std::unique_lock<std::timed_mutex> lock(dev_->ioMutex(), std::try_to_lock);
    if (!lock.owns_lock())
        throw std::runtime_error("Device is busy");

    driver_->subChanReadMode(TrackData::SUBCHAN_NONE);
    driver_->rawDataReading(true);
    driver_->mode2Mixed(false);
    driver_->fastTocReading(true);
    driver_->taoSource(false);

    toc_ = driver_->readDiskToc(1);

    dev_->scsiIf()->release();
}

void FastTocRead::completed()
{
    signalDone.emit(toc_);
}

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>
#include <cerrno>

#include "CdDevice.h"
#include "dao/ScsiIf.h"
#include "dao/CdrDriver.h"
#include "ProcessMonitor.h"
#include "xcdrdao.h"
#include "trackdb/log.h"
#include "guiUpdate.h"
#include "TocEdit.h"
#include "ConfigManager.h"
#include "ProgressDialog.h"
#include "TaskManager.h"
#include "log.h"

#include <glibmm/i18n.h>

#define DRIVER_DEFAULT "generic-mmc"

std::vector<CdDevice *> CdDevice::DEVICE_LIST = {};
std::vector<CdDevice *> CdDevice::REMOVED_LIST = {};

namespace
{

// SCSI timeouts (seconds) for background I/O. Short, so that a wedged
// drive doesn't hold a worker thread (and, through the ordered task
// completion, other drives' results) for long.
constexpr int PROBE_TIMEOUT_S = 10;
constexpr int STATUS_TIMEOUT_S = 5;

// Delays before retrying a failed probe, in milliseconds.
constexpr std::array<int, 3> PROBE_RETRY_DELAYS_MS = {300, 1000, 3000};

// How long to wait, on the main thread, for in-process I/O on a drive to
// finish before handing it to a cdrdao process. Status polls take
// milliseconds; this only matters for a wedged drive or a TOC read.
constexpr int IO_HANDOFF_TIMEOUT_MS = 3000;

// Interval of the drive status polls. A TEST UNIT READY is cheap and
// doesn't spin up the drive, so this can be short.
constexpr int STATUS_POLL_INTERVAL_MS = 1000;

std::unique_ptr<DeviceMonitor> MONITOR;
sigc::connection STATUS_TIMER;

// Worker threads for device probing and status polling. Separate from
// the per-project task managers, so that device I/O never delays them.
// Created on first use, on the main thread, and intentionally never
// destroyed: tasks may still be running when the application exits.
TaskManager *deviceTasks()
{
    static TaskManager *tm = new TaskManager(2);
    return tm;
}

} // namespace

sigc::signal<void()> &CdDevice::signalListChanged()
{
    static sigc::signal<void()> sig;
    return sig;
}

sigc::signal<void(CdDevice *)> &CdDevice::signalStatusChanged()
{
    static sigc::signal<void(CdDevice *)> sig;
    return sig;
}

// Identifies a drive and selects its driver. Holds the device's I/O mutex
// while running.
class DeviceProbeTask : public Task
{
  public:
    DeviceProbeTask(CdDevice *dev, bool force)
        : dev_(dev), sif_(dev->scsiIf_), node_(dev->dev_), vendor_(dev->vendor_),
          product_(dev->product_), force_(force),
          lastAttempt_(dev->probeAttempt_ >= PROBE_RETRY_DELAYS_MS.size())
    {
    }

    void run() override
    {
        // Never scheduled while a cdrdao process owns the drive (see
        // probe()); if one starts meanwhile, releaseDevice() waits for us.
        std::lock_guard<std::timed_mutex> lock(dev_->ioMutex_);

        if (!sif_)
            return;

        // A device node that just appeared may not have its final
        // permissions yet (udev sets them after the kernel announces the
        // device). Fail silently then, unless this is the last retry, so
        // that init() doesn't log an error for a transient condition.
        if (!lastAttempt_ && access(node_.c_str(), R_OK) != 0 && errno == EACCES)
            return;

        int oldTimeout = sif_->timeout(PROBE_TIMEOUT_S);

        if (sif_->init() == 0) {
            ok_ = true;

            const char *detected =
                CdrDriver::selectDriver(1, vendor_.c_str(), product_.c_str(), &options_);

            cd_page_2a *p2a = sif_->checkMmc();
            if (p2a) {
                if (p2a->cd_r_read)
                    type_ = CdDevice::CD_ROM;
                if (p2a->cd_r_write)
                    type_ = CdDevice::CD_R;
                if (p2a->cd_rw_write)
                    type_ = CdDevice::CD_RW;
            }

            if (!detected) {
                // Same fallback as the cdrdao CLI (CdrDriver::detectDriver):
                // a generic MMC drive gets CD-TEXT writing enabled.
                options_ = 0;
                detected = CdrDriver::detectDriver(sif_, &options_);
            }
            if (detected)
                driver_ = detected;
        }

        sif_->timeout(oldTimeout);
        sif_->release();
    }

    void completed() override
    {
        dev_->ioPending_ = false;

        if (dev_->present_ && !ok_ && dev_->probeAttempt_ < PROBE_RETRY_DELAYS_MS.size()) {
            // Typically a drive that was just plugged in: the kernel
            // announces it before udev has set the permissions of its
            // device node, or before the drive answers INQUIRY. Retry
            // shortly, keeping the device hidden meanwhile.
            int delay = PROBE_RETRY_DELAYS_MS[dev_->probeAttempt_++];
            CdDevice *dev = dev_;
            bool force = force_;
            Glib::signal_timeout().connect_once([dev, force] { dev->probe(force); }, delay);
            dev_->runDeferredProbe();
        } else if (dev_->present_) {
            dev_->probeAttempt_ = 0;
            dev_->initOk_ = ok_;

            if (!dev_->manuallyConfigured_ || force_) {
                dev_->driver_ = driver_;
                dev_->driverOptions_ = options_;
                dev_->deviceType_ = type_;
                dev_->manuallyConfigured_ = false;
            }
            dev_->probed_ = true;
            dev_->createDriver();

            log_message(1, "Probed %s (%s): %s, driver %s 0x%lx", dev_->dev_.c_str(),
                        dev_->description_.c_str(), ok_ ? "ok" : "failed", dev_->driver_.c_str(),
                        dev_->driverOptions_);

            CdDevice::signalListChanged().emit();
            guiUpdate(UPD_CD_DEVICES);

            dev_->runDeferredProbe();
            dev_->pollStatus();
        }

        delete this;
    }

  private:
    CdDevice *dev_;
    std::shared_ptr<ScsiIf> sif_;
    std::string node_;
    std::string vendor_;
    std::string product_;
    bool force_;
    bool lastAttempt_;

    // Results
    bool ok_ = false;
    std::string driver_ = DRIVER_DEFAULT;
    unsigned long options_ = 0;
    CdDevice::DeviceType type_ = CdDevice::CD_R;
};

// Sends TEST UNIT READY to a drive.
class DeviceStatusTask : public Task
{
  public:
    DeviceStatusTask(CdDevice *dev) : dev_(dev), sif_(dev->scsiIf_) {}

    void run() override
    {
        // Don't wait behind a long operation such as a TOC read: the
        // status will be refreshed at the next poll.
        std::unique_lock<std::timed_mutex> lock(dev_->ioMutex_, std::try_to_lock);
        if (!lock.owns_lock() || !sif_) {
            skipped_ = true;
            return;
        }

        int oldTimeout = sif_->timeout(STATUS_TIMEOUT_S);
        int ret = CdrDriver::unitReady(*sif_, false);
        sif_->timeout(oldTimeout);
        sif_->release();

        switch (ret) {
        case CdrDriver::TUR_READY:
        case CdrDriver::TUR_STOPPED: // disc present, will be spun up on use
            status_ = CdDevice::DEV_READY;
            break;
        case CdrDriver::TUR_NO_DISC:
            status_ = CdDevice::DEV_NO_DISC;
            break;
        case CdrDriver::TUR_TRAY_OPEN:
            status_ = CdDevice::DEV_TRAY_OPEN;
            break;
        case CdrDriver::TUR_BECOMING_READY:
            status_ = CdDevice::DEV_SPINNING_UP;
            break;
        case CdrDriver::TUR_BAD_DISC:
            status_ = CdDevice::DEV_BAD_DISC;
            break;
        case CdrDriver::TUR_NOT_READY:
        case CdrDriver::TUR_UNIT_ATTENTION:
            status_ = CdDevice::DEV_BUSY;
            break;
        case CdrDriver::TUR_ERROR:
        default:
            status_ = CdDevice::DEV_FAULT;
            break;
        }
    }

    void completed() override
    {
        dev_->ioPending_ = false;

        // Discard the result if the drive went away or a cdrdao process
        // took it over in the meantime.
        if (!skipped_ && dev_->present_ && !dev_->busy())
            dev_->setStatus(status_);

        dev_->runDeferredProbe();
        delete this;
    }

  private:
    CdDevice *dev_;
    std::shared_ptr<ScsiIf> sif_;
    bool skipped_ = false;
    CdDevice::Status status_ = CdDevice::DEV_UNKNOWN;
};

std::vector<std::string> CdDevice::DRIVER_NAMES = {
    "Undefined",    "cdd2600",      "generic-mmc",  "generic-mmc-raw", "plextor",
    "plextor-scan", "ricoh-mp6200", "sony-cdu920",  "sony-cdu948",     "taiyo-yuden",
    "teac-cdr55",   "toshiba",      "yamaha-cdr10x"};

std::vector<std::string> CdDevice::DEV_TYPE_NAMES = {"CD-R", "CD-RW", "CD-ROM"};

std::vector<std::string> CdDevice::STATUS_NAMES = {
    "Ready", "Recording", "Reading",       "Waiting", "Blanking",
    "Busy",  "No disk",   "Not available", "In Use",  "Unknown"};

CdDevice::CdDevice(const std::string &dev, const std::string &vendor, const std::string &product)
{
    dev_ = dev;
    vendor_ = vendor;
    product_ = product;
    description_ = vendor_ + std::string(" ") + product_;

    // Never claim the drive exclusively: gcdmaster only polls it, and must
    // not get in the way of a cdrdao child process or an automounter.
    scsiIf_ = ScsiIf::create(dev_);
    if (scsiIf_)
        scsiIf_->setExclusive(false);

    // Identification (driver, type) is done asynchronously by probe().
}

CdDevice::~CdDevice()
{
    if (cdDriver_)
        delete cdDriver_;
}

Glib::ustring CdDevice::settingString() const
{
    char buf[100];
    Glib::ustring s;

    s = "'" + dev_ + "','";
    s += vendor_;
    s += "','";
    s += product_;
    s += "',";

    switch (deviceType_) {
    case CD_R:
        s += "CD_R";
        break;
    case CD_RW:
        s += "CD_RW";
        break;
    case CD_ROM:
        s += "CD_ROM";
        break;
    }

    s += ",";

    s += driver_;

    s += ",";

    snprintf(buf, sizeof(buf), "0x%lx", driverOptions_);
    s += buf;

    return s;
}

void CdDevice::status(Status s)
{
    setStatus(s);
}

void CdDevice::setStatus(Status s)
{
    if (s == status_)
        return;

    log_message(1, "%s: status %s -> %s", dev_.c_str(), status2string(status_).c_str(),
                status2string(s).c_str());
    status_ = s;
    signalStatusChanged().emit(this);
}

int CdDevice::exitStatus() const
{
    return exitStatus_;
}

bool CdDevice::busy() const
{
    return process_ != nullptr || status_ == DEV_RECORDING || status_ == DEV_READING ||
           status_ == DEV_BLANKING;
}

void CdDevice::probe(bool force)
{
    if (!present_)
        return;

    if (ioPending_ || busy()) {
        // Run it once the pending task is done, or once the cdrdao
        // process using the drive has exited.
        probeRequested_ = true;
        probeForce_ = probeForce_ || force;
        return;
    }

    ioPending_ = true;
    deviceTasks()->addJob(new DeviceProbeTask(this, force));
}

void CdDevice::runDeferredProbe()
{
    if (probeRequested_ && !ioPending_ && !busy()) {
        bool force = probeForce_;
        probeRequested_ = probeForce_ = false;
        probe(force);
    }
}

void CdDevice::pollStatus()
{
    // Drives that could not be opened (e.g. no permission) are left
    // alone, reported as not available.
    if (!present_ || !probed_ || !initOk_ || ioPending_ || busy())
        return;

    ioPending_ = true;
    deviceTasks()->addJob(new DeviceStatusTask(this));
}

bool CdDevice::checkProcess()
{
    if (process_ == NULL || !process_->exited())
        return false;

    exitStatus_ = process_->exitStatus();

    progressStatusChanged_ = 1;

    PROCESS_MONITOR->remove(process_);
    process_ = NULL;

    setStatus(DEV_UNKNOWN);
    reclaimDevice();

    if (slaveDevice_ != NULL) {
        slaveDevice_->setStatus(DEV_UNKNOWN);
        slaveDevice_->reclaimDevice();
        slaveDevice_->runDeferredProbe();
        slaveDevice_->pollStatus();
        slaveDevice_ = NULL;
    }
    signalProcessFinished.emit(this);

    // The drive is ours again: find out what state it was left in.
    runDeferredProbe();
    pollStatus();

    return true;
}

bool CdDevice::updateProgress(Glib::IOCondition cond, int fd)
{
    static unsigned char msgSync[4] = {0xff, 0x00, 0xff, 0x00};
    fd_set fds;
    int state = 0;
    unsigned char buf[10];
    struct timeval timeout = {0, 0};

    if (process_ == NULL)
        return false;

    if ((cond & Glib::IOCondition::IO_IN) != Glib::IOCondition::IO_IN)
        return false;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    while (select(fd + 1, &fds, NULL, NULL, &timeout) > 0 && FD_ISSET(fd, &fds)) {
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        state = 0;

        while (state < 4) {
            if (read(fd, buf, 1) != 1) {
                // message(-2, "Reading of msg sync failed.");
                return false;
            }

            if (buf[0] == msgSync[state]) {
                state++;
            } else {
                state = 0;

                if (buf[0] == msgSync[state]) {
                    state++;
                }
            }
        }

        ProgressMsg msg;

        int msgsize = read(fd, (char *)&msg, sizeof(msg));
        if (msgsize >= PSGMSG_MINSIZE) {
            if (msg.status >= PGSMSG_MIN && msg.status <= PGSMSG_MAX && msg.track >= 0 &&
                msg.totalProgress >= 0 && msg.totalProgress <= 1000 && msg.bufferFillRate >= 0 &&
                msg.bufferFillRate <= 100) {
                progressStatus_ = msg.status;
                progressTotalTracks_ = msg.totalTracks;
                progressTrack_ = msg.track;
                progressTrackRelative_ = msg.trackProgress;
                progressTotal_ = msg.totalProgress;
                progressBufferFill_ = msg.bufferFillRate;
                if (msgsize == sizeof(msg))
                    progressWriterFill_ = msg.writerFillRate;
                else
                    progressWriterFill_ = 0;

                progressStatusChanged_ = 1;
            }
        } else {
            log_message(-1, _("Reading of progress message failed."));
        }
    }

    if (progressStatusChanged_)
        guiUpdate(UPD_PROGRESS_STATUS);

    return true;
}

CdDevice::DeviceType CdDevice::deviceType() const
{
    return deviceType_;
}

void CdDevice::deviceType(DeviceType t)
{
    deviceType_ = t;
}

unsigned long CdDevice::driverOptions() const
{
    return driverOptions_;
}

void CdDevice::driverOptions(unsigned long o)
{
    driverOptions_ = o;
}

bool CdDevice::ejectCd(bool load)
{
    bool success = false;

    // Not while a cdrdao process owns the drive (it may have exited
    // but not been reaped yet, in which case ioMutex_ is still held).
    if (busy())
        return false;

    std::unique_lock<std::timed_mutex> lock(ioMutex_, std::defer_lock);
    if (cdDriver_ && lock.try_lock_for(std::chrono::milliseconds(IO_HANDOFF_TIMEOUT_MS))) {
        int ret = cdDriver_->loadUnload((load ? 0 : 1));
        success = (ret == 0);
        scsiIf_->release();
    }
    pollStatus();

    return success;
}

// Create or recreate Driver.
//
void CdDevice::createDriver()
{
    if (cdDriver_) {
        delete cdDriver_;
        cdDriver_ = nullptr;
    }
    // The driver reads the transfer size from the interface, so it can
    // only be created once the interface has been initialized by probe().
    if (!scsiIf_ || !initOk_ || driver_.empty()) {
        setStatus(DEV_FAULT);
        return;
    }

    cdDriver_ = CdrDriver::createDriver(driver_.c_str(), driverOptions_, scsiIf_);
    if (!cdDriver_)
        status(DEV_FAULT);
}

bool CdDevice::releaseDevice()
{
    if (!processLock_.owns_lock()) {
        processLock_ = std::unique_lock<std::timed_mutex>(ioMutex_, std::defer_lock);
        if (!processLock_.try_lock_for(std::chrono::milliseconds(IO_HANDOFF_TIMEOUT_MS))) {
            log_message(-2, _("Device %s is busy, please try again."), dev_.c_str());
            return false;
        }
    }

    createDriver();
    if (scsiIf_)
        scsiIf_->release();
    return true;
}

void CdDevice::reclaimDevice()
{
    if (processLock_.owns_lock())
        processLock_.unlock();
}

// Starts a 'cdrdao' for recording given toc. Returns false if an
// error occured and the process was not successfully launched.
bool CdDevice::recordDao(Gtk::Window &parent, TocEdit *tocEdit, int simulate, int multiSession,
                         int speed, int eject, int reload, int buffer, int overburn)
{
    std::string tocFileName;
    std::vector<std::string> args;
    char devname[30];
    char drivername[50];
    char speedbuf[20];
    char bufferbuf[20];

    // Media states (no disc, spinning up...) are left to cdrdao, which
    // waits for the drive and reports a meaningful error.
    if (busy())
        return false;

    try {
        auto tempPath = std::filesystem::temp_directory_path();
        std::string fileName = "cdrdao_" + std::to_string(getpid()) + "_" +
                               std::to_string(std::time(nullptr)) + ".gcdm.toc";
        tempPath /= fileName;
        tocFileName = tempPath.string();
        log_message(1, "Creating temporary TOC file \"%s\"", tocFileName.c_str());
    } catch (...) {
        log_message(-2, _("Failed to determine temporary directory."));
        return false;
    }

    // Write out temporary toc file containing all the converted wav
    // files (don't want to rely on cdrdao doing the mp3->wav
    // translation, besides it's already been done).
    if (tocEdit->toc()->write(tocFileName, true != 0)) {
        log_message(-2, _("Cannot write temporary toc-file."));
        return false;
    }

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();
    int remoteFdArgNum = 3;

    args.assign(
        {execName, (simulate ? "simulate" : "write"), "--remote", "", "-v0", "--device", dev_});

    if (multiSession)
        args.push_back("--multi");

    if (speed > 0) {
        snprintf(speedbuf, sizeof(speedbuf), "%d", speed);
        args.push_back("--speed");
        args.push_back(speedbuf);
    }

    if (eject)
        args.push_back("--eject");

    if (reload)
        args.push_back("--reload");

    if (overburn)
        args.push_back("--overburn");

    if (!driver_.empty()) {
        snprintf(drivername, sizeof(drivername), "%s:0x%lx", driver_.c_str(), driverOptions_);
        args.push_back("--driver");
        args.push_back(drivername);
    }

    if (buffer >= 10) {
        snprintf(bufferbuf, sizeof(bufferbuf), "%i", buffer);
        args.push_back("--buffers");
        args.push_back(bufferbuf);
    }

    args.push_back(tocFileName);

    if (!releaseDevice()) {
        unlink(tocFileName.c_str());
        return false;
    }

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    if (process_ != NULL) {
        setStatus(DEV_RECORDING);
        action_ = A_RECORD;

        std::string cmdline;
        for (auto &c : args) {
            cmdline += c;
            cmdline += " ";
        }
        PROGRESS_POOL->start(parent, this, cmdline);

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }

        return true;
    } else {
        reclaimDevice();
        unlink(tocFileName.c_str());
        return false;
    }
}

void CdDevice::abortDaoRecording()
{
    if (process_ != NULL && !process_->exited()) {
        PROCESS_MONITOR->stop(process_);
    }
}

int CdDevice::progressStatusChanged()
{
    if (progressStatusChanged_) {
        progressStatusChanged_ = 0;
        return 1;
    }

    return 0;
}

void CdDevice::progress(int *status, int *totalTracks, int *track, int *trackProgress,
                        int *totalProgress, int *bufferFill, int *writerFill) const
{
    *status = progressStatus_;
    *totalTracks = progressTotalTracks_;
    *track = progressTrack_;
    *trackProgress = progressTrackRelative_;
    *totalProgress = progressTotal_;
    *bufferFill = progressBufferFill_;
    *writerFill = progressWriterFill_;
}

// Starts a 'cdrdao' for reading whole cd.
// Return: 0: OK, process succesfully launched
//         1: error occured
int CdDevice::extractDao(Gtk::Window &parent, const std::string &tocFile,
                         const std::string &dataFile, int correction, int readSubChanMode)
{
    std::vector<std::string> args;
    int n = 0;
    char devname[30];
    char drivername[50];
    const char *s;
    char correctionbuf[20];

    // Media states (no disc, spinning up...) are left to cdrdao, which
    // waits for the drive and reports a meaningful error.
    if (busy())
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    int remoteFdArgNum = 3;

    snprintf(correctionbuf, sizeof(correctionbuf), "%d", correction);

    args.assign({execName, "read-cd", "--remote", "", "-v0", "--read-raw", "--device", dev_,
                 "--paranoia-mode", correctionbuf, "--datafile", dataFile});

    switch (readSubChanMode) {
    case 1:
        args.push_back("--read-subchan");
        args.push_back("rw");
        break;
    case 2:
        args.push_back("--read-subchan");
        args.push_back("rw_raw");
        break;
    }

    if (!driver_.empty()) {
        snprintf(drivername, sizeof(drivername), "%s:0x%lx", driver_.c_str(), driverOptions_);
        args.push_back("--driver");
        args.push_back(drivername);
    }
    args.push_back(tocFile);

    if (!releaseDevice())
        return 1;

    process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

    if (process_) {
        setStatus(DEV_READING);
        action_ = A_READ;

        std::string cmdline;
        for (auto &c : args) {
            cmdline += c;
            cmdline += " ";
        }
        PROGRESS_POOL->start(parent, this, cmdline, false, false);

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(),
                Glib::IOCondition::IO_IN | Glib::IOCondition::IO_PRI | Glib::IOCondition::IO_ERR |
                    Glib::IOCondition::IO_HUP);
        }
        return 0;
    } else {
        reclaimDevice();
        return 1;
    }
}

void CdDevice::abortDaoReading()
{
    if (process_ != NULL && !process_->exited()) {
        PROCESS_MONITOR->stop(process_);
    }
}

// Starts a 'cdrdao' for duplicating a CD.
// Return: 0: OK, process succesfully launched
//         1: error occured
int CdDevice::duplicateDao(Gtk::Window &parent, int simulate, int multiSession, int speed,
                           int eject, int reload, int buffer, int onthefly, int correction,
                           int readSubChanMode, CdDevice *readdev)
{
    std::vector<std::string> args;
    int n = 0;
    char devname[30];
    char drivername[50];
    char r_drivername[50];
    char speedbuf[20];
    char correctionbuf[20];
    const char *s;
    char bufferbuf[20];

    if (readdev->busy())
        return 1;

    // Media states (no disc, spinning up...) are left to cdrdao, which
    // waits for the drive and reports a meaningful error.
    if (busy())
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    snprintf(correctionbuf, sizeof(correctionbuf), "%d", correction);
    int remoteFdArgNum = 3;

    args.assign({execName, "copy", "--remote", "", "-v0", "--paranoia-mode", correctionbuf,
                 "--device", dev_});

    if (simulate)
        args.push_back("--simulate");

    if (multiSession)
        args.push_back("--multi");

    if (speed > 0) {
        snprintf(speedbuf, sizeof(speedbuf), "%d", speed);
        args.push_back("--speed");
        args.push_back(speedbuf);
    }

    if (eject)
        args.push_back("--eject");

    if (reload)
        args.push_back("--reload");

    if (onthefly)
        args.push_back("--on-the-fly");

    switch (readSubChanMode) {
    case 1:
        args.push_back("--read-subchan");
        args.push_back("rw");
        break;

    case 2:
        args.push_back("--read-subchan");
        args.push_back("rw_raw");
        break;
    }

    if (!driver_.empty()) {
        snprintf(drivername, sizeof(drivername), "%s:0x%lx", driver_.c_str(), driverOptions_);
        args.push_back("--driver");
        args.push_back(drivername);
    }

    if (readdev != this) { // reader and write the same, skip source device

        args.push_back("--source-device");
        args.push_back(readdev->dev());

        if (!readdev->driver().empty()) {
            snprintf(r_drivername, sizeof(r_drivername), "%s:0x%lx", readdev->driver().c_str(),
                     readdev->driverOptions());
            args.push_back("--source-driver");
            args.push_back(r_drivername);
        }
    }
    if (buffer >= 10) {
        snprintf(bufferbuf, sizeof(bufferbuf), "%i", buffer);
        args.push_back("--buffers");
        args.push_back(bufferbuf);
    }

    if (!releaseDevice())
        return 1;
    if (readdev != this && !readdev->releaseDevice()) {
        reclaimDevice();
        return 1;
    }

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    if (process_ != NULL) {
        slaveDevice_ = readdev;
        slaveDevice_->setStatus(DEV_READING);
        setStatus(DEV_RECORDING);

        action_ = A_DUPLICATE;

        std::string cmdline;
        for (auto &c : args) {
            cmdline += c;
            cmdline += " ";
        }
        PROGRESS_POOL->start(parent, this, cmdline);

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }

        return 0;
    } else {
        reclaimDevice();
        if (readdev != this)
            readdev->reclaimDevice();
        return 1;
    }
}

void CdDevice::abortDaoDuplication()
{
    if (process_ != NULL && !process_->exited()) {
        PROCESS_MONITOR->stop(process_);
    }
}

// Starts a 'cdrdao' for blanking a CD.
// Return: 0: OK, process succesfully launched
//         1: error occured
int CdDevice::blank(Gtk::Window *parent, int fast, int speed, int eject, int reload)
{
    std::vector<std::string> args;
    int n = 0;
    char devname[30];
    char drivername[50];
    char speedbuf[20];
    const char *s;
    int remoteFdArgNum = 0;

    // Media states (no disc, spinning up...) are left to cdrdao, which
    // waits for the drive and reports a meaningful error.
    if (busy())
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    remoteFdArgNum = 3;

    args.assign({execName, "blank", "--remote", "", "-v0", "--blank-mode",
                 (fast ? "minimal" : "full"), "--device", dev_});

    if (speed > 0) {
        snprintf(speedbuf, sizeof(speedbuf), "%d", speed);
        args.push_back("--speed");
        args.push_back(speedbuf);
    }

    if (eject)
        args.push_back("--eject");

    if (reload)
        args.push_back("--reload");

    if (!driver_.empty()) {
        snprintf(drivername, sizeof(drivername), "%s:0x%lx", driver_.c_str(), driverOptions_);
        args.push_back("--driver");
        args.push_back(drivername);
    }

    if (!releaseDevice())
        return 1;

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    if (process_ != NULL) {
        setStatus(DEV_BLANKING);
        action_ = A_BLANK;

        std::string cmdline;
        for (auto &c : args) {
            cmdline += c;
            cmdline += " ";
        }
        PROGRESS_POOL->start(*parent, this, cmdline, false, false);

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }
        return 0;
    } else {
        reclaimDevice();
        return 1;
    }
}

void CdDevice::abortBlank()
{
    if (process_ != NULL && !process_->exited()) {
        PROCESS_MONITOR->stop(process_);
    }
}

CdDevice::DeviceType CdDevice::devtypeName2Id(const std::string &dt)
{
    int i = 0;

    for (auto str : deviceNames()) {
        if (str == dt)
            return static_cast<DeviceType>(i);
        i++;
    }
    return CdDevice::CD_R;
}

const std::string CdDevice::status2string(Status s)
{
    switch (s) {
    case DEV_READY:
        return ("Ready");
    case DEV_RECORDING:
        return ("Recording");
    case DEV_READING:
        return ("Reading");
    case DEV_SPINNING_UP:
        return ("Spinning Up");
    case DEV_BUSY:
        return ("Busy");
    case DEV_NO_DISC:
        return ("No Disc");
    case DEV_BLANKING:
        return ("Blanking");
    case DEV_FAULT:
        return ("Not Available");
    case DEV_LOCKED:
        return ("Already In Use");
    case DEV_TRAY_OPEN:
        return ("Tray Open");
    case DEV_BAD_DISC:
        return ("Incompatible Disc");
    case DEV_UNKNOWN:
    default:
        return ("Unknown");
    }
}

const std::string CdDevice::deviceType2string(DeviceType t)
{
    const char *ret = NULL;

    switch (t) {
    case CD_R:
        ret = "CD-R";
        break;

    case CD_RW:
        ret = "CD-RW";
        break;

    case CD_ROM:
        ret = "CD-ROM";
        break;
    }

    return ret;
}

// Reads configured devices from gnome settings. Only called once on
// application startup.
//
void CdDevice::importSettings()
{
    // Needs complete rerwite, current implementatio makes no sense as
    // the "dev" string will change, key needs to be the name and
    // model #.
}

/* saves manually configured devices as gnome settings
 */
void CdDevice::exportSettings()
{
    // Needs complete rerwite, current implementatio makes no sense as
    // the "dev" string will change, key needs to be the name and
    // model #.
}

CdDevice *CdDevice::add(const std::string &dev, const std::string &vendor,
                        const std::string &product)
{
    for (auto device : DEVICE_LIST) {
        if (device->dev() == dev)
            return device;
    }

    auto ent = new CdDevice(dev, vendor, product);
    DEVICE_LIST.push_back(ent);
    return ent;
}

static char *nextToken(char *&p)
{
    char *val = NULL;

    if (p == NULL || *p == 0)
        return NULL;

    while (*p != 0 && isspace(*p))
        p++;

    if (*p == 0)
        return NULL;

    if (*p == '\'') {
        p++;
        val = p;

        while (*p != 0 && *p != '\'')
            p++;

        if (*p == 0) {
            // error, no matching ' found
            return NULL;
        } else {
            *p++ = 0;

            // skip over ,
            while (*p != 0 && *p != ',')
                p++;

            if (*p == ',')
                p++;
        }
    } else {
        val = p;

        while (*p != 0 && *p != ',')
            p++;

        if (*p == ',')
            *p++ = 0;
    }

    return val;
}

static CdDevice *addImpl(const std::string &setting)
{
    char *s = (char *)alloca(setting.size() + 1);
    strcpy(s, setting.c_str());
    char *p;
    std::string driver;
    std::string dev;
    std::string vendor;
    std::string model;
    std::string device;
    unsigned long options;
    char *val;
    CdDevice::DeviceType type;
    CdDevice *cddev;

    if (s[0] != '\'')
        return NULL;

    p = s;

    if ((val = nextToken(p)) == NULL)
        return NULL;
    dev = val;

    if ((val = nextToken(p)) == NULL)
        return NULL;
    vendor = val;

    if ((val = nextToken(p)) == NULL)
        return NULL;
    model = val;

    if ((val = nextToken(p)) == NULL)
        return NULL;

    if (strcasecmp(val, "CD_R") == 0)
        type = CdDevice::CD_R;
    else if (strcasecmp(val, "CD_RW") == 0)
        type = CdDevice::CD_RW;
    else if (strcasecmp(val, "CD_ROM") == 0)
        type = CdDevice::CD_ROM;
    else
        type = CdDevice::CD_R;

    if ((val = nextToken(p)) == NULL)
        return NULL;
    driver = val;

    if ((val = nextToken(p)) == NULL)
        return NULL;
    options = strtoul(val, NULL, 0);

    cddev = CdDevice::add(dev, vendor, model);

    cddev->driver(driver);
    cddev->deviceType(type);
    cddev->driverOptions(options);

    return cddev;
}

CdDevice *CdDevice::add(const std::string &setting)
{
    CdDevice *dev = addImpl(setting);
    return dev;
}

CdDevice *CdDevice::find(const std::string devname)
{
    for (auto dev : DEVICE_LIST)
        if (dev->dev() == devname)
            return dev;

    return nullptr;
}
bool CdDevice::updateDeviceList(const std::vector<DeviceMonitor::DeviceInfo> &devices)
{
    bool changed = false;

    // Remove devices that are no longer present.
    for (auto it = DEVICE_LIST.begin(); it != DEVICE_LIST.end();) {
        bool present = false;
        for (const auto &info : devices) {
            if (info.node == (*it)->dev()) {
                present = true;
                break;
            }
        }
        if (present) {
            ++it;
        } else {
            CdDevice *dev = *it;
            log_message(0, "Device %s (%s) removed", dev->dev().c_str(),
                        dev->description().c_str());
            dev->present_ = false;
            if (!dev->busy())
                dev->setStatus(DEV_FAULT);
            if (dev->scsiIf_)
                dev->scsiIf_->release();
            REMOVED_LIST.push_back(dev);
            it = DEVICE_LIST.erase(it);
            changed = true;
        }
    }

    // Add devices that just appeared. Persisting entries keep their
    // existing object.
    for (const auto &info : devices) {
        if (find(info.node) == nullptr) {
            log_message(0, "Device %s (%s %s) added", info.node.c_str(), info.vendor.c_str(),
                        info.product.c_str());
            add(info.node, info.vendor, info.product)->probe();
            changed = true;
        }
    }

    return changed;
}

void CdDevice::startMonitor()
{
    if (MONITOR)
        return;

    // Load the driver table now, on the main thread: the lazy loading in
    // CdrDriver::selectDriver() is not thread-safe, and probes call it
    // from worker threads.
    unsigned long options = 0;
    CdrDriver::selectDriver(1, "", "", &options);

    MONITOR = DeviceMonitor::create();
    MONITOR->signal_changed().connect([] {
        if (updateDeviceList(MONITOR->devices())) {
            signalListChanged().emit();
            guiUpdate(UPD_CD_DEVICES);
        }
    });
    MONITOR->signal_media_changed().connect([] {
        for (auto dev : DEVICE_LIST)
            dev->pollStatus();
    });
    MONITOR->start();

    STATUS_TIMER = Glib::signal_timeout().connect(
        [] {
            for (auto dev : DEVICE_LIST)
                dev->pollStatus();
            return true;
        },
        STATUS_POLL_INTERVAL_MS);
}

void CdDevice::stopMonitor()
{
    STATUS_TIMER.disconnect();
    if (MONITOR) {
        MONITOR->stop();
        MONITOR.reset();
    }
}

void CdDevice::rescan()
{
    if (MONITOR)
        MONITOR->requestRescan();
}

int CdDevice::update()
{
    int status = 0;

    blockProcessMonitorSignals();

    // Removed devices may still have a cdrdao process to reap.
    for (auto list : {&DEVICE_LIST, &REMOVED_LIST})
        for (auto dev : *list)
            if (dev->checkProcess())
                status |= UPD_CD_DEVICE_STATUS;

    unblockProcessMonitorSignals();

    // Status polls run on their own timer, see startMonitor().
    return status;
}

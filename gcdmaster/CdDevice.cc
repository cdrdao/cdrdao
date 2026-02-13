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

#include <cassert>
#include <cstdlib>
#include <filesystem>

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
#include "log.h"

#include <glibmm/i18n.h>

#define DRIVER_DEFAULT "generic-mmc"

std::vector<CdDevice*> CdDevice::DEVICE_LIST = {};

std::vector<std::string> CdDevice::DRIVER_NAMES = {
	"Undefined",    "cdd2600",      "generic-mmc",  "generic-mmc-raw", "plextor",
	"plextor-scan", "ricoh-mp6200", "sony-cdu920",  "sony-cdu948",     "taiyo-yuden",
	"teac-cdr55",   "toshiba",      "yamaha-cdr10x"
};

std::vector<std::string> CdDevice::DEV_TYPE_NAMES = { "CD-R", "CD-RW", "CD-ROM" };

std::vector<std::string> CdDevice::STATUS_NAMES = {
    "Ready", "Recording", "Reading", "Waiting", "Blanking",
    "Busy", "No disk", "Not available", "In Use", "Unknown" };



CdDevice::CdDevice(const std::string& dev,
                   const std::string& vendor, const std::string& product)
{
    dev_ = dev;
    vendor_ = vendor;
    product_ = product;
    description_ = vendor_ + std::string(" ") + product_;

    autoSelectDriver();
    createDriver();
}

CdDevice::~CdDevice()
{
    if (cdDriver_)
        delete cdDriver_;
    if (scsiIf_)
        delete scsiIf_;
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
    status_ = s;
}

int CdDevice::exitStatus() const
{
    return exitStatus_;
}

int CdDevice::autoSelectDriver()
{
    unsigned long options = 0;
    const char *driverName;

    auto detected = CdrDriver::selectDriver(1, vendor_.c_str(), product_.c_str(), &options);

    if (detected) {
        driver_ = detected;
        driverOptions_ = options;
    } else {
        cd_page_2a *p2a;

        ScsiIf *sif = new ScsiIf(dev_.c_str());

        if (sif && sif->init() == 0 && (p2a = sif->checkMmc())) {

            driver_ = DRIVER_DEFAULT;
            if (p2a->cd_r_read)
                deviceType_ = CD_ROM;
            if (p2a->cd_r_write)
                deviceType_ = CD_R;
            if (p2a->cd_rw_write)
                deviceType_ = CD_RW;
        } else {
            driver_ = DRIVER_DEFAULT;
            driverOptions_ = 0;
        }
        if (sif)
            delete sif;
    }

    return 1;
}

int CdDevice::updateStatus()
{
    Status newStatus = status_;

    if (process_ != NULL) {
        if (process_->exited()) {
            newStatus = DEV_UNKNOWN;
            exitStatus_ = process_->exitStatus();

            progressStatusChanged_ = 1;

            PROCESS_MONITOR->remove(process_);
            process_ = NULL;

            if (slaveDevice_ != NULL) {
                slaveDevice_->status(DEV_UNKNOWN);
                slaveDevice_ = NULL;
            }
	    signalProcessFinished.emit(this);
        }
    }

    if (!cdDriver_) {
        status_ = DEV_FAULT;
    } else {
        switch (cdDriver_->testUnitReady()) {
        case 0:
            newStatus = DEV_READY;
            break;
        case 1:
            newStatus = DEV_FAULT;
            break;
        case 2:
            newStatus = DEV_BUSY;
            break;
        case 3:
            newStatus = DEV_NO_DISC;
            break;
        case 4:
            newStatus = DEV_TRAY_OPEN;
            break;
        case 5:
            newStatus = DEV_SPINNING_UP;
            break;
        case 6:
            newStatus = DEV_BAD_DISC;
            break;
        default:
            assert(-1);
        }
        if (status_ != newStatus)
            log_message(1, "TEST UNIT READY, status was %d,  is now %d",
                        status_, newStatus);
    }

    if (newStatus != status_) {
        status_ = newStatus;
        return 1;
    }

    return 0;
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

    if (cdDriver_) {
        int ret = cdDriver_->loadUnload((load ? 0 : 1));
        success = (ret == 0);
    }

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
    if (scsiIf_) {
        delete scsiIf_;
        scsiIf_ = nullptr;
    }

    scsiIf_ = new ScsiIf(dev_.c_str());

    if (!scsiIf_ or driver_.empty()) {
        status(DEV_FAULT);
	return;
    }

    cdDriver_ = CdrDriver::createDriver(driver_.c_str(), driverOptions_, scsiIf_);
    if (cdDriver_)
        status(DEV_FAULT);
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

    if ((status_ != DEV_READY && status_ != DEV_FAULT &&
         status_ != DEV_UNKNOWN && status_ != DEV_LOCKED) ||
        process_ != NULL)
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
	{ execName, (simulate ? "simulate" : "write"),
		"--remote", "", "-v0",
		"--device", dev_ });

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

    // Remove the SCSI interface of this device to avoid problems with double
    // usage of device nodes.
    createDriver();

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    std::string cmdline;
    for (auto &c : args) {
	cmdline += c;
	cmdline += " ";
    }
    PROGRESS_POOL->start(parent, this, cmdline);

    if (process_ != NULL) {
        status_ = DEV_RECORDING;
        action_ = A_RECORD;

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }

        return true;
    } else {
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
int CdDevice::extractDao(Gtk::Window &parent,
			 const std::string& tocFile, const std::string& dataFile,
			 int correction, int readSubChanMode)
{
    std::vector<std::string> args;
    int n = 0;
    char devname[30];
    char drivername[50];
    const char *s;
    char correctionbuf[20];

    if ((status_ != DEV_READY && status_ != DEV_FAULT &&
         status_ != DEV_UNKNOWN && status_ != DEV_LOCKED) ||
        process_ != nullptr)
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    int remoteFdArgNum = 3;

    snprintf(correctionbuf, sizeof(correctionbuf), "%d", correction);

    args.assign(
	{ execName, "read-cd", "--remote", "","-v0", "--read-raw",
	  "--device", dev_, "--paranoia-mode", correctionbuf,
	  "--datafile", dataFile});

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

    // Remove the SCSI interface of this device to avoid problems with double
    // usage of device nodes.
    createDriver();

    process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

    std::string cmdline;
    for (auto &c : args) {
	cmdline += c;
	cmdline += " ";
    }
    PROGRESS_POOL->start(parent, this, cmdline, false, false);

    if (process_) {
        status_ = DEV_READING;
        action_ = A_READ;

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(),
		Glib::IOCondition::IO_IN |
		Glib::IOCondition::IO_PRI |
		Glib::IOCondition::IO_ERR |
		Glib::IOCondition::IO_HUP);
        }
        return 0;
    } else {
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

    int rdstat = readdev->status();
    if ((rdstat != DEV_READY && rdstat != DEV_UNKNOWN && rdstat != DEV_FAULT) ||
        readdev->process() != NULL)
        return 1;

    if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN) ||
        process_ != NULL)
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    snprintf(correctionbuf, sizeof(correctionbuf), "%d", correction);
    int remoteFdArgNum = 3;

    args.assign(
	{ execName, "copy", "--remote", "", "-v0",
	  "--paranoia-mode", correctionbuf,
	  "--device", dev_ });

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
            snprintf(r_drivername, sizeof(r_drivername), "%s:0x%lx",
                     readdev->driver().c_str(), readdev->driverOptions());
            args.push_back("--source-driver");
            args.push_back(r_drivername);
        }
    }
    if (buffer >= 10) {
        snprintf(bufferbuf, sizeof(bufferbuf), "%i", buffer);
        args.push_back("--buffers");
        args.push_back(bufferbuf);
    }

    // Remove the SCSI interface of this device to avoid problems with double
    // usage of device nodes.
    createDriver();

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    std::string cmdline;
    for (auto &c : args) {
	cmdline += c;
	cmdline += " ";
    }
    PROGRESS_POOL->start(parent, this, cmdline);

    if (process_ != NULL) {
        slaveDevice_ = readdev;
        slaveDevice_->status(DEV_READING);
        status_ = DEV_RECORDING;

        action_ = A_DUPLICATE;

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }

        return 0;
    } else {
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

    if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN) ||
        process_ != NULL)
        return 1;

    Glib::ustring execName = CONFIG_MANAGER->getCdrdaoPath();

    remoteFdArgNum = 3;

    args.assign(
	{ execName,  "blank", "--remote", "", "-v0",
	  "--blank-mode", (fast ? "minimal" : "full"),
	  "--device", dev_ });

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
        snprintf(drivername, sizeof(drivername), "%s:0x%lx",
                 driver_.c_str(),
                 driverOptions_);
        args.push_back("--driver");
        args.push_back(drivername);
    }

    std::string cmdline;
    for (auto &c : args) {
	cmdline += c;
	cmdline += " ";
    }
    PROGRESS_POOL->start(*parent, this, cmdline, false, false);

    // Remove the SCSI interface of this device to avoid problems with double
    // usage of device nodes.
    createDriver();

    process_ = PROCESS_MONITOR->start(execName.c_str(), args, remoteFdArgNum);

    if (process_ != NULL) {
        status_ = DEV_BLANKING;
        action_ = A_BLANK;

        if (process_->commFd() >= 0) {
            Glib::signal_io().connect(
                bind(mem_fun(*this, &CdDevice::updateProgress), process_->commFd()),
                process_->commFd(), Glib::IOCondition::IO_IN | Glib::IOCondition::IO_HUP);
        }
        return 0;
    } else {
        return 1;
    }
}

void CdDevice::abortBlank()
{
    if (process_ != NULL && !process_->exited()) {
        PROCESS_MONITOR->stop(process_);
    }
}

CdDevice::DeviceType CdDevice::devtypeName2Id(const std::string& dt)
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
        return("Ready");
    case DEV_RECORDING:
        return("Recording");
    case DEV_READING:
        return("Reading");
    case DEV_SPINNING_UP:
        return("Spinning Up");
    case DEV_BUSY:
        return("Busy");
    case DEV_NO_DISC:
        return("No Disc");
    case DEV_BLANKING:
        return("Blanking");
    case DEV_FAULT:
        return("Not Available");
    case DEV_LOCKED:
        return("Already In Use");
    case DEV_TRAY_OPEN:
        return("Tray Open");
    case DEV_BAD_DISC:
        return("Incompatible Disc");
    case DEV_UNKNOWN:
    default:
        return("Unknown");
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

CdDevice *CdDevice::add(const std::string& dev, const std::string& vendor, const std::string& product)
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

static CdDevice *addImpl(const std::string& setting)
{
    char* s = (char*)alloca(setting.size() + 1);
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

CdDevice *CdDevice::add(const std::string& setting)
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
bool CdDevice::scan()
{
    log_message(0, "CdDevice::scan() :");
    bool changed = false;
    int i, len;
    ScsiIf::ScanData *sdata = ScsiIf::scan(&len);

    log_message(0, "  scan() returned %d devices", len);

    if (len != DEVICE_LIST.size()) {
	changed = true;
    } else {
	for (int i = 0; i < len; i++)
	    if (DEVICE_LIST[i]->dev() != sdata[i].dev) {
		changed = true;
		break;
	    }
    }

    if (changed) {    
        log_message(0, "  scan() status change");
	CdDevice::clear();

	if (sdata) {
	    for (i = 0; i < len; i++)
		CdDevice::add(sdata[i].dev.c_str(), sdata[i].vendor, sdata[i].product);
	    delete[] sdata;
	}
    }

    return changed;
}

void CdDevice::clear()
{
    for (auto dev : DEVICE_LIST)
	delete dev;
    DEVICE_LIST.clear();
}

int CdDevice::update()
{
    log_message(1, "[update] CdDevice");

     static int skipcnt = 0;
    int status = 0;

    blockProcessMonitorSignals();

    // if (skipcnt > 2) {
    //     if (CdDevice::scan())
    //         status |= UPD_CD_DEVICES;
    //     skipcnt = 0;
    // } else {
    //     skipcnt++;
    // }

    for (auto dev : DEVICE_LIST)
        if (dev->updateStatus())
	    status |= UPD_CD_DEVICE_STATUS;

    unblockProcessMonitorSignals();
    return status;
}

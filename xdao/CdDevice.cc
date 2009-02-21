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

#include <sys/time.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

#include <gtkmm.h>
#include <gnome.h>

#include "TocEdit.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "ProgressDialog.h"
#include "Settings.h"
#include "ConfigManager.h"

#include "config.h"
#include "remote.h"
#include "ScsiIf.h"
#include "CdrDriver.h"
#include "util.h"
#include "log.h"
#include "Toc.h"

#define DRIVER_IDS 13
#define DRIVER_ID_DEFAULT 2

CdDevice *CdDevice::DEVICE_LIST_ = NULL;

const char *CdDevice::DRIVER_NAMES_[DRIVER_IDS] = {
  "Undefined",
  "cdd2600",
  "generic-mmc",
  "generic-mmc-raw",
  "plextor",
  "plextor-scan",
  "ricoh-mp6200",
  "sony-cdu920",
  "sony-cdu948",
  "taiyo-yuden",
  "teac-cdr55",
  "toshiba",
  "yamaha-cdr10x"
};
  

CdDevice::CdDevice(const char* dev, const char *vendor, const char *product)
{
  dev_ = dev;
  vendor_ = vendor;
  product_ = product;
  
  driverId_ = 0;
  driverOptions_ = 0;

  deviceType_ = CD_R;

  manuallyConfigured_ = false;

  status_ = DEV_UNKNOWN;

  exitStatus_ = 0;

  progressStatusChanged_ = 0;
  progressStatus_ = 0;
  progressTotalTracks_ = 0;
  progressTrack_ = 0;
  progressTotal_ = 0;
  progressTrackRelative_ = 0;
  progressBufferFill_ = 0;
  progressWriterFill_ = 0;

  process_ = NULL;

  scsiIf_ = NULL;
  scsiIfInitFailed_ = 0;

  next_ = NULL;
  slaveDevice_ = NULL;

  autoSelectDriver();
}

CdDevice::~CdDevice()
{
  delete scsiIf_;
  scsiIf_ = NULL;
}

char *CdDevice::settingString() const
{
  char buf[100];
  std::string s;

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
    s+= "CD_ROM";
    break;
  }

  s += ",";

  s += driverName(driverId_);

  s += ",";

  sprintf(buf, "0x%lx", driverOptions_);
  s += buf;

  return strdupCC(s.c_str());
}

void CdDevice::driverId(int id)
{
  if (id >= 0 && id < DRIVER_IDS) 
    driverId_ = id;
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

  driverName = CdrDriver::selectDriver(1, vendor_.c_str(), product_.c_str(),
                                       &options);

  if (driverName) {
    driverId_ = driverName2Id(driverName);
    driverOptions_ = options;

  } else {
    bool r_cdr, w_cdr, r_cdrw, w_cdrw;

    ScsiIf* sif = new ScsiIf(dev_.c_str());

    if (sif && sif->init() == 0 &&
        sif->checkMmc(&r_cdr, &w_cdr, &r_cdrw, &w_cdrw)) {

      driverId_ = driverName2Id("generic-mmc");
      if (r_cdr)  deviceType_ = CD_ROM;
      if (w_cdr)  deviceType_ = CD_R;
      if (w_cdrw) deviceType_ = CD_RW;
    } else {
      driverId_ = DRIVER_ID_DEFAULT;
      driverOptions_ = 0;
    }
    if (sif) delete sif;
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
    }
  }

  if (status_ == DEV_READY || status_ == DEV_BUSY || status_ == DEV_NO_DISK ||
      status_ == DEV_UNKNOWN) {
    if (scsiIf_ == NULL)
      createScsiIf();

    if (scsiIf_ != NULL) {
      switch (scsiIf_->testUnitReady()) {
      case 0:
	newStatus = DEV_READY;
	break;
      case 1:
	newStatus = DEV_BUSY;
	break;
      case 2:
	newStatus = DEV_NO_DISK;
	break;
      case 3:
        // Most likely a timeout error.
	newStatus = DEV_BUSY;
	break;
      }
    }
    else {
      newStatus = DEV_FAULT;
    }
  }

  if (newStatus != status_) {
    status_ = newStatus;
    return 1;
  }
  
  return 0;
}

bool CdDevice::updateProgress(Glib::IOCondition cond, int fd)
{
  static char msgSync[4] = { 0xff, 0x00, 0xff, 0x00 };
  fd_set fds;
  int state = 0;
  char buf[10];
  struct timeval timeout = { 0, 0 };

  if (process_ == NULL)
    return false;

  if (!(cond & Gdk::INPUT_READ))
    return false;

  FD_ZERO(&fds);
  FD_SET(fd, &fds);

  while (select(fd + 1, &fds, NULL, NULL, &timeout) > 0 &&
	 FD_ISSET(fd, &fds)) {
    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    state = 0;

    while (state < 4) {
      if (read(fd, buf, 1) != 1) {
	//message(-2, "Reading of msg sync failed.");
	return false;
      }

      if (buf[0] == msgSync[state]) {
	state++;
      }
      else {
	state = 0;
	
	if (buf[0] == msgSync[state]) {
	  state++;
	}
      }
    }

    ProgressMsg msg;

    int msgsize = read(fd, (char *)&msg, sizeof(msg));
    if (msgsize >= PSGMSG_MINSIZE) {
      if (msg.status >= PGSMSG_MIN && msg.status <= PGSMSG_MAX &&
	  msg.track >= 0 &&
	  msg.totalProgress >= 0 && msg.totalProgress <= 1000 &&
	  msg.bufferFillRate >= 0 && msg.bufferFillRate <= 100) {
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
    }
    else {
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

  if (!scsiIf_)
    createScsiIf();

  if (scsiIf_) {
    CdrDriver* driver = CdrDriver::createDriver(driverName(driverId_),
                                                driverOptions_,
                                                scsiIf_);

    int ret = driver->loadUnload((load ? 0 : 1));
    success = (ret == 0);
    delete(driver);
  }

  return success;
}

// Starts a 'cdrdao' for recording given toc. Returns false if an
// error occured and the process was not successfully launched.
bool CdDevice::recordDao(Gtk::Window& parent, TocEdit *tocEdit, int simulate,
                        int multiSession, int speed, int eject, int reload,
                        int buffer, int overburn)
{
  char* tocFileName;
  const char *args[30];
  int n = 0;
  char devname[30];
  char drivername[50];
  char speedbuf[20];
  char *execName;
  const char *s;
  char bufferbuf[20];
  int remoteFdArgNum = 0;

  if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN)
      || process_ != NULL)
    return false;

  // Create temporary toc file. Get temporary directory from
  // ConfigManager, then append mkstemp template.
  Glib::ustring tempdir =
      configManager->client()->get_string("/apps/gcdmaster/temp_dir");
  int length = tempdir.length();
  tocFileName = (char*)alloca(length + 24);

  strcpy(tocFileName, tempdir.c_str());
  strcat(tocFileName, "/gcdm.toc.XXXXXX");

  int fd = mkstemp(tocFileName);
  if (!fd) {
    log_message(-2, _("Cannot create temporary toc-file: %s"), strerror(errno));
    return false;
  }

  // Write out temporary toc file containing all the converted wav
  // files (don't want to rely on cdrdao doing the mp3->wav
  // translation, besides it's already been done).
  if (!tocEdit->toc()->write(fd, true)) {
    close(fd);
    log_message(-2, _("Cannot write temporary toc-file."));
    return false;
  }

  close(fd);
  if ((s = gnome_config_get_string(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");


  args[n++] = execName;

  if (simulate)
    args[n++] = "simulate";
  else
    args[n++] = "write";

  args[n++] = "--remote";

  remoteFdArgNum = n;
  args[n++] = NULL;

  args[n++] = "-v0";

  if (multiSession)
    args[n++] = "--multi";

  if (speed > 0) {
    sprintf(speedbuf, "%d", speed);
    args[n++] = "--speed";
    args[n++] = speedbuf;
  }

  if (eject)
    args[n++] = "--eject";

  if (reload)
    args[n++] = "--reload";

  if (overburn)
    args[n++] = "--overburn";

  args[n++] = "--device";
  args[n++] = (char*)dev_.c_str();

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), driverOptions_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }

  if (buffer >= 10) {
    sprintf(bufferbuf, "%i", buffer);
    args[n++] = "--buffers";
    args[n++] = bufferbuf;
  }

  args[n++] = tocFileName;

  args[n++] = NULL;
  
  assert(n <= 20);
  
  PROGRESS_POOL->start(parent, this, tocEdit->filename());

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

  delete execName;
  if (process_ != NULL) {
    status_ = DEV_RECORDING;
    action_ = A_RECORD;

    if (process_->commFd() >= 0) {
        Glib::signal_io().connect(bind(mem_fun(*this, &CdDevice::updateProgress),
                                       process_->commFd()),
                                  process_->commFd(),
                                  Glib::IO_IN | Glib::IO_HUP);
    }

    return true;
  }
  else {
    unlink(tocFileName);
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

void CdDevice::progress(int *status, int *totalTracks, int *track,
			int *trackProgress, int *totalProgress,
			int *bufferFill, int *writerFill) const
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
int CdDevice::extractDao(Gtk::Window& parent, const char *tocFileName,
                         int correction, int readSubChanMode)
{
  const char *args[30];
  int n = 0;
  char devname[30];
  char drivername[50];
  char *execName;
  const char *s; 
  char correctionbuf[20];
  int remoteFdArgNum = 0;

  if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN)
      || process_ != NULL)
    return 1;

  if ((s = gnome_config_get_string(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");


  args[n++] = execName;

  args[n++] = "read-cd";

  args[n++] = "--remote";

  remoteFdArgNum = n;
  args[n++] = NULL;

  args[n++] = "-v0";

  args[n++] = "--read-raw";

  switch (readSubChanMode) {
  case 1:
    args[n++] = "--read-subchan";
    args[n++] = "rw";
    break;

  case 2:
    args[n++] = "--read-subchan";
    args[n++] = "rw_raw";
    break;
  }

  args[n++] = "--device";
  args[n++] = (char*)dev_.c_str();

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), driverOptions_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }

  sprintf(correctionbuf, "%d", correction);
  args[n++] = "--paranoia-mode";
  args[n++] = correctionbuf;

  args[n++] = "--datafile";
  args[n++] = g_strdup_printf("%s.bin", tocFileName);

  args[n++] = g_strdup_printf("%s.toc", tocFileName);

  args[n++] = NULL;
  
  assert(n <= 20);
  
  PROGRESS_POOL->start(parent, this, tocFileName, false, false);

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

  delete[] execName;

  if (process_ != NULL) {
    status_ = DEV_READING;
    action_ = A_READ;

    if (process_->commFd() >= 0) {
        Glib::signal_io().connect(bind(mem_fun(*this, &CdDevice::updateProgress),
                                       process_->commFd()),
                                  process_->commFd(),
                                  Glib::IO_IN | Glib::IO_PRI |
                                  Glib::IO_ERR | Glib::IO_HUP);
    }
    return 0;
  }
  else {
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
int CdDevice::duplicateDao(Gtk::Window& parent, int simulate, int multiSession,
                           int speed, int eject, int reload, int buffer,
                           int onthefly, int correction, int readSubChanMode, 
			   CdDevice *readdev)
{
  const char *args[30];
  int n = 0;
  char devname[30];
  char drivername[50];
  char r_drivername[50];
  char speedbuf[20];
  char correctionbuf[20];
  char *execName;
  const char *s;
  char bufferbuf[20];
  int remoteFdArgNum = 0;


  int rdstat = readdev->status();
  if ((rdstat != DEV_READY && rdstat != DEV_UNKNOWN && rdstat != DEV_FAULT) ||
      readdev->process() != NULL)
    return 1;

  if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN)
      || process_ != NULL)
    return 1;

  if ((s = gnome_config_get_string(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");

  args[n++] = execName;

  args[n++] = "copy";

  if (simulate)
    args[n++] = "--simulate";

  args[n++] = "--remote";

  remoteFdArgNum = n;
  args[n++] = NULL;

  args[n++] = "-v0";

  if (multiSession)
    args[n++] = "--multi";

  sprintf(correctionbuf, "%d", correction);
  args[n++] = "--paranoia-mode";
  args[n++] = correctionbuf;

  if (speed > 0) {
    sprintf(speedbuf, "%d", speed);
    args[n++] = "--speed";
    args[n++] = speedbuf;
  }

  if (eject)
    args[n++] = "--eject";

  if (reload)
    args[n++] = "--reload";

  if (onthefly)
    args[n++] = "--on-the-fly";

  switch (readSubChanMode) {
  case 1:
    args[n++] = "--read-subchan";
    args[n++] = "rw";
    break;

  case 2:
    args[n++] = "--read-subchan";
    args[n++] = "rw_raw";
    break;
  }

  args[n++] = "--device";
  args[n++] = (char*)dev_.c_str();

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), driverOptions_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }


  if (readdev != this) { // reader and write the same, skip source device
		  
    args[n++] = "--source-device";
    args[n++] = (char*)readdev->dev();

    if (readdev->driverId() > 0) {
      sprintf(r_drivername, "%s:0x%lx", driverName(readdev->driverId()),
      			 readdev->driverOptions());
      args[n++] = "--source-driver";
      args[n++] = r_drivername;
    }
  }
  if (buffer >= 10) {
    sprintf(bufferbuf, "%i", buffer);
    args[n++] = "--buffers";
    args[n++] = bufferbuf;
  }


  args[n++] = NULL;
  
  assert(n <= 25);
  
  PROGRESS_POOL->start(parent, this, _("CD to CD copy"));

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

  delete[] execName;

  if (process_ != NULL) {
    slaveDevice_ = readdev;
    slaveDevice_->status(DEV_READING);
    status_ = DEV_RECORDING;

    action_ = A_DUPLICATE;

    if (process_->commFd() >= 0) {
        Glib::signal_io().connect(bind(mem_fun(*this, &CdDevice::updateProgress),
                                       process_->commFd()),
                                  process_->commFd(),
                                  Glib::IO_IN | Glib::IO_HUP);
    }

    return 0;
  }
  else {
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
int CdDevice::blank(Gtk::Window* parent, int fast, int speed, int eject,
                    int reload)
{
  const char *args[20];
  int n = 0;
  char devname[30];
  char drivername[50];
  char speedbuf[20];
  char *execName;
  const char *s;
  int remoteFdArgNum = 0;

  if ((status_ != DEV_READY && status_ != DEV_FAULT && status_ != DEV_UNKNOWN)
      || process_ != NULL)
    return 1;

  if ((s = gnome_config_get_string(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");

  args[n++] = execName;

  args[n++] = "blank";

  args[n++] = "--remote";

  remoteFdArgNum = n;
  args[n++] = NULL;

  args[n++] = "-v0";

  args[n++] = "--blank-mode";

  if (fast)
    args[n++] = "minimal";
  else
    args[n++] = "full";

  if (speed > 0) {
    sprintf(speedbuf, "%d", speed);
    args[n++] = "--speed";
    args[n++] = speedbuf;
  }

  if (eject)
    args[n++] = "--eject";

  if (reload)
    args[n++] = "--reload";

  args[n++] = "--device";
  args[n++] = (char*)dev_.c_str();

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), driverOptions_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }

  args[n++] = NULL;
  
  assert(n <= 20);
  
  if (parent)
    PROGRESS_POOL->start(*parent, this, _("Blanking CDRW"), false, false);
  else
    PROGRESS_POOL->start(this, _("Blanking CDRW"), false, false);

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args, remoteFdArgNum);

  delete[] execName;

  if (process_ != NULL) {
    status_ = DEV_BLANKING;
    action_ = A_BLANK;

    if (process_->commFd() >= 0) {
        Glib::signal_io().connect(bind(mem_fun(*this, &CdDevice::updateProgress),
                                       process_->commFd()),
                                  process_->commFd(),
                                  Glib::IO_IN | Glib::IO_HUP);
    }
    return 0;
  }
  else {
    return 1;
  }
}

void CdDevice::abortBlank()
{
  if (process_ != NULL && !process_->exited()) {
    PROCESS_MONITOR->stop(process_);
  }
}

void CdDevice::createScsiIf()
{
  char buf[100];

  if (scsiIfInitFailed_)
    return;

  delete scsiIf_;
  scsiIf_ = new ScsiIf(dev_.c_str());

  if (scsiIf_->init() != 0) {
    delete scsiIf_;
    scsiIf_ = NULL;
    scsiIfInitFailed_ = 1;
  }
}

int CdDevice::driverName2Id(const char *driverName)
{
  int i;

  for (i = 1; i < DRIVER_IDS; i++) {
    if (strcmp(DRIVER_NAMES_[i], driverName) == 0)
      return i;
  }

  return 0;
}

int CdDevice::maxDriverId()
{
  return DRIVER_IDS - 1;
}

const char *CdDevice::driverName(int id)
{
  if (id >= 0 && id < DRIVER_IDS) {
    return DRIVER_NAMES_[id];
  }
  else {
    return "Undefined";
  }
}

const char *CdDevice::status2string(Status s)
{
  const char *ret = NULL;

  switch (s) {
  case DEV_READY:
    ret = "Ready";
    break;
  case DEV_RECORDING:
    ret = "Recording";
    break;
  case DEV_READING:
    ret = "Reading";
    break;
  case DEV_WAITING:
    ret = "Waiting";
    break;
  case DEV_BLANKING:
    ret = "Blanking";
    break;
  case DEV_BUSY:
    ret = "Busy";
    break;
  case DEV_NO_DISK:
    ret = "No disk";
    break;
  case DEV_FAULT:
    ret = "Not available";
    break;
  case DEV_UNKNOWN:
    ret = "Unknown";
    break;
  }

  return ret;
}

const char *CdDevice::deviceType2string(DeviceType t)
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


/* reads configured devices from gnome settings
 */
void CdDevice::importSettings()
{
  int i, n;
  char *s;
  char buf[20];
  CdDevice *dev;

  n = gnome_config_get_int(SET_DEVICES_NUM);

  if (n > 0) {
    gnome_config_push_prefix(SET_SECTION_DEVICES);
    
    for (i = 0; i < n; i++) {
      sprintf(buf, "%d", i);
      s = gnome_config_get_string(buf);

      if (s != NULL) {
	if ((dev = CdDevice::add(s)) != NULL)
	  dev->manuallyConfigured(true);
      }
    }

    gnome_config_pop_prefix();
  }
}


/* saves manually configured devices as gnome settings
 */
void CdDevice::exportSettings()
{
  static const char* pathBase = "/apps/gcdmaster/devices";
  char* key;
  char* s;
  CdDevice* drun;
  int n;

  key = (char*)alloca(strlen(pathBase) + 12);

  for (drun = first(), n = 0; drun != NULL; drun = next(drun)) {

    if (drun->manuallyConfigured()) {
      sprintf(key, "%s%d", pathBase, n);
      s = drun->settingString();

      try {
	Gnome::Conf::Schema sch;
	sch.set_type(Gnome::Conf::VALUE_STRING);
	configManager->client()->set(key, sch);
	configManager->client()->set(key, s);
      } catch (const Glib::Error& e) {
	std::cerr << e.what() << std::endl;
      }
      // CdDevice::settingString allocates the string. Must delete it
      // here.
      delete[] s;
      n++;
    }
  }
}

CdDevice *CdDevice::add(const char* dev, const char *vendor,
                        const char *product)
{
  CdDevice *run, *pred, *ent;

  for (pred = NULL, run = DEVICE_LIST_; run != NULL;
       pred = run, run = run->next_) {
    if (strcmp(run->dev(), dev) == 0)
      return run;
  }

  ent = new CdDevice(dev, vendor, product);

  if (pred != NULL) {
    ent->next_ = pred->next_;
    pred->next_ = ent;
  }
  else {
    ent->next_ = DEVICE_LIST_;
    DEVICE_LIST_ = ent;
  }

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
    }
    else {
      *p++ = 0;
      
      // skip over ,
      while (*p != 0 && *p != ',')
	p++;

      if (*p == ',')
	p++;
    }
  }
  else {
    val = p;

    while (*p != 0 && *p != ',')
      p++;
   
    if (*p == ',')
      *p++ = 0;
  }

  return val;
}

static CdDevice *addImpl(char *s)
{
  char *p;
  int driverId;
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
  driverId = CdDevice::driverName2Id(val);

  if ((val = nextToken(p)) == NULL)
    return NULL;
  options = strtoul(val, NULL, 0);

  cddev = CdDevice::add(dev.c_str(), vendor.c_str(), model.c_str());

  cddev->driverId(driverId);
  cddev->deviceType(type);
  cddev->driverOptions(options);
  
  return cddev;
}

CdDevice *CdDevice::add(const char *setting)
{
  char *s = strdupCC(setting);

  CdDevice *dev = addImpl(s);

  delete[] s;

  return dev;
}



CdDevice *CdDevice::find(const char* dev)
{
  CdDevice *run;

  for (run = DEVICE_LIST_; run != NULL; run = run->next_) {
    if (strcmp(run->dev(), dev) == 0)
      return run;
  }

  return NULL;
}
  
void CdDevice::scan()
{
  int i, len;
  ScsiIf::ScanData *sdata = ScsiIf::scan(&len);

  if (sdata) {
    for (i = 0; i < len; i++)
      CdDevice::add(sdata[i].dev.c_str(), sdata[i].vendor, sdata[i].product);
    delete[] sdata;
  }

#ifdef SCSI_ATAPI
  sdata = ScsiIf::scan(&len, "ATA");
  if (sdata) {
    for (i = 0; i < len; i++)
      CdDevice::add(sdata[i].dev.c_str(), sdata[i].vendor, sdata[i].product);
    delete[] sdata;
  } else {
    // Only scan for ATAPI devices if we got nothing on the ATA
    // interface, otherwise every device would show up twice on the
    // list.
    sdata = ScsiIf::scan(&len, "ATAPI");
    if (sdata) {
      for (i = 0; i < len; i++)
        CdDevice::add(sdata[i].dev.c_str(), sdata[i].vendor, sdata[i].product);
      delete[] sdata;
    }
  }
#endif
}

void CdDevice::remove(const char* dev)
{
  CdDevice *run, *pred;

  for (pred = NULL, run = DEVICE_LIST_; run != NULL;
       pred = run, run = run->next_) {
    if (strcmp(run->dev(), dev) == 0) {
      if (run->status() == DEV_RECORDING || run->status() == DEV_BLANKING ||
	  run->status() == DEV_READING || run->status() == DEV_WAITING)
	return;
	
      if (pred != NULL)
	pred->next_ = run->next_;
      else
	DEVICE_LIST_ = run->next_;

      delete run;
      return;
    }
  }
}

void CdDevice::clear()
{
  CdDevice *next;

  while (DEVICE_LIST_ != NULL) {
    next = DEVICE_LIST_->next_;
    delete DEVICE_LIST_;
    DEVICE_LIST_ = next;
  }
}

CdDevice *CdDevice::first()
{
  return DEVICE_LIST_;
}

CdDevice *CdDevice::next(const CdDevice *run)
{
  if (run != NULL)
    return run->next_;
  else
    return NULL;
}

int CdDevice::count()
{
  CdDevice *run;
  int cnt = 0;

  for (run = DEVICE_LIST_; run != NULL; run = run->next_)
    cnt++;

  return cnt;
}

int CdDevice::updateDeviceStatus()
{
  int newStatus = 0;

  CdDevice *run;

  blockProcessMonitorSignals();

  for (run = DEVICE_LIST_; run != NULL; run = run->next_) {
    if (run->updateStatus())
      newStatus = 1;
  }

  unblockProcessMonitorSignals();

  return newStatus;
}

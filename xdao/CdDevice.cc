/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000 Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: CdDevice.cc,v $
 * Revision 1.2  2000/04/24 12:49:06  andreasm
 * Changed handling or message from remote processes to use the
 * Gtk::Main::input mechanism.
 *
 * Revision 1.3  1999/12/15 20:34:18  mueller
 * CD image extraction added by Manuel Clos.
 *
 * Revision 1.2  1999/11/07 09:18:45  mueller
 * Release 1.1.3
 *
 * Revision 1.1  1999/09/03 16:05:14  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: CdDevice.cc,v 1.2 2000/04/24 12:49:06 andreasm Exp $";

#include <sys/time.h>
#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "TocEdit.h"
#include "CdDevice.h"
#include "ProcessMonitor.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "ExtractProgressDialog.h"
#include "RecordProgressDialog.h"
#include "Settings.h"

#include "remote.h"
#include "ScsiIf.h"
#include "CdrDriver.h"

#include "util.h"
#include "Toc.h"

#define DRIVER_IDS 12

CdDevice *CdDevice::DEVICE_LIST_ = NULL;

char *CdDevice::DRIVER_NAMES_[DRIVER_IDS] = {
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
  "yamaha-cdr10x"
};
  

CdDevice::CdDevice(int bus, int id, int lun, const char *vendor,
		   const char *product)
{
  bus_ = bus;
  id_ = id;
  lun_ = lun;

  vendor_ = strdupCC(vendor);
  product_ = strdupCC(product);
  
  driverId_ = 0; // undefined
  options_ = 0;
  specialDevice_ = NULL;

  deviceType_ = CD_R;

  manuallyConfigured_ = 0;

  status_ = DEV_UNKNOWN;

  exitStatus_ = 0;

  progressStatusChanged_ = 0;
  progressStatus_ = 0;
  progressTrack_ = 0;
  progressTotal_ = 0;
  progressBufferFill_ = 0;

  process_ = NULL;

  scsiIf_ = NULL;
  scsiIfInitFailed_ = 0;

  next_ = NULL;

  autoSelectDriver();
}

CdDevice::~CdDevice()
{
  delete[] vendor_;
  vendor_ = NULL;

  delete[] product_;
  product_ = NULL;

  delete[] specialDevice_;
  specialDevice_ = NULL;

  delete scsiIf_;
  scsiIf_ = NULL;
}

char *CdDevice::settingString() const
{
  char buf[100];
  string s;

  sprintf(buf, "%d,%d,%d,'", bus_, id_, lun_);

  s += buf;
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

  sprintf(buf, "0x%lx,", options_);
  s += buf;

  s += "'";
  if (specialDevice_ != NULL)
    s += specialDevice_;
  s += "'";

  return strdupCC(s.c_str());
}

int CdDevice::bus() const
{
  return bus_;
}

int CdDevice::id() const
{
  return id_;
}

int CdDevice::lun() const
{
  return lun_;
}

const char *CdDevice::vendor() const
{
  return vendor_;
}

const char *CdDevice::product() const
{
  return product_;
}

int CdDevice::driverId() const
{
  return driverId_;
}

void CdDevice::driverId(int id)
{
  if (id >= 0 && id < DRIVER_IDS) 
    driverId_ = id;
}

const char *CdDevice::specialDevice() const
{
  return specialDevice_;
}

void CdDevice::specialDevice(const char *s)
{
  delete[] specialDevice_;

  if (s == NULL || *s == 0)
    specialDevice_ = NULL;
  else
    specialDevice_ = strdupCC(s);

  // remove existing SCSI interface class, it will be recreated with
  // respect to special device setting automatically later
  delete scsiIf_;
  scsiIf_ = NULL;
  scsiIfInitFailed_ = 0;

  status_ = DEV_UNKNOWN;
}

int CdDevice::manuallyConfigured() const
{
  return manuallyConfigured_;
}

void CdDevice::manuallyConfigured(int f)
{
  manuallyConfigured_ = (f != 0) ? 1 : 0;
}

CdDevice::Status CdDevice::status() const
{
  return status_;
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

  driverName = CdrDriver::selectDriver(1, vendor_, product_, &options);

  if (driverName == NULL) {
    // select read driver 
    driverName = CdrDriver::selectDriver(0, vendor_, product_, &options);

    if (driverName != NULL)
      deviceType_ = CD_ROM;
  }

  if (driverName != NULL) {
    driverId_ = driverName2Id(driverName);
    options_ = options;

    return 1;
  }
  else {
    driverId_ = 0;
    options_ = 0;

    return 0;
  }
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
	newStatus = DEV_FAULT;
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

void CdDevice::updateProgress(int fd, GdkInputCondition cond)
{
  static char msgSync[4] = { 0xff, 0x00, 0xff, 0x00 };
  fd_set fds;
  int state = 0;
  char buf[10];
  struct timeval timeout = { 0, 0 };

  //message(0, "Rcvd msg");

  if (process_ == NULL)
    return;

  if (!(cond & GDK_INPUT_READ))
    return;

  if (fd < 0 || fd != process_->commFd()) {
    message(-3,
	    "CdDevice::updateProgress called with illegal fild descriptor.");
    return;
  }

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
	return;
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

    if (status_ == DEV_RECORDING) {
      DaoWritingProgress msg;

      if (read(fd, (char *)&msg, sizeof(msg)) == sizeof(msg)) {
	/*
	  message(0, "progress: %d %d %d %d", msg.status, msg.track,
	  msg.totalProgress, msg.bufferFillRate);
	  */
	if (msg.status >= 1 && msg.status <= 3 &&
	    msg.track >= 0 && msg.track <= 0xaa &&
	    msg.totalProgress >= 0 && msg.totalProgress <= 1000 &&
	    msg.bufferFillRate >= 0 && msg.bufferFillRate <= 100) {
	  progressStatus_ = msg.status;
	  progressTrack_ = msg.track;
	  progressTotal_ = msg.totalProgress;
	  progressBufferFill_ = msg.bufferFillRate;

	  progressStatusChanged_ = 1;
	}
      }
      else {
	message(-1, "Reading of record progress msg failed.");
      }
    }
    else if (status_ == DEV_READING) {
      ReadCdProgress msg;

      if (read(fd, (char *)&msg, sizeof(msg)) == sizeof(msg)) {
	/*
	  message(0, "progress: %d %d %d %d", msg.status, msg.track,
	  msg.totalProgress, msg.bufferFillRate);
	  */
	if (msg.status >= 1 && msg.status <= 2 &&
	    msg.track >= 0 && msg.track <= 99 &&
	    msg.trackProgress >= 0 && msg.trackProgress <= 1000) {
	  progressStatus_ = msg.status;
	  progressTrack_ = msg.track;
	  progressTrackRelative_ = msg.trackProgress;

	  progressStatusChanged_ = 1;
	}
      }
      else {
	message(-1, "Reading of record progress msg failed.");
      }
    }
  }

  if (progressStatusChanged_)
    guiUpdate(UPD_PROGRESS_STATUS);

  return;
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
  return options_;
}

void CdDevice::driverOptions(unsigned long o)
{
  options_ = o;
}

// Starts a 'cdrdao' for recording given toc.
// Return: 0: OK, process succesfully launched
//         1: error occured
int CdDevice::recordDao(TocEdit *tocEdit, int simulate, int multiSession,
			int speed, int eject, int reload)
{
  char *tocFileName;
  char *args[20];
  int n = 0;
  char devname[30];
  char drivername[50];
  char speedbuf[20];
  char *execName;
  const char *s;

  if (status_ != DEV_READY || process_ != NULL)
    return 1;

  if ((tocFileName = tempnam(NULL, "toc")) == NULL) {
    message(-2, "Cannot create temporary toc-file: %s", strerror(errno));
    return 1;
  }

  if (tocEdit->toc()->write(tocFileName) != 0) {
    message(-2, "Cannot write temporary toc-file.");
    return 1;
  }

  if ((s = SETTINGS->getString(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");


  args[n++] = execName;

  if (simulate)
    args[n++] = "simulate";
  else
    args[n++] = "write";

  args[n++] = "--remote";

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

  args[n++] = "--device";

  if (specialDevice_ != NULL && *specialDevice_ != 0) {
    args[n++] = specialDevice_;
  }
  else {
    sprintf(devname, "%d,%d,%d", bus_, id_, lun_);
    args[n++] = devname;
  }

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), options_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }

  args[n++] = tocFileName;

  args[n++] = NULL;
  
  assert(n <= 20);
  
  int i;

  message(0, "Starting: ");
  for (i = 0; i < n - 1; i++)
    message(0, "%s ", args[i]);
  message(0, "");

  RECORD_PROGRESS_POOL->start(this, tocEdit);

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args);

  delete[] execName;

  if (process_ != NULL) {
    status_ = DEV_RECORDING;
    free(tocFileName);

    if (process_->commFd() >= 0) {
      Gtk::Main::instance()->input.connect(slot(this,
						&CdDevice::updateProgress),
					   process_->commFd(),
					   (GdkInputCondition)(GDK_INPUT_READ|GDK_INPUT_EXCEPTION));
    }

    return 0;
  }
  else {
    unlink(tocFileName);
    free(tocFileName);
    return 1;
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

void CdDevice::recordProgress(int *status, int *track, int *totalProgress,
		      int *bufferFill) const
{
  *status = progressStatus_;
  *track = progressTrack_;
  *totalProgress = progressTotal_;
  *bufferFill = progressBufferFill_;
}

// Starts a 'cdrdao' for reading whole cd.
// Return: 0: OK, process succesfully launched
//         1: error occured
int CdDevice::extractDao(char *tocFileName)
{
  char *args[20];
  int n = 0;
  char devname[30];
  char drivername[50];
  char speedbuf[20];
  char *execName;
  const char *s; 

  if (status_ != DEV_READY || process_ != NULL)
    return 1;

  if ((s = SETTINGS->getString(SET_CDRDAO_PATH)) != NULL)
    execName = strdupCC(s);
  else
    execName = strdupCC("cdrdao");


  args[n++] = execName;

  args[n++] = "read-cd";

  args[n++] = "--remote";

  args[n++] = "-v0";

  args[n++] = "--read-raw";

  args[n++] = "--device";

  if (specialDevice_ != NULL && *specialDevice_ != 0) {
    args[n++] = specialDevice_;
  }
  else {
    sprintf(devname, "%d,%d,%d", bus_, id_, lun_);
    args[n++] = devname;
  }

  if (driverId_ > 0) {
    sprintf(drivername, "%s:0x%lx", driverName(driverId_), options_);
    args[n++] = "--driver";
    args[n++] = drivername;
  }

  args[n++] = "--datafile";
  args[n++] = g_strdup_printf("%s.bin", tocFileName);

  args[n++] = g_strdup_printf("%s.toc", tocFileName);

  args[n++] = NULL;
  
  assert(n <= 20);
  
  int i;

  message(0, "Starting: ");
  for (i = 0; i < n - 1; i++)
    message(0, "%s ", args[i]);
  message(0, "");

  EXTRACT_PROGRESS_POOL->start(this, tocFileName);

  // Remove the SCSI interface of this device to avoid problems with double
  // usage of device nodes.
  delete scsiIf_;
  scsiIf_ = NULL;

  process_ = PROCESS_MONITOR->start(execName, args);

  delete[] execName;

  if (process_ != NULL) {
    status_ = DEV_READING;

    if (process_->commFd() >= 0) {
      Gtk::Main::instance()->input.connect(slot(this,
					       &CdDevice::updateProgress),
					  process_->commFd(),
					  (GdkInputCondition)(GDK_INPUT_READ|GDK_INPUT_EXCEPTION));
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

void CdDevice::readProgress(int *status, int *track, int *trackProgress) const
{
  *status = progressStatus_;
  *track = progressTrack_;
  *trackProgress = progressTrackRelative_;
}

void CdDevice::createScsiIf()
{
  char buf[100];

  if (scsiIfInitFailed_)
    return;

  delete scsiIf_;

  if (specialDevice_ != NULL && *specialDevice_ != 0) {
    scsiIf_ = new ScsiIf(specialDevice_);
  }
  else {
    sprintf(buf, "%d,%d,%d", bus_, id_, lun_);
    scsiIf_ = new ScsiIf(buf);
  }

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
  char *ret = NULL;

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
  char *ret = NULL;

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

CdDevice *CdDevice::add(int bus, int id, int lun, const char *vendor,
			const char *product)
{
  CdDevice *run, *pred, *ent;

  for (pred = NULL, run = DEVICE_LIST_; run != NULL;
       pred = run, run = run->next_) {
    if (bus < run->bus() ||
	(bus == run->bus() && id < run->id()) ||
	(bus == run->bus() && id == run->id() && lun < run->lun())) {
      break;
    }
    else if (bus == run->bus() && id == run->id() && lun == run->lun()) {
      return run;
    }
  }

  ent = new CdDevice(bus, id, lun, vendor, product);

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
  int bus, id, lun;
  int driverId;
  string vendor;
  string model;
  string device;
  unsigned long options;
  char *val;
  CdDevice::DeviceType type;
  CdDevice *dev;

  p = s;

  if ((val = nextToken(p)) == NULL)
    return NULL;
  bus = atoi(val);

  if ((val = nextToken(p)) == NULL)
    return NULL;
  id = atoi(val);

  if ((val = nextToken(p)) == NULL)
    return NULL;
  lun = atoi(val);

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

  if ((val = nextToken(p)) == NULL)
    return NULL;
  device = val;

  dev = CdDevice::add(bus, id, lun, vendor.c_str(), model.c_str());

  dev->driverId(driverId);
  dev->deviceType(type);
  dev->driverOptions(options);
  dev->specialDevice(device.c_str());
  
  return dev;
}

CdDevice *CdDevice::add(const char *setting)
{
  char *s = strdupCC(setting);

  CdDevice *dev = addImpl(s);

  delete[] s;

  return dev;
}



CdDevice *CdDevice::find(int bus, int id, int lun)
{
  CdDevice *run;

  for (run = DEVICE_LIST_; run != NULL; run = run->next_) {
    if (bus == run->bus() && id == run->id() && lun == run->lun()) {
      return run;
    }
  }

  return NULL;
}
  
void CdDevice::scan()
{
  int i, len;
  ScsiIf::ScanData *sdata = ScsiIf::scan(&len);

  if (sdata == NULL)
    return;

  for (i = 0; i < len; i++)
    CdDevice::add(sdata[i].bus, sdata[i].id, sdata[i].lun, sdata[i].vendor,
		  sdata[i].product);

  delete[] sdata;
}

void CdDevice::remove(int bus, int id, int lun)
{
  CdDevice *run, *pred;

  for (pred = NULL, run = DEVICE_LIST_; run != NULL;
       pred = run, run = run->next_) {
    if (bus == run->bus() && id == run->id() && lun == run->lun()) {
      if (run->status() == DEV_RECORDING || run->status() == DEV_BLANKING)
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

#if 0
/* not used anymore since Gtk::Main::input signal will call
 * CdDevice::updateProgress directly.
 */
int CdDevice::updateDeviceProgress()
{
  int progress = 0;
  int fd;
  fd_set fds;
  int maxFd = -1;
  CdDevice *run;
  struct timeval timeout = { 0, 0 };

  blockProcessMonitorSignals();

  FD_ZERO(&fds);

  // collect set of file descriptors for 'select'
  for (run = DEVICE_LIST_; run != NULL; run = run->next_) {
    if (run->process_ != NULL && (fd = run->process_->commFd()) >= 0) {
      FD_SET(fd, &fds);
      if (fd > maxFd)
	maxFd = fd;
    }
  }

  if (maxFd < 0) {
    unblockProcessMonitorSignals();
    return 0;
  }

  if (select(maxFd + 1, &fds, NULL, NULL, &timeout) > 0) {
    for (run = DEVICE_LIST_; run != NULL; run = run->next_) {
      if (run->process_ != NULL && (fd = run->process_->commFd()) >= 0) {
	if (FD_ISSET(fd, &fds)) {
	  if (run->updateProgress())
	    progress = 1;
	}
      }
    }
  }

  unblockProcessMonitorSignals();

  return progress;
}
#endif

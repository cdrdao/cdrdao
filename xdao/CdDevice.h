/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: CdDevice.h,v $
 * Revision 1.1  2000/02/05 01:38:46  llanero
 * Initial revision
 *
 * Revision 1.1  1999/09/03 16:05:14  mueller
 * Initial revision
 *
 */

#ifndef __CD_DEVICE_H__
#define __CD_DEVICE_H__

/*
#include <gtk--.h>
#include <gtk/gtk.h>
*/

class TocEdit;
class Process;
class ScsiIf;

class CdDevice /*: public Gtk_Signal_Base*/ {
public:
  enum Status { DEV_READY, DEV_RECORDING, DEV_READING, DEV_BUSY,
		DEV_NO_DISK, DEV_BLANKING, DEV_FAULT, DEV_UNKNOWN };
  enum DeviceType { CD_R, CD_RW, CD_ROM };

  CdDevice(int bus, int id, int lun, const char *vendor,
	   const char *product);
  ~CdDevice();

  char *settingString() const;

  int bus() const; 
  int id() const;
  int lun() const;

  const char *vendor() const;
  const char *product() const;

  Status status() const;

  int exitStatus() const;

  void status(Status);
  int updateStatus();

  int updateProgress();

  int autoSelectDriver();

  int driverId() const;
  void driverId(int);

  DeviceType deviceType() const;
  void deviceType(DeviceType);

  unsigned long driverOptions() const;
  void driverOptions(unsigned long);

  const char *specialDevice() const;
  void specialDevice(const char *);

  int manuallyConfigured() const;
  void manuallyConfigured(int);

  int recordDao(TocEdit *, int simulate, int multiSession, int speed,
		int eject, int reload);
  void abortDaoRecording();

  int extractDao(char *tocFileName);
  void abortDaoReading();
    
  int progressStatusChanged();
  void recordProgress(int *status, int *track, int *totalProgress,
		      int *bufferFill) const;
  
  void readProgress(int *status, int *track, int *totalProgress,
		      int *bufferFill) const;

  static int maxDriverId();
  static const char *driverName(int id);
  static int driverName2Id(const char *);

  static const char *status2string(Status);
  static const char *deviceType2string(DeviceType);

  static CdDevice *add(int bus, int id, int lun, const char *vendor,
		       const char *product);

  static CdDevice *add(const char *setting);

  static CdDevice *find(int bus, int id, int lun);
  
  static void scan();

  static void remove(int bus, int id, int lun);

  static void clear();

  static CdDevice *first();
  static CdDevice *next(const CdDevice *);

  static int updateDeviceStatus();
  static int updateDeviceProgress();

  static int count();

private:
  int bus_; // SCSI bus
  int id_;  // SCSI id
  int lun_; // SCSI logical unit

  char *vendor_;
  char *product_;

  DeviceType deviceType_;

  int driverId_;
  unsigned long options_;
  char *specialDevice_;

  int manuallyConfigured_;

  ScsiIf *scsiIf_;
  int scsiIfInitFailed_;
  Status status_;

  int exitStatus_;

  int progressStatusChanged_;
  int progressStatus_;
  int progressTrack_;
  int progressTotal_;
  int progressBufferFill_;

  Process *process_;

  CdDevice *next_;

  void createScsiIf();

  static char *DRIVER_NAMES_[];
  static CdDevice *DEVICE_LIST_;
};

#endif

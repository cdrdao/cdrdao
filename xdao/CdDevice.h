/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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
 * Revision 1.11  2000/11/05 19:20:59  andreasm
 * Unified progress messages sent from cdrdao to gcdmaster.
 *
 * Revision 1.10  2000/10/08 16:39:41  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.9  2000/08/01 01:27:50  llanero
 * CD to CD copy works now.
 *
 * Revision 1.8  2000/07/31 01:55:49  llanero
 * got rid of old Extract dialog and Record dialog.
 * both are using RecordProgressDialog now.
 *
 * Revision 1.7  2000/07/30 14:25:53  llanero
 * fixed bug with --device not receiving the right device
 *
 * Revision 1.6  2000/07/30 02:41:03  llanero
 * started CD to CD copy. Still not functional.
 *
 * Revision 1.5  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
 *
 * Revision 1.4  2000/04/29 14:46:38  llanero
 * added the "buffers" option to the Record Dialog.
 *
 * Revision 1.3  2000/04/28 19:08:10  llanero
 * modified glade files.
 * modified toolbar a little.
 * extract dialog has more option, and now you can specify the paranoia mode.
 *
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

#ifndef __CD_DEVICE_H__
#define __CD_DEVICE_H__

#include <sigc++/object.h>
#include <gdk/gdk.h>

class TocEdit;
class Process;
class ScsiIf;

class CdDevice : public SigC::Object {
public:
  enum Status { DEV_READY, DEV_RECORDING, DEV_READING, DEV_WAITING, DEV_BUSY,
		DEV_NO_DISK, DEV_BLANKING, DEV_FAULT, DEV_UNKNOWN };
  enum DeviceType { CD_R, CD_RW, CD_ROM };

  enum Action { A_RECORD, A_READ, A_DUPLICATE, A_NONE };

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
  Process *process() const;

  int exitStatus() const;

  void status(Status);
  int updateStatus();

  Action action() const;

  void updateProgress(int fd, GdkInputCondition);

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
		int eject, int reload, int buffer);
  void abortDaoRecording();

  int extractDao(char *tocFileName, int correction);
  void abortDaoReading();

  int duplicateDao(int simulate, int multiSession, int speed,
		int eject, int reload, int buffer, int onthefly, int correction, CdDevice *readdev);
  void abortDaoDuplication();
    
  int progressStatusChanged();
  void progress(int *status, int *totalTracks, int *track,
		      int *trackProgress, int *totalProgress,
		      int *bufferFill) const;
  
  static int maxDriverId();
  static const char *driverName(int id);
  static int driverName2Id(const char *);

  static const char *status2string(Status);
  static const char *deviceType2string(DeviceType);

  static void importSettings();
  static void exportSettings();

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

  /* not used anymore since Gtk::Main::input signal will call
   * CdDevice::updateProgress directly.

     static int updateDeviceProgress();
  */

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

  Action action_;

  int exitStatus_;

  int progressStatusChanged_;
  int progressStatus_;
  int progressTotalTracks_;
  int progressTrack_;
  int progressTotal_;
  int progressTrackRelative_;
  int progressBufferFill_;

  Process *process_;

  CdDevice *next_;
  CdDevice *slaveDevice_; // slave device (used when copying etc.)

  void createScsiIf();

  static char *DRIVER_NAMES_[];
  static CdDevice *DEVICE_LIST_;
};

#endif

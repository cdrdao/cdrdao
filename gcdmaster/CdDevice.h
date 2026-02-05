/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __CD_DEVICE_H__
#define __CD_DEVICE_H__

#include <gtkmm.h>
#include <string>
#include <vector>

class TocEdit;
class Process;
class ScsiIf;
class CdrDriver;

class CdDevice : public sigc::trackable
{
  public:
    enum Status {
        DEV_READY,
        DEV_RECORDING,
        DEV_READING,
        DEV_WAITING,
        DEV_BUSY,
        DEV_NO_DISK,
        DEV_BLANKING,
        DEV_FAULT,
        DEV_LOCKED,
        DEV_UNKNOWN
    };
    enum DeviceType {
        CD_R,
        CD_RW,
        CD_ROM
    };

    enum Action {
        A_RECORD,
        A_READ,
        A_DUPLICATE,
        A_BLANK,
        A_NONE
    };

    CdDevice(const std::string& dev, const std::string& vendor, const std::string& product);
    ~CdDevice();

    Glib::ustring settingString() const;

    const std::string& dev() const { return dev_; }
    const std::string& vendor() const { return vendor_; }
    const std::string& product() const { return product_; }
    const std::string& description() const { return description_; }

    Status status() const { return status_; }
    Process *process() const { return process_; }

    int exitStatus() const;

    void status(Status);
    int updateStatus();

    Action action() const { return action_; }

    bool updateProgress(Glib::IOCondition, int fd);

    int autoSelectDriver();

    const std::string& driver() const { return driver_; }
    void driver(const std::string& d) { driver_ = d; }

    DeviceType deviceType() const;
    std::string deviceTypeName() const { return CdDevice::deviceType2string(deviceType()); }
    void deviceType(DeviceType);
    void deviceType(const std::string& t) { deviceType(CdDevice::devtypeName2Id(t)); }

    unsigned long driverOptions() const;
    void driverOptions(unsigned long);

    bool manuallyConfigured() const { return manuallyConfigured_; }
    void manuallyConfigured(bool b) { manuallyConfigured_ = b; }

    bool ejectCd(bool load = false);
    bool loadCd() { return ejectCd(true); }

    bool recordDao(Gtk::Window &parent, TocEdit *, int simulate, int multiSession, int speed,
                   int eject, int reload, int buffer, int overburn);
    void abortDaoRecording();

    int extractDao(Gtk::Window &parent,
		   const std::string& tocFile, const std::string& dataFile,
		   int correction,int readSubChanMode);
    void abortDaoReading();

    int duplicateDao(Gtk::Window &parent, int simulate, int multiSession, int speed, int eject,
                     int reload, int buffer, int onthefly, int correction, int readSubChanMode,
                     CdDevice *readdev);
    void abortDaoDuplication();

    int blank(Gtk::Window *parent, int fast, int speed, int eject, int reload);
    void abortBlank();

    int progressStatusChanged();
    void progress(int *status, int *totalTracks, int *track, int *trackProgress, int *totalProgress,
                  int *bufferFill, int *writerFill) const;

    CdrDriver* createDriver();

    sigc::signal<void(CdDevice*)> signalProcessFinished;

    // Static methods
    //
    
    static DeviceType devtypeName2Id(const std::string&);

    static const std::string status2string(Status);
    static const std::string deviceType2string(DeviceType);

    static void importSettings();
    static void exportSettings();

    static CdDevice *add(const std::string& scsidev, const std::string& vendor, const std::string& product);
    static CdDevice *add(const std::string& setting);
    static CdDevice *find(const std::string dev);
    static bool scan();
    static void clear();
    static int update();

    static std::vector<CdDevice*>&   deviceList()  { return DEVICE_LIST; }
    static std::vector<std::string>& driverNames() { return DRIVER_NAMES; }
    static std::vector<std::string>  deviceNames() { return DEV_TYPE_NAMES; }
    static std::vector<std::string>  statusNames() { return STATUS_NAMES; }

  private:
    std::string dev_; // SCSI device
    std::string vendor_;
    std::string product_;
    std::string description_;

    DeviceType deviceType_ = CD_R;

    std::string driver_;
    unsigned long driverOptions_ = 0;
    bool manuallyConfigured_ = false;

    ScsiIf *scsiIf_ = nullptr;
    bool scsiIfInitFailed_ = false;
    Status status_ = DEV_UNKNOWN;

    enum Action action_;

    int exitStatus_ = 0;

    int progressStatusChanged_ = 0;
    int progressStatus_ = 0;
    int progressTotalTracks_ = 0;
    int progressTrack_ = 0;
    int progressTotal_ = 0;
    int progressTrackRelative_ = 0;
    int progressBufferFill_ = 0;
    int progressWriterFill_ = 0;

    Process *process_ = nullptr;

    // slave device (used when copying etc.)
    CdDevice *slaveDevice_ = nullptr;

    void createScsiIf();

    static std::vector<std::string> DRIVER_NAMES;
    static std::vector<CdDevice*> DEVICE_LIST;
    static std::vector<std::string> DEV_TYPE_NAMES;
    static std::vector<std::string> STATUS_NAMES;
};

#endif

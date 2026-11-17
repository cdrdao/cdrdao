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
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "DeviceMonitor.h"

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
        DEV_SPINNING_UP,
        DEV_BUSY,
        DEV_NO_DISC,
        DEV_BLANKING,
        DEV_FAULT,
        DEV_LOCKED,
        DEV_TRAY_OPEN,
        DEV_BAD_DISC,
        DEV_UNKNOWN
    };
    // The medium in a ready drive, see medium().
    enum Medium {
        MEDIUM_NONE,    // not identified (yet), or drive not ready
        MEDIUM_UNKNOWN, // a disc that could not be identified
        MEDIUM_BLANK_CDR,
        MEDIUM_BLANK_CDRW,
        MEDIUM_AUDIO_CD,
        MEDIUM_DATA_CD,
        MEDIUM_MIXED_CD,
        MEDIUM_DVD,
        MEDIUM_BD
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

    CdDevice(const std::string &dev, const std::string &vendor, const std::string &product);
    ~CdDevice();

    Glib::ustring settingString() const;

    const std::string &dev() const { return dev_; }
    const std::string &vendor() const { return vendor_; }
    const std::string &product() const { return product_; }
    const std::string &description() const { return description_; }

    Status status() const { return status_; }

    // The medium in the drive. Only meaningful while status() is
    // DEV_READY; MEDIUM_NONE otherwise.
    Medium medium() const { return medium_; }

    // Why the medium in the drive is unsuitable for 'action' (A_RECORD,
    // A_READ or A_BLANK), as a sentence for the user; empty if it looks
    // suitable or is not known.
    Glib::ustring mediumProblem(Action action) const;

    // Whether the drive holds a disc with audio tracks.
    bool hasAudioDisc() const
    {
        return status_ == DEV_READY && (medium_ == MEDIUM_AUDIO_CD || medium_ == MEDIUM_MIXED_CD);
    }

    // Text for the status column of the device lists: the medium while
    // the drive is ready, the status otherwise.
    std::string statusText() const;
    Process *process() const { return process_; }

    int exitStatus() const;

    void status(Status);

    // Schedules an asynchronous TEST UNIT READY on a worker thread,
    // followed by the identification of the medium if the drive became
    // ready or the medium may have changed. The result is applied on the
    // main thread, which emits signalStatusChanged() if the status or
    // the medium changed. Does nothing while the drive is used by a cdrdao
    // process or another I/O on it is pending.
    void pollStatus();

    // Whether the drive is currently driven by a cdrdao child process.
    bool busy() const;

    // Whether the device is still attached. Devices that disappear are
    // kept alive (see updateDeviceList()) but flagged as not present.
    bool present() const { return present_; }

    // Whether the asynchronous identification (driver/type detection,
    // see probe()) has completed. Devices are hidden from the selectors
    // until then.
    bool probed() const { return probed_; }

    Action action() const { return action_; }

    bool updateProgress(Glib::IOCondition, int fd);

    // Identifies the drive and selects its driver and device type, on a
    // worker thread. Settings the user entered manually are kept unless
    // 'force' is set. When done, signalListChanged() and
    // guiUpdate(UPD_CD_DEVICES) are emitted.
    void probe(bool force = false);

    const std::string &driver() const { return driver_; }
    void driver(const std::string &d) { driver_ = d; }

    DeviceType deviceType() const;
    std::string deviceTypeName() const { return CdDevice::deviceType2string(deviceType()); }
    void deviceType(DeviceType);
    void deviceType(const std::string &t) { deviceType(CdDevice::devtypeName2Id(t)); }

    unsigned long driverOptions() const;
    void driverOptions(unsigned long);

    bool manuallyConfigured() const { return manuallyConfigured_; }
    void manuallyConfigured(bool b) { manuallyConfigured_ = b; }

    bool ejectCd(bool load = false);
    bool loadCd() { return ejectCd(true); }

    bool recordDao(Gtk::Window &parent, TocEdit *, int simulate, int multiSession, int speed,
                   int eject, int reload, int buffer, int overburn);
    void abortDaoRecording();

    int extractDao(Gtk::Window &parent, const std::string &tocFile, const std::string &dataFile,
                   int correction, int readSubChanMode);
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

    // The SCSI interface, shared with the driver. Opened lazily and
    // released after each use so that the drive is not held open.
    std::shared_ptr<ScsiIf> scsiIf() const { return scsiIf_; }

    // Serializes in-process I/O on this drive (status polling, probing,
    // TOC reading, eject). Worker code that sends commands to scsiIf()
    // must hold it. While a cdrdao child process drives the device, the
    // main thread holds it on the child's behalf (see releaseDevice()),
    // so callers should use try_lock() rather than block.
    std::timed_mutex &ioMutex() { return ioMutex_; }

    sigc::signal<void(CdDevice *)> signalProcessFinished;

    // Static methods
    //

    static DeviceType devtypeName2Id(const std::string &);

    static const std::string status2string(Status);
    static const std::string medium2string(Medium);
    static const std::string deviceType2string(DeviceType);

    static void importSettings();
    static void exportSettings();

    static CdDevice *add(const std::string &scsidev, const std::string &vendor,
                         const std::string &product);
    static CdDevice *add(const std::string &setting);
    static CdDevice *find(const std::string dev);

    // Starts the application-wide DeviceMonitor. DEVICE_LIST is then kept
    // in sync with the attached drives, and listeners are notified
    // through signalListChanged() and guiUpdate(UPD_CD_DEVICES). Must be
    // called on the main thread once the GTK application has started.
    static void startMonitor();
    static void stopMonitor();

    // Asks the monitor to rescan for devices now.
    static void rescan();

    // Reconcile DEVICE_LIST with the set of devices reported by a
    // DeviceMonitor. New devices are added and probed; devices that have
    // disappeared are removed from DEVICE_LIST. Removed CdDevice objects
    // are never deleted, because raw pointers to them are held all over
    // the GUI (selectors, progress dialogs, a running cdrdao process);
    // they are flagged as not present instead. Entries that persist keep
    // their existing CdDevice object (and thus their status and any
    // running process). Returns true if the list was modified.
    static bool updateDeviceList(const std::vector<DeviceMonitor::DeviceInfo> &devices);

    // Periodic update, called on the main thread: reaps finished cdrdao
    // processes. Returns UPD_* flags for the changes detected.
    static int update();

    // Emitted on the main thread when DEVICE_LIST changes, or when a
    // device's identification (driver, type) changes.
    static sigc::signal<void()> &signalListChanged();

    // Emitted on the main thread when a device's status changes.
    static sigc::signal<void(CdDevice *)> &signalStatusChanged();

    static std::vector<CdDevice *> &deviceList() { return DEVICE_LIST; }
    static std::vector<std::string> &driverNames() { return DRIVER_NAMES; }
    static std::vector<std::string> deviceNames() { return DEV_TYPE_NAMES; }

  private:
    std::string dev_; // SCSI device
    std::string vendor_;
    std::string product_;
    std::string description_;

    DeviceType deviceType_ = CD_R;

    std::string driver_;
    unsigned long driverOptions_ = 0;
    bool manuallyConfigured_ = false;

    std::shared_ptr<ScsiIf> scsiIf_;
    CdrDriver *cdDriver_ = nullptr;
    Status status_ = DEV_UNKNOWN;
    Medium medium_ = MEDIUM_NONE;
    // Details of medium_: READ DISC INFORMATION disc status (-1 unknown,
    // 0 blank, 1 appendable, 2 complete) and erasable bit.
    int mediumDiscStatus_ = -1;
    bool mediumErasable_ = false;
    // The medium must be identified at the next poll that finds the drive
    // ready (e.g. the system reported a media change).
    bool mediumStale_ = true;
    // Identification attempts that gave MEDIUM_UNKNOWN; see pollStatus().
    int mediumAttempts_ = 0;

    bool present_ = true;
    bool probed_ = false;
    // Whether the last probe managed to open and identify the drive.
    bool initOk_ = false;
    // Number of failed probe attempts so far; see PROBE_RETRY_DELAYS_MS.
    size_t probeAttempt_ = 0;
    // A probe or status task is queued or running (main thread only).
    bool ioPending_ = false;
    // A probe was requested while another task was pending.
    bool probeRequested_ = false;
    bool probeForce_ = false;
    std::timed_mutex ioMutex_;
    // Holds ioMutex_ while a cdrdao child process uses the device.
    std::unique_lock<std::timed_mutex> processLock_;

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

    void createDriver();

    // Hands the drive over to a cdrdao child process that is about to be
    // started: waits for in-process I/O (status poll, probe, TOC read) to
    // finish, then keeps ioMutex_ locked so that none starts until the
    // process is reaped, and closes our handle so that the child can open
    // the drive (exclusively on some platforms). Returns false, with an
    // error logged, if the drive stays busy; the process must not be
    // started then.
    bool releaseDevice();

    // Takes the drive back from a child process (on exit, or if it could
    // not be started), allowing in-process I/O again.
    void reclaimDevice();

    // Sets status_ (and medium_, which is reset unless the drive is
    // ready), emitting signalStatusChanged() if either changed.
    void setStatus(Status s);
    void setStatus(Status s, Medium m);

    // Reaps the cdrdao child process if it has exited. Returns true if
    // the status changed.
    bool checkProcess();

    // Runs a deferred probe request, if any (see probe()).
    void runDeferredProbe();

    friend class DeviceProbeTask;
    friend class DeviceStatusTask;

    static std::vector<std::string> DRIVER_NAMES;
    static std::vector<CdDevice *> DEVICE_LIST;
    // Devices that were unplugged; see updateDeviceList().
    static std::vector<CdDevice *> REMOVED_LIST;
    static std::vector<std::string> DEV_TYPE_NAMES;
    static std::vector<std::string> STATUS_NAMES;
};

#endif

/*
 * Native implementation of cdrdao's SCSI interface for Mac OS X.
 * Copyright (C) by Edgar Fuﬂ, Bonn, July 2007.
 * Do with this whatever you like, as long as you are either me or you keep
 * this message intact and both
 * - acknowledge that I wrote it for cdrdao in the first place, and
 * - don't blame me if it doesn't do what you like or expect.
 * These routines do exactly what they do. If that's not what you expect them
 * or would like them to do, don't complain with me, the cdrdao project, my
 * neighbour's brother-in-law or anybody else, but rewrite them to your taste.
 */

/*
 * Device naming
 *
 * macOS has no short device node for optical drives, so each drive found
 * by scan() gets a small number ("1", "2", ...), the same 1-based scheme
 * 'drutil list' uses. Numbers are handed out in the order the drives were
 * attached (IORegistry entry ID); a drive keeps its number for the life of
 * the process, and a newly attached drive gets the lowest free number.
 *
 * Numbers can change between runs when drives come and go, so a drive can
 * also be named by its unit serial number, its full IOService-plane path,
 * or the BSD node of the inserted disc ("/dev/disk4"). ScanData::id holds
 * the serial number (or the path when there is none) for callers that need
 * a name that stays the same, such as gcdmaster's saved settings.
 *
 * Drives are not held open between commands. scan() only opens a drive
 * briefly to read its serial number; it gets exclusive access when a
 * command first uses it (init() or sendCmd()). A drive that cannot be
 * opened, typically because a disc in it is mounted, is still listed and
 * flagged as in use.
 */

/* standard includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

/* cdrdao specific includes and prototype */
#include "ScsiIf.h"
#include "decodeSense.cc"
#include "log.h"
#include "trackdb/util.h"

/* Mac OS X specific includes */
#include <CoreFoundation/CFPlugInCOM.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>


class ScsiIfOsx : public ScsiIf
{
  public:
    ScsiIfOsx(const std::string& dev);
    ~ScsiIfOsx() override;

    virtual int init() override;
    virtual int sendCmd(const u8 *, int, const u8 *, int, u8 *, int, int showMessage = 1) override;
    virtual const u8 *getSense(int &len) const override;
    virtual void printError() override;
    virtual int timeout(int) override;

    // Opens the drive with exclusive access and keeps it open until the
    // object is destroyed. Returns 0 on success.
    int acquire();

    // Opens the drive briefly to update the in-use flag and read the
    // serial number. Does nothing if the drive is already acquired.
    void probe();

    // Returns the unit serial number from INQUIRY VPD page 0x80, or from
    // the MMC Logical Unit Serial Number feature (0x0108) for drives that
    // don't support VPD pages. Returns an empty string if neither works.
    std::string serialNumber();

    // Fills vendor, product and revision from the IORegistry, which works
    // without opening the drive.
    void readCharacteristics();

    // BSD name of the disc in the drive ("disk4"), or empty if none.
    std::string mediaBsdName() const;

    using ScsiIf::dev_;
    std::string path_;
    std::string serial_;
    int num_ = 0;
    uint64_t entryId_ = 0;
    io_object_t object_ = 0;
    std::atomic<bool> inUse_{false};

  private:
    int open();
    void close();

    IOCFPlugInInterface **plugin_ = nullptr;
    MMCDeviceInterface **mmc_ = nullptr;
    SCSITaskDeviceInterface **scsi_ = nullptr;
    bool exclusive_ = false;

    // Set once acquire() succeeds; the drive then stays open.
    std::atomic<bool> held_{false};
    // True while probe() has the drive open, so that its own commands
    // don't try to acquire it.
    bool probing_ = false;
    // Guards open/close. Recursive because probe() and acquire() hold it
    // while sending commands, and sendCmd() takes it too.
    std::recursive_mutex mutex_;

    long timeout_ms_ = 10 * 1000;
    std::string error_;
    SCSIServiceResponse response_;
    SCSITaskStatus status_;
    struct SCSI_Sense_Data sense_;
};

ScsiIfOsx::~ScsiIfOsx()
{
    close();
    if (object_)
        IOObjectRelease(object_);
}

int ScsiIfOsx::open()
{
    if (scsi_)
        return 0;

    SInt32 score;
    if (IOCreatePlugInInterfaceForService(object_, kIOMMCDeviceUserClientTypeID,
                                          kIOCFPlugInInterfaceID, &plugin_,
                                          &score) != kIOReturnSuccess ||
        !plugin_) {
        error_ = "cannot create plugin interface";
        close();
        return 1;
    }
    (*plugin_)->QueryInterface(plugin_, CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
                               (LPVOID *)&mmc_);
    if (!mmc_) {
        error_ = "no MMC interface";
        close();
        return 1;
    }
    scsi_ = (*mmc_)->GetSCSITaskDeviceInterface(mmc_);
    if (!scsi_) {
        error_ = "no SCSI task interface";
        close();
        return 1;
    }
    if ((*scsi_)->ObtainExclusiveAccess(scsi_) != kIOReturnSuccess) {
        error_ = "device is in use";
        close();
        return 1;
    }
    exclusive_ = true;
    return 0;
}

void ScsiIfOsx::close()
{
    if (scsi_) {
        if (exclusive_)
            (*scsi_)->ReleaseExclusiveAccess(scsi_);
        (*scsi_)->Release(scsi_);
        scsi_ = nullptr;
    }
    exclusive_ = false;
    if (mmc_) {
        (*mmc_)->Release(mmc_);
        mmc_ = nullptr;
    }
    if (plugin_) {
        IODestroyPlugInInterface(plugin_);
        plugin_ = nullptr;
    }
}

int ScsiIfOsx::acquire()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (held_)
        return 0;
    if (open() != 0) {
        inUse_ = true;
        return 1;
    }
    inUse_ = false;
    held_ = true;
    return 0;
}

void ScsiIfOsx::probe()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (held_)
        return;
    if (open() != 0) {
        inUse_ = true;
        return;
    }
    inUse_ = false;

    probing_ = true;
    if (serial_.empty())
        serial_ = serialNumber();
    if (vendor_.empty() && product_.empty())
        inquiry();
    probing_ = false;

    close();
}

// Returns a string value of a CF dictionary, trimmed, or "" if absent.
static std::string dictString(CFDictionaryRef dict, CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
        return "";

    char buf[256];
    if (!CFStringGetCString((CFStringRef)value, buf, sizeof(buf), kCFStringEncodingUTF8))
        return "";
    std::string s(buf);
    Util::trimSpaces(s);
    return s;
}

void ScsiIfOsx::readCharacteristics()
{
    CFTypeRef prop = IORegistryEntryCreateCFProperty(
        object_, CFSTR(kIOPropertyDeviceCharacteristicsKey), kCFAllocatorDefault, 0);
    if (!prop)
        return;
    if (CFGetTypeID(prop) == CFDictionaryGetTypeID()) {
        auto dict = (CFDictionaryRef)prop;
        vendor_ = dictString(dict, CFSTR(kIOPropertyVendorNameKey));
        product_ = dictString(dict, CFSTR(kIOPropertyProductNameKey));
        revision_ = dictString(dict, CFSTR(kIOPropertyProductRevisionLevelKey));
    }
    CFRelease(prop);
}

std::string ScsiIfOsx::mediaBsdName() const
{
    // The whole-disc media object is the first BSD node below the drive.
    CFTypeRef prop = IORegistryEntrySearchCFProperty(object_, kIOServicePlane, CFSTR("BSD Name"),
                                                     kCFAllocatorDefault,
                                                     kIORegistryIterateRecursively);
    if (!prop)
        return "";
    std::string name;
    if (CFGetTypeID(prop) == CFStringGetTypeID()) {
        char buf[64];
        if (CFStringGetCString((CFStringRef)prop, buf, sizeof(buf), kCFStringEncodingUTF8))
            name = buf;
    }
    CFRelease(prop);
    return name;
}

class DeviceManager
{
public:
    DeviceManager() {};

    void scan();

    std::shared_ptr<ScsiIfOsx> find(const std::string& dev);

    // Drives sorted by number.
    std::vector<std::shared_ptr<ScsiIfOsx>> sorted() const;

    // The serial number if no other drive has the same one, else "".
    std::string uniqueSerial(const ScsiIfOsx& sif) const;

    // Keyed by full IOService-plane path.
    std::map<std::string, std::shared_ptr<ScsiIfOsx>> devmap;
};

DeviceManager* DM = new DeviceManager();

// Serializes access to DM: gcdmaster scans from its device monitor thread
// while the main thread may be creating interfaces.
static std::mutex DMmutex;

std::string DeviceManager::uniqueSerial(const ScsiIfOsx& sif) const
{
    if (sif.serial_.empty())
        return "";
    for (auto const& [path, other] : devmap) {
        if (other.get() != &sif && other->serial_ == sif.serial_)
            return "";
    }
    return sif.serial_;
}

std::vector<std::shared_ptr<ScsiIfOsx>> DeviceManager::sorted() const
{
    std::vector<std::shared_ptr<ScsiIfOsx>> list;
    for (auto const& [path, sif] : devmap)
        list.push_back(sif);
    std::sort(list.begin(), list.end(),
              [](const auto& a, const auto& b) { return a->num_ < b->num_; });
    return list;
}

std::shared_ptr<ScsiIfOsx> DeviceManager::find(const std::string& dev)
{
    auto it = devmap.find(dev);
    if (it != devmap.end())
        return it->second;

    for (auto const& [path, sif] : devmap) {
        if (sif->dev_ == dev)
            return sif;
        if (!sif->serial_.empty() && uniqueSerial(*sif) == dev)
            return sif;
    }

    // Accept a BSD node name ("/dev/disk4", as reported by Disk
    // Arbitration) by walking up the IOService plane from the media
    // object until we reach one of our authoring devices.
    std::string bsd = dev;
    if (bsd.rfind("/dev/", 0) == 0)
        bsd = bsd.substr(5);
    if (bsd.rfind("rdisk", 0) == 0)
        bsd = bsd.substr(1);
    if (bsd.rfind("disk", 0) != 0)
        return nullptr;

    io_registry_entry_t entry = IOServiceGetMatchingService(
        kIOMainPortDefault, IOBSDNameMatching(kIOMainPortDefault, 0, bsd.c_str()));
    std::shared_ptr<ScsiIfOsx> found;
    while (entry && !found) {
        io_string_t path;
        if (IORegistryEntryGetPath(entry, kIOServicePlane, path) == KERN_SUCCESS) {
            auto it = devmap.find(path);
            if (it != devmap.end())
                found = it->second;
        }
        io_registry_entry_t parent = 0;
        if (IORegistryEntryGetParentEntry(entry, kIOServicePlane, &parent) != KERN_SUCCESS)
            parent = 0;
        IOObjectRelease(entry);
        entry = parent;
    }
    if (entry)
        IOObjectRelease(entry);
    return found;
}

void DeviceManager::scan()
{
    struct Found {
        uint64_t entryId;
        std::string path;
        io_object_t object;
    };
    std::vector<Found> found;

    auto dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    auto sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(sub, CFSTR(kIOPropertySCSITaskDeviceCategory),
                         CFSTR(kIOPropertySCSITaskAuthoringDevice));
    CFDictionarySetValue(dict, CFSTR(kIOPropertyMatchKey), sub);
    CFRelease(sub);

    // Consumes dict.
    io_iterator_t iterator = 0;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, dict, &iterator) == KERN_SUCCESS &&
        iterator) {
        while (io_object_t object = IOIteratorNext(iterator)) {
            io_string_t path;
            uint64_t entryId = 0;
            if (IORegistryEntryGetPath(object, kIOServicePlane, path) != KERN_SUCCESS) {
                IOObjectRelease(object);
                continue;
            }
            IORegistryEntryGetRegistryEntryID(object, &entryId);
            found.push_back({entryId, path, object});
        }
        IOObjectRelease(iterator);
    }

    // Forget drives that are gone, so their numbers become free.
    std::set<std::string> present;
    for (auto const& f : found)
        present.insert(f.path);
    for (auto it = devmap.begin(); it != devmap.end();) {
        if (present.count(it->first) == 0)
            it = devmap.erase(it);
        else
            it++;
    }

    // Number new drives in attach order.
    std::sort(found.begin(), found.end(),
              [](const Found& a, const Found& b) { return a.entryId < b.entryId; });

    for (auto const& f : found) {
        auto it = devmap.find(f.path);
        if (it != devmap.end()) {
            IOObjectRelease(f.object);
            it->second->probe();
            continue;
        }

        std::set<int> used;
        for (auto const& [path, sif] : devmap)
            used.insert(sif->num_);
        int num = 1;
        while (used.count(num))
            num++;

        auto sif = std::make_shared<ScsiIfOsx>(std::to_string(num));
        sif->num_ = num;
        sif->path_ = f.path;
        sif->entryId_ = f.entryId;
        sif->object_ = f.object;
        sif->readCharacteristics();
        sif->probe();
        devmap[f.path] = sif;

        log_message(3, "Scan: found device %d: %s %s%s (%s)", num, sif->vendor().c_str(),
                    sif->product().c_str(), sif->inUse_ ? " [in use]" : "",
                    sif->serial_.empty() ? f.path.c_str() : sif->serial_.c_str());
    }
}

ScsiIfOsx::ScsiIfOsx(const std::string& dev)
{
    dev_ = dev;
    maxDataLen_ = 64 * 1024;
}

int ScsiIfOsx::init()
{
    if (acquire() != 0) {
        log_message(-2, "Cannot open device %s (%s %s): %s.", dev_.c_str(), vendor_.c_str(),
                    product_.c_str(), error_.c_str());
        auto bsd = mediaBsdName();
        if (!bsd.empty())
            log_message(-2, "If the disc is mounted, unmount it first with "
                            "'diskutil unmountDisk %s'.",
                        bsd.c_str());
        return 1;
    }

    if (vendor_.empty() && product_.empty() && inquiry() != 0)
        return 2;

    return 0;
}

// Converts a serial number field to a string. Returns an empty string
// if it contains non-printable characters.
static std::string serialString(const unsigned char *buf, int len)
{
    std::string serial;
    for (int i = 0; i < len; i++) {
        if (buf[i] == 0)
            break;
        if (!isprint(buf[i]))
            return "";
        serial += buf[i];
    }
    Util::trimSpaces(serial);
    return serial;
}

std::string ScsiIfOsx::serialNumber()
{
    unsigned char result[0xfc];

    // INQUIRY, Unit Serial Number VPD page
    unsigned char inq[6] = { 0x12 /* INQUIRY */, 0x01 /* EVPD */, 0x80, 0, sizeof(result), 0 };
    memset(result, 0, sizeof(result));
    if (sendCmd(inq, 6, NULL, 0, result, sizeof(result), 0) == 0 && result[1] == 0x80) {
        int len = std::min<int>(result[3], sizeof(result) - 4);
        auto serial = serialString(result + 4, len);
        if (!serial.empty())
            return serial;
    }

    // GET CONFIGURATION, Logical Unit Serial Number feature (RT=2: only
    // the requested feature)
    unsigned char cfg[10] = { 0x46 /* GET CONFIGURATION */, 0x02, 0x01, 0x08, 0, 0, 0, 0,
                              sizeof(result), 0 };
    memset(result, 0, sizeof(result));
    if (sendCmd(cfg, 10, NULL, 0, result, sizeof(result), 0) == 0) {
        // 8-byte feature header, then the feature descriptor
        const unsigned char *desc = result + 8;
        if (desc[0] == 0x01 && desc[1] == 0x08) {
            int len = std::min<int>(desc[3], sizeof(result) - 12);
            return serialString(desc + 4, len);
        }
    }

    return "";
}

int ScsiIfOsx::timeout(int t)
{
    int ret = timeout_ms_ / 1000;
    timeout_ms_ = t * 1000;
    return ret;
}

int ScsiIfOsx::sendCmd(const unsigned char *cmd, int cmdLen, const unsigned char *dataOut,
                       int dataOutLen, unsigned char *dataIn, int dataInLen, int showMessage)
{
    SCSITaskInterface **task = nullptr;
    IOVirtualRange range;
    IOReturn ret;
    UInt64 len;

    error_.clear();

    auto ERROR=[&](const std::string msg) {
        error_ = "sendCmd: " + msg;
        if (showMessage)
            printError();
        if (task)
            (*task)->Release(task);
    };

    // Open the drive on first use. Waits for a probe() running on another
    // thread to finish; probe()'s own commands skip this.
    if (!held_) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (!held_ && !probing_ && acquire() != 0) {
            ERROR(error_);
            return 1;
        }
    }

    task = (*scsi_)->CreateSCSITask(scsi_);
    if (!task) {
        ERROR("no task");
        return 1;
    }
    ret = (*task)->SetCommandDescriptorBlock(task, (UInt8 *)cmd, cmdLen);
    if (ret != kIOReturnSuccess) {
        ERROR("SetCommandDescriptorBlock failed");
        return 1;
    }
    /* The OSX SCSI interface can't deal with two data phases */
    if (dataIn && dataOut) {
        ERROR("dataIn && dataOut");
        return 1;
    }
    if (dataIn) {
        range.address = (IOVirtualAddress)dataIn;
        range.length = dataInLen;
        ret = (*task)->SetScatterGatherEntries(task, &range, 1, dataInLen,
                                               kSCSIDataTransfer_FromTargetToInitiator);
    } else if (dataOut) {
        range.address = (IOVirtualAddress)dataOut;
        range.length = dataOutLen;
        ret = (*task)->SetScatterGatherEntries(task, &range, 1, dataOutLen,
                                               kSCSIDataTransfer_FromInitiatorToTarget);
    } else {
        /* Just to make sure. We pass in zero ranges anyway */
        range.address = (IOVirtualAddress)NULL;
        range.length = 0;
        ret =
            (*task)->SetScatterGatherEntries(task, &range, 0, 0, kSCSIDataTransfer_NoDataTransfer);
    }
    if (ret != kIOReturnSuccess) {
        ERROR("SetScatterGatherEntries failed");
        return 1;
    }
    ret = (*task)->SetTimeoutDuration(task, timeout_ms_);
    if (ret != kIOReturnSuccess) {
        ERROR("SetTimeoutDuration failed");
        return 1;
    }
    ret = (*task)->ExecuteTaskSync(task, &sense_, &status_, &len);
    if (ret != kIOReturnSuccess) {
        ERROR("ExecuteTaskSync failed");
        return 1;
    }
    ret = (*task)->GetSCSIServiceResponse(task, &response_);
    if (ret != kIOReturnSuccess) {
        ERROR("GetSCSIServiceResponse failed");
        return 1;
    }
    (*task)->Release(task);
    if (response_ == kSCSIServiceResponse_TASK_COMPLETE) {
        if (status_ == kSCSITaskStatus_GOOD)
            return 0;
        if (status_ == kSCSITaskStatus_CHECK_CONDITION)
            return 2;
    }
    if (response_ == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE)
        return 1;
    return 1 /* XXX This shouldn't happen */;
}

const unsigned char *ScsiIfOsx::getSense(int &len) const
{
    len = kSenseDefaultSize;
    return (unsigned char *)&sense_;
}

void ScsiIfOsx::printError()
{
    const char *s;

    if (!error_.empty())
        /* Internal error in sendCmd(). We saved a message string. */
        s = error_.c_str();
    else
        switch (response_) {
        case kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE:
            /* The SCSI command didn't complete */
            switch (status_) {
            case kSCSITaskStatus_TaskTimeoutOccurred:
                s = "task timeout";
                break;
            case kSCSITaskStatus_ProtocolTimeoutOccurred:
                s = "protocol timeout";
                break;
            case kSCSITaskStatus_DeviceNotResponding:
                s = "device not responding";
                break;
            case kSCSITaskStatus_DeviceNotPresent:
                s = "device not present";
                break;
            case kSCSITaskStatus_DeliveryFailure:
                s = "delivery failure";
                break;
            case kSCSITaskStatus_No_Status:
                s = "no status";
                break;
            default:
                s = "failure, unknown status";
                break;
            }
            break;
        case kSCSIServiceResponse_TASK_COMPLETE:
            /* The SCSI command did complete */
            switch (status_) {
            case kSCSITaskStatus_GOOD:
                s = "good";
                break;
            case kSCSITaskStatus_CHECK_CONDITION:
                decodeSense((unsigned char *)&sense_, sizeof(sense_));
                s = NULL;
                break;
            case kSCSITaskStatus_CONDITION_MET:
                s = "condition met";
                break;
            case kSCSITaskStatus_BUSY:
                s = "busy";
                break;
            case kSCSITaskStatus_INTERMEDIATE:
                s = "intermediate";
                break;
            case kSCSITaskStatus_INTERMEDIATE_CONDITION_MET:
                s = "intermediate, condition met";
                break;
            case kSCSITaskStatus_RESERVATION_CONFLICT:
                s = "reservation conflict";
                break;
            case kSCSITaskStatus_TASK_SET_FULL:
                s = "task set full";
                break;
            case kSCSITaskStatus_ACA_ACTIVE:
                s = "aca active";
                break;
            default:
                s = "complete, unknown status";
                break;
            }
            break;
        default:
            s = "unknown response";
            break;
        }
    if (s)
        log_message(-2, s);
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char *)
{
    std::lock_guard<std::mutex> lock(DMmutex);

    if (!DM) {
        DM = new DeviceManager();
    }

    DM->scan();

    auto list = DM->sorted();
    *len = list.size();
    auto scanData = new ScanData[*len];

    log_message(3, "Scan: %d devices found", *len);

    int i = 0;
    for (auto const& sif : list) {
        scanData[i].dev = sif->dev_;
        scanData[i].vendor = sif->vendor_;
        scanData[i].product = sif->product_;
        scanData[i].revision = sif->revision_;
        auto serial = DM->uniqueSerial(*sif);
        scanData[i].id = serial.empty() ? sif->path_ : serial;
        scanData[i].inUse = sif->inUse_;
        i++;
    }
    return scanData;
}

std::shared_ptr<ScsiIf> ScsiIf::create(const std::string& dev)
{
    std::lock_guard<std::mutex> lock(DMmutex);

    if (!DM) {
        DM = new DeviceManager();
    }

    // Accept the drive number, the serial number, the full IOService-plane
    // path, or the BSD node name of the inserted media.
    auto sif = DM->find(dev);
    if (!sif) {
        DM->scan();
        sif = DM->find(dev);
    }
    if (!sif) {
        log_message(-2, "Unknown SCSI device %s", dev.c_str());
        return nullptr;
    }
    return sif;
}

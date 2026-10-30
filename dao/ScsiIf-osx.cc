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

/* standard includes */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <set>
#include <memory>

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

    // Returns the unit serial number from INQUIRY VPD page 0x80, or from
    // the MMC Logical Unit Serial Number feature (0x0108) for drives that
    // don't support VPD pages. Returns an empty string if neither works.
    std::string serialNumber();

    using ScsiIf::dev_;
    std::string path_;
    int num_ = 0;
    io_object_t object_ = 0;
    IOCFPlugInInterface **plugin_ = nullptr;
    MMCDeviceInterface **mmc_ = nullptr;
    SCSITaskDeviceInterface **scsi_ = nullptr;
    bool exclusive_ = false;
    long timeout_ms_ = 10 * 1000;
    std::string error_;
    SCSIServiceResponse response_;
    SCSITaskStatus status_;
    struct SCSI_Sense_Data sense_;
};

ScsiIfOsx::~ScsiIfOsx()
{
    if (scsi_) {
        if (exclusive_)
            (*scsi_)->ReleaseExclusiveAccess(scsi_);
        (*scsi_)->Release(scsi_);
    }
    if (mmc_)
        (*mmc_)->Release(mmc_);
    if (plugin_)
        IODestroyPlugInInterface(plugin_);
    if (object_)
        IOObjectRelease(object_);
}

class DeviceManager
{
public:
    DeviceManager() {};

    void scan();
    void delete_all();

    std::shared_ptr<ScsiIfOsx> find(const std::string& dev);

    // Keyed by full IOService-plane path.
    std::map<std::string, std::shared_ptr<ScsiIfOsx>> devmap;
};

DeviceManager* DM = new DeviceManager();

void DeviceManager::delete_all()
{
    devmap.clear();
}

std::shared_ptr<ScsiIfOsx> DeviceManager::find(const std::string& dev)
{
    auto it = devmap.find(dev);
    if (it != devmap.end())
        return it->second;

    for (auto const& [path, sif] : devmap) {
        if (sif->dev_ == dev)
            return sif;
    }
    return nullptr;
}

void DeviceManager::scan()
{
    std::set<std::string> scanned;

    auto dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    auto sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
    CFDictionarySetValue(sub, CFSTR(kIOPropertySCSITaskDeviceCategory),
                         CFSTR(kIOPropertySCSITaskAuthoringDevice));
    CFDictionarySetValue(dict, CFSTR(kIOPropertyMatchKey), sub);

    io_iterator_t iterator = 0;
    IOServiceGetMatchingServices(kIOMainPortDefault, dict, &iterator);
    if (!iterator) {
        delete_all();
        return;
    }
    while (auto object = IOIteratorNext(iterator)) {

        io_string_t path;
        
        if (IORegistryEntryGetPath(object, kIOServicePlane, path) != noErr) {
            continue;
        }
        // Check if we already have this device
        if (devmap.find(path) != devmap.end()) {
            scanned.insert(path);
            continue;
        }

        auto sif = std::make_shared<ScsiIfOsx>(path);
        sif->path_ = path;
        sif->object_ = object;

        SInt32 score;
        if (IOCreatePlugInInterfaceForService(object, kIOMMCDeviceUserClientTypeID,
                                              kIOCFPlugInInterfaceID,
                                              &(sif->plugin_), &score) != noErr) {
            log_message(-2, "scan: IOCreatePlugInInterfaceForService failed");
            continue;
        }
        if (!sif->plugin_) {
            log_message(-2, "scan: no plugin");
            continue;
        }
        auto herr = (*(sif->plugin_))->
            QueryInterface(sif->plugin_, CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
                           (LPVOID *)&(sif->mmc_));
        if (!sif->mmc_) {
            log_message(-2, "scan: no mmc");
            continue;
        }
        sif->scsi_ = (*(sif->mmc_))->GetSCSITaskDeviceInterface(sif->mmc_);
        if (!sif->scsi_) {
            log_message(-2, "scan: no scsi");
            continue;
        }
        if ((*(sif->scsi_))->ObtainExclusiveAccess(sif->scsi_) != noErr) {
            log_message(-2, "Device already in use, please use diskutil "
                        "to unmount the disc first");
            continue;
        }
        sif->exclusive_ = true;

        if (sif->inquiry() != 0) {
            log_message(-2, "scan: inq failed");
            continue;
        }

        // Use the serial number as the device name, unless it's missing
        // or already used by another drive.
        auto serial = sif->serialNumber();
        if (!serial.empty() && !find(serial))
            sif->dev_ = serial;

        log_message(0, "Scan: found device %s %s (%s)",
                    sif->vendor().c_str(), sif->product().c_str(), sif->dev_.c_str());
        scanned.insert(path);
        devmap[path] = sif;
    }

    // Remove devices that are no longer there.
    for (auto it = devmap.begin(); it != devmap.end();) {
        if (scanned.count(it->first) == 0)
            it = devmap.erase(it);
        else
            it++;
    }
}

ScsiIfOsx::ScsiIfOsx(const std::string& dev)
{
    dev_ = dev;
    maxDataLen_ = 64 * 1024;
}

int ScsiIfOsx::init()
{
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
    SCSITaskInterface **task;
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

#define MAX_SCAN 10

ScsiIf::ScanData *ScsiIf::scan(int *len, char *)
{
    if (!DM) {
        DM = new DeviceManager();
    }

    DM->scan();

    *len = DM->devmap.size();
    auto scanData = new ScanData[*len];

    log_message(0, "Scan: %d devices found", *len);
    
    int i = 0;
    for (auto const& [key, val] : DM->devmap)
    {
        scanData[i].dev = val->dev_;
        scanData[i].vendor = val->vendor_;
        scanData[i].product = val->product_;
        scanData[i].revision = val->revision_;
        i++;
    }
    return scanData;
}

std::shared_ptr<ScsiIf> ScsiIf::create(const std::string& dev)
{
    if (!DM) {
        DM = new DeviceManager();
    }

    // Accept either the serial number or the full IOService-plane path.
    auto sif = DM->find(dev);
    if (!sif) {
        DM->scan();
        sif = DM->find(dev);
    }
    if (!sif) {
        log_message(-2, "Unknown SCSI device %s", dev.c_str());
        throw("Creating unknown SCSI device.");
    }
    return sif;
}

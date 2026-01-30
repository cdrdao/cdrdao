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
#include <map>
#include <set>

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

int inq(SCSITaskDeviceInterface **scsi, SCSIServiceResponse *response, SCSITaskStatus *status,
        struct SCSI_Sense_Data *sense, char *vend, char *prod, char *rev);

    class ScsiIfImpl
{
  public:
    ScsiIfImpl() { printf("[SCSI] created\n"); }
    ~ScsiIfImpl();

    int num_ = 0;
    std::string path_;
    io_object_t object_ = 0;
    IOCFPlugInInterface **plugin_ = nullptr;
    MMCDeviceInterface **mmc_ = nullptr;
    SCSITaskDeviceInterface **scsi_ = nullptr;
    bool exclusive_ = false;
    long timeout_ = 10 * 1000;
    std::string error_;
    SCSIServiceResponse response_;
    SCSITaskStatus status_;
    struct SCSI_Sense_Data sense_;
    char vendor_[9];
    char product_[17];
    char revision_[5];
};

ScsiIfImpl::~ScsiIfImpl()
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

    printf("[SCSI] %s deleted\n", vendor_);
}

class DeviceManager
{
public:
    DeviceManager() {};

    void scan();
    void delete_all();

    std::map<std::string, ScsiIfImpl*> devmap;
};

DeviceManager* DM = nullptr;

void DeviceManager::delete_all()
{
    devmap.clear();
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

        auto impl = new ScsiIfImpl();
        impl->object_ = object;
        io_string_t path;
        
        if (IORegistryEntryGetPath(object, kIOServicePlane, path) != noErr) {
            delete(impl);
            continue;
        }
        // Check if we already have this device
        if (devmap.find(path) != devmap.end()) {
            delete(impl);
            scanned.insert(path);
            continue;
        }

        impl->path_ = path;            
        SInt32 score;
        if (IOCreatePlugInInterfaceForService(object, kIOMMCDeviceUserClientTypeID,
                                              kIOCFPlugInInterfaceID,
                                              &(impl->plugin_), &score) != noErr) {
            log_message(-2, "scan: IOCreatePlugInInterfaceForService failed");
            delete(impl);
            continue;
        }
        if (!impl->plugin_) {
            log_message(-2, "scan: no plugin");
            delete(impl);
            continue;
        }
        auto herr = (*(impl->plugin_))->
            QueryInterface(impl->plugin_, CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
                           (LPVOID *)&(impl->mmc_));
        if (!impl->mmc_) {
            log_message(-2, "scan: no mmc");
            delete(impl);
            continue;
        }
        impl->scsi_ = (*(impl->mmc_))->GetSCSITaskDeviceInterface(impl->mmc_);
        if (!impl->scsi_) {
            log_message(-2, "scan: no scsi");
            delete(impl);
            continue;
        }
        if ((*(impl->scsi_))->ObtainExclusiveAccess(impl->scsi_) != noErr) {
            log_message(-2, "Device already in use, please use diskutil "
                        "to unmount the disc first");
            delete(impl);
            continue;
        }
        impl->exclusive_ = true;

        if (inq(impl->scsi_, &(impl->response_), &(impl->status_),
                nullptr, impl->vendor_, impl->product_, impl->revision_) != 0) {
            log_message(-2, "scan: inq failed");
            delete(impl);
            continue;
        }

        log_message(0, "Scan: found device %s %s", impl->vendor_, impl->product_);
        scanned.insert(path);
        devmap[path] = impl;
    }

    // Remove devices that are no longer there.
    for (auto it = devmap.begin(); it != devmap.end();) {
        if (scanned.count(it->first) == 0)
            it = devmap.erase(it);
        else
            it++;
    }
}

ScsiIf::ScsiIf(const char *name)
{
    int len;
    int bus, targ, lun, count;

    if (DM->devmap.find(name) == DM->devmap.end()) {
        log_message(-2, "Unknown SCSI device %s", name); 
        throw("Creating unknown SCSI device.");
    }
    impl_ = DM->devmap[name];
    vendor_[0] = 0;
    product_[0] = 0;
    revision_[0] = 0;
    maxDataLen_ = 64 * 1024; /* XXX */
}

ScsiIf::~ScsiIf()
{
}

int ScsiIf::init()
{
    return 0;
}

int ScsiIf::timeout(int t)
{
    int ret = impl_->timeout_ / 1000;
    impl_->timeout_ = t * 1000;
    return ret;
}

int ScsiIf::sendCmd(const unsigned char *cmd, int cmdLen, const unsigned char *dataOut,
                    int dataOutLen, unsigned char *dataIn, int dataInLen, int showMessage)
{
    SCSITaskInterface **task;
    IOVirtualRange range;
    IOReturn ret;
    UInt64 len;

    impl_->error_.clear();

    auto ERROR=[&](const std::string msg) {
        impl_->error_ = "sendCmd: " + msg;
        if (showMessage)
            printError();
        if (task)
            (*task)->Release(task);
    };

    task = (*impl_->scsi_)->CreateSCSITask(impl_->scsi_);
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
    ret = (*task)->SetTimeoutDuration(task, impl_->timeout_);
    if (ret != kIOReturnSuccess) {
        ERROR("SetTimeoutDuration failed");
        return 1;
    }
    ret = (*task)->ExecuteTaskSync(task, &impl_->sense_, &impl_->status_, &len);
    if (ret != kIOReturnSuccess) {
        ERROR("ExecuteTaskSync failed");
        return 1;
    }
    ret = (*task)->GetSCSIServiceResponse(task, &impl_->response_);
    if (ret != kIOReturnSuccess) {
        ERROR("GetSCSIServiceResponse failed");
        return 1;
    }
    (*task)->Release(task);
    if (impl_->response_ == kSCSIServiceResponse_TASK_COMPLETE) {
        if (impl_->status_ == kSCSITaskStatus_GOOD)
            return 0;
        if (impl_->status_ == kSCSITaskStatus_CHECK_CONDITION)
            return 2;
    }
    if (impl_->response_ == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE)
        return 1;
    return 1 /* XXX This shouldn't happen */;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
    len = kSenseDefaultSize;
    return (unsigned char *)&impl_->sense_;
}

void ScsiIf::printError()
{
    const char *s;

    if (!impl_->error_.empty())
        /* Internal error in sendCmd(). We saved a message string. */
        s = impl_->error_.c_str();
    else
        switch (impl_->response_) {
        case kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE:
            /* The SCSI command didn't complete */
            switch (impl_->status_) {
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
            switch (impl_->status_) {
            case kSCSITaskStatus_GOOD:
                s = "good";
                break;
            case kSCSITaskStatus_CHECK_CONDITION:
                decodeSense((unsigned char *)&impl_->sense_, sizeof(impl_->sense_));
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

/*
 * Internal form or inquiry command.
 * Used by both inquiry() and scanData(), but with different data.
 */
int inq(SCSITaskDeviceInterface **scsi, SCSIServiceResponse *response, SCSITaskStatus *status,
        struct SCSI_Sense_Data *sense, char *vend, char *prod, char *rev)
{
    SCSICmd_INQUIRY_StandardData inq_data;
    SCSICommandDescriptorBlock cdb;
    SCSITaskInterface **task;
    IOVirtualRange range;
    IOReturn ret;
    UInt64 len;
    int i;

    task = (*scsi)->CreateSCSITask(scsi);
    if (!task) {
        log_message(-2, "inq: no task");
        return 1;
    }
    bzero(cdb, sizeof(cdb));
    cdb[0] = kSCSICmd_INQUIRY;
    cdb[4] = sizeof(inq_data);
    ret = (*task)->SetCommandDescriptorBlock(task, cdb, kSCSICDBSize_6Byte);
    if (ret != kIOReturnSuccess) {
        log_message(-2, "inq: SetCommandDescriptorBlock failed: %d", ret);
        (*task)->Release(task);
        return 1;
    }
    range.address = (IOVirtualAddress)&inq_data;
    range.length = sizeof(inq_data);
    ret = (*task)->SetScatterGatherEntries(task, &range, 1, sizeof(inq_data),
                                           kSCSIDataTransfer_FromTargetToInitiator);
    if (ret != kIOReturnSuccess) {
        log_message(-2, "inq: SetScatterGatherEntries failed: %d", ret);
        (*task)->Release(task);
        return 1;
    }
    ret = (*task)->SetTimeoutDuration(task, 1000);
    if (ret != kIOReturnSuccess) {
        log_message(-2, "inq: SetTimeoutDuration failed: %d", ret);
        (*task)->Release(task);
        return 1;
    }
    ret = (*task)->ExecuteTaskSync(task, sense, status, &len);
    if (ret != kIOReturnSuccess) {
        log_message(-2, "inq: ExecuteTaskSync failed: %d", ret);
        (*task)->Release(task);
        return 1;
    }
    ret = (*task)->GetSCSIServiceResponse(task, response);
    if (ret != kIOReturnSuccess) {
        log_message(-2, "inq: GetSCSIServiceResponse failed: %d", ret);
        (*task)->Release(task);
        return 1;
    }
    if (*response != kSCSIServiceResponse_TASK_COMPLETE) {
        log_message(-2, "inq: response=%d", *response);
        (*task)->Release(task);
        return 1;
    }
    if (*status != kSCSITaskStatus_GOOD) {
        log_message(-2, "inq: status=%d", *status);
        (*task)->Release(task);
        return 1;
    }
    (*task)->Release(task);
    /* Copy vendor/product/revision stripping traiiling spaces */
    i = kINQUIRY_VENDOR_IDENTIFICATION_Length;
    while (i > 0 && inq_data.VENDOR_IDENTIFICATION[i - 1] == ' ')
        i--;
    memcpy(vend, inq_data.VENDOR_IDENTIFICATION, i);
    vend[i] = '\0';
    i = kINQUIRY_PRODUCT_IDENTIFICATION_Length;
    while (i > 0 && inq_data.PRODUCT_IDENTIFICATION[i - 1] == ' ')
        i--;
    memcpy(prod, inq_data.PRODUCT_IDENTIFICATION, i);
    prod[i] = '\0';
    i = kINQUIRY_PRODUCT_REVISION_LEVEL_Length;
    while (i > 0 && inq_data.PRODUCT_REVISION_LEVEL[i - 1] == ' ')
        i--;
    memcpy(rev, inq_data.PRODUCT_REVISION_LEVEL, i);
    rev[i] = '\0';
    return 0;
}

int ScsiIf::inquiry()
{
    return inq(impl_->scsi_, &impl_->response_, &impl_->status_, &impl_->sense_, vendor_, product_,
               revision_);
}

#define MAX_SCAN 10

ScsiIf::ScanData *ScsiIf::scan(int *len, char *dev)
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
        scanData[i].dev = val->path_;
        strncpy(scanData[i].vendor, val->vendor_, 9);
        strncpy(scanData[i].product, val->product_, 17);
        strncpy(scanData[i].revision, val->revision_, 5);
        i++;
    }
    return scanData;
}

#include "ScsiIf-common.cc"

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
#include <stdio.h>
#include <ctype.h>
#include <string.h>

/* cdrdao specific includes and prototype */
#include "ScsiIf.h"
#include "trackdb/util.h"
extern void message(int level, const char *fmt, ...);
#include "decodeSense.cc"

/* Mac OS X specific includes */
#include <CoreFoundation/CFPlugInCOM.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>

class ScsiIfImpl {
public:
	int num_; /* number of device for compatibility mode */
	char *path_; /* native (IO registry) pathname of device */
	io_object_t object_;
	IOCFPlugInInterface **plugin_;
	MMCDeviceInterface  **mmc_;
	SCSITaskDeviceInterface **scsi_;
	int exclusive_;
	long timeout_; /* in ms */
	char *error_; /* sendCmd() internal error string */
	SCSIServiceResponse response_;
	SCSITaskStatus status_;
	struct SCSI_Sense_Data sense_;
};

ScsiIf::ScsiIf(const char *name)
{
	int len;
	int bus, targ, lun, count;

	impl_ = new ScsiIfImpl;
	impl_->num_ = 0;
	impl_->path_ = NULL;
	len = strlen(name);
	if (len) {
		if (isdigit(name[0])) {
			/* Compatibility mode. Just add bus+targ+lun */
			if (sscanf(name, "%i,%i,%i%n", &bus, &targ, &lun, &count) == 3 && count == len) {
				if ((bus >= 0) && (targ >= 0) && (lun >= 0))
					impl_->num_ = 1 + bus + targ + lun;	
			}
		} else {
			/* Native mode. Take name as IOreg path */
			impl_->path_ = strdupCC(name);
		}
	}
	impl_->object_ = 0;
	impl_->plugin_ = NULL;
	impl_->mmc_ = NULL;
	impl_->scsi_ = NULL;
	impl_->exclusive_ = 0;
	impl_->timeout_ = 10*1000;
	impl_->error_ = NULL;

	vendor_[0] = 0;
	product_[0] = 0;
	revision_[0] = 0;

	maxDataLen_ = 64*1024; /* XXX */
}

ScsiIf::~ScsiIf()
{
	if (impl_->scsi_) {
		if (impl_->exclusive_)
			(*impl_->scsi_)->ReleaseExclusiveAccess(impl_->scsi_);
		(*impl_->scsi_)->Release(impl_->scsi_);
	}
	if (impl_->mmc_)
		(*impl_->mmc_)->Release(impl_->mmc_);
	if (impl_->plugin_)
		IODestroyPlugInInterface(impl_->plugin_);
	if (impl_->object_)
		IOObjectRelease(impl_->object_);
	if (impl_->path_ != NULL) delete[] impl_->path_;
	if (impl_->error_ != NULL) delete[] impl_->error_;
	delete impl_;
}

int ScsiIf::init()
{
	CFMutableDictionaryRef dict = NULL;
	CFMutableDictionaryRef sub = NULL;
	io_iterator_t iterator = 0;
	kern_return_t err;
	SInt32 score;
	HRESULT herr;
	int i;
	
	if (impl_->num_) {
		/*
		 * Compatibility mode.
		 * Build dictionaries to search for num_'th device having the
		 * authoring property using an IO iterator.
		 */
		dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
		sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
		CFDictionarySetValue(sub, CFSTR(kIOPropertySCSITaskDeviceCategory),
		                          CFSTR(kIOPropertySCSITaskAuthoringDevice));
		CFDictionarySetValue(dict, CFSTR(kIOPropertyMatchKey), sub);
		IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iterator);
		if (!iterator) message(-2, "init: no iterator");
		if (iterator) {
			i = impl_->num_;
			do {
				impl_->object_ = IOIteratorNext(iterator);
				i--;
			} while (i && impl_->object_);
			IOObjectRelease(iterator);
		}
	} else if (impl_->path_) {
		/* Native mode. Just use the IO Registry pathname */
		impl_->object_ = IORegistryEntryFromPath(kIOMasterPortDefault, impl_->path_);
	}
	/* Strange if (!x) ... if (x) style so you can #ifdef out the !x part */
	if (!impl_->object_) message(-2, "init: no object");
	if (impl_->object_) {
		/* Get intermediate (IOCFPlugIn) plug-in for MMC device */
		err = IOCreatePlugInInterfaceForService(impl_->object_,
		       kIOMMCDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
		       &impl_->plugin_, &score);
		if (err  != noErr)
			message(-2, "init: IOCreatePlugInInterfaceForService failed: %d", err);
	}
	if (!impl_->plugin_) message(-2, "init: no plugin");
	if (impl_->plugin_) {
		/* Get the MMC interface (MMCDeviceInterface) */
		herr = (*impl_->plugin_)->QueryInterface(impl_->plugin_,
		        CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
			/*
			 * Most of Apple's examples erroneously cast to LPVOID,
			 * not LPVOID *.
			 */
		        (LPVOID *)&impl_->mmc_);
		if (herr != S_OK)
			message(-2, "init: QueryInterface failed: %d", herr);
	}
	if (!impl_->mmc_) message(-2, "init: no mmc");
	if (impl_->mmc_) {
		/* Get the SCSI interface */
		impl_->scsi_ = (*impl_->mmc_)->GetSCSITaskDeviceInterface(impl_->mmc_);
	}
	if (!impl_->scsi_) message(-2, "init: no scsi");
	if (impl_->scsi_) {
		/* Obtain exclusive access to device */
		err = (*impl_->scsi_)->ObtainExclusiveAccess(impl_->scsi_);
		if (err != noErr)
			message(-2, "init: ObtainExclusiveAccess failed: %d", err);
		if (err == noErr) {
			impl_->exclusive_ = 1;
			/* Send SCSI inquiry command */
			i = inquiry();
			if (i != 0)
				message(-2, "init: inquiry failed: %d", i);
			return (i == 0) ? 0 : 2;
		}
	}
	message(-2, "init: failed");
	return 1;
}

int ScsiIf::timeout(int t)
{
	int ret = impl_->timeout_/1000;
	impl_->timeout_ = t*1000;
	return ret;
}

int ScsiIf::sendCmd(const unsigned char *cmd, int cmdLen, 
	    const unsigned char *dataOut, int dataOutLen,
	    unsigned char *dataIn, int dataInLen,
	    int showMessage)
{
	SCSITaskInterface **task;
	IOVirtualRange range;
	IOReturn ret;
	UInt64 len;

	if (impl_->error_ != NULL) { delete[] impl_->error_; impl_->error_ = NULL; }

#define ERROR(msg) do {\
	impl_->error_ = new char[9 + strlen(msg) + 1];\
	strcpy(impl_->error_, "sendCmd: ");\
	strcat(impl_->error_, msg);\
	if (showMessage) printError();\
	if (task) (*task)->Release(task);\
	return 1;\
} while(0)

	task = (*impl_->scsi_)->CreateSCSITask(impl_->scsi_);
	if (!task) ERROR("no task");
	ret = (*task)->SetCommandDescriptorBlock(task, (UInt8 *)cmd, cmdLen);
	if (ret != kIOReturnSuccess) ERROR("SetCommandDescriptorBlock failed");
	/* The OSX SCSI interface can't deal with two data phases */
	if (dataIn && dataOut) ERROR("dataIn && dataOut");
	if (dataIn) {
		range.address = (IOVirtualAddress)dataIn;
		range.length = dataInLen;
		ret = (*task)->SetScatterGatherEntries(task, &range, 1,
		      dataInLen, kSCSIDataTransfer_FromTargetToInitiator);
	} else if (dataOut) {
		range.address = (IOVirtualAddress)dataOut;
		range.length = dataOutLen;
		ret = (*task)->SetScatterGatherEntries(task, &range, 1,
		      dataOutLen, kSCSIDataTransfer_FromInitiatorToTarget);
	} else {
		/* Just to make sure. We pass in zero ranges anyway */
		range.address = (IOVirtualAddress)NULL;
		range.length = 0;
		ret = (*task)->SetScatterGatherEntries(task, &range, 0,
		      0, kSCSIDataTransfer_NoDataTransfer);
	}
	if (ret != kIOReturnSuccess) ERROR("SetScatterGatherEntries failed");
	ret = (*task)->SetTimeoutDuration(task, impl_->timeout_);
	if (ret != kIOReturnSuccess) ERROR("SetTimeoutDuration failed");
	ret = (*task)->ExecuteTaskSync(task, &impl_->sense_, &impl_->status_, &len);
	if (ret != kIOReturnSuccess) ERROR("ExecuteTaskSync failed");
	ret = (*task)->GetSCSIServiceResponse(task, &impl_->response_);
	if (ret != kIOReturnSuccess) ERROR("GetSCSIServiceResponse failed");
	(*task)->Release(task);
	if (impl_->response_ == kSCSIServiceResponse_TASK_COMPLETE) {
		if (impl_->status_ == kSCSITaskStatus_GOOD) return 0;
		if (impl_->status_ == kSCSITaskStatus_CHECK_CONDITION) return 2;
	}
	if (impl_->response_ == kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE) return 1;
	return 1 /* XXX This shouldn't happen */;

#undef ERROR
}

const unsigned char *ScsiIf::getSense(int &len) const
{
	len = kSenseDefaultSize;
	return (unsigned char *)&impl_->sense_;
}

void ScsiIf::printError()
{
	char *s;

	if (impl_->error_)
		/* Internal error in sendCmd(). We saved a message string. */
		s = impl_->error_;
	else switch (impl_->response_) {
		case kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE:
		/* The SCSI command didn't complete */
			switch (impl_->status_) {
				case kSCSITaskStatus_TaskTimeoutOccurred:
					s = "task timeout"; break;
				case kSCSITaskStatus_ProtocolTimeoutOccurred:
					s = "protocol timeout"; break;
				case kSCSITaskStatus_DeviceNotResponding:
					s = "device not responding"; break;
				case kSCSITaskStatus_DeviceNotPresent:
					s = "device not present"; break;
				case kSCSITaskStatus_DeliveryFailure:
					s = "delivery failure"; break;
				case kSCSITaskStatus_No_Status:
					s = "no status"; break;
				default: 
					s = "failure, unknown status"; break;
			}
			break;
		case kSCSIServiceResponse_TASK_COMPLETE:
		/* The SCSI command did complete */
			switch (impl_->status_) {
				case kSCSITaskStatus_GOOD:
					s = "good"; break;
				case kSCSITaskStatus_CHECK_CONDITION:
					decodeSense((unsigned char *)&impl_->sense_, sizeof(impl_->sense_));
					s = NULL; break;
				case kSCSITaskStatus_CONDITION_MET:
					s = "condition met"; break;
				case kSCSITaskStatus_BUSY:
					s = "busy"; break;
				case kSCSITaskStatus_INTERMEDIATE:
					s = "intermediate"; break;
				case kSCSITaskStatus_INTERMEDIATE_CONDITION_MET:
					s = "intermediate, condition met"; break;
				case kSCSITaskStatus_RESERVATION_CONFLICT:
					s = "reservation conflict"; break;
				case kSCSITaskStatus_TASK_SET_FULL:
					s = "task set full"; break;
				case kSCSITaskStatus_ACA_ACTIVE:
					s = "aca active"; break;
				default: 
					s = "complete, unknown status"; break;
			}
			break;
		default:
			s = "unknown response"; break;
	}
	if (s) message(-2, s);
}

/*
 * Internal form or inquiry command.
 * Used by both inquiry() and scanData(), but with different data.
 */
int inq(SCSITaskDeviceInterface **scsi,
	SCSIServiceResponse *response,
	SCSITaskStatus *status,
	struct SCSI_Sense_Data *sense,
	char *vend, char *prod, char *rev)
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
		message(-2, "inq: no task");
		return 1;
	}
	bzero(cdb, sizeof(cdb));
	cdb[0] = kSCSICmd_INQUIRY;
	cdb[4] = sizeof(inq_data);
	ret = (*task)->SetCommandDescriptorBlock(task, cdb, kSCSICDBSize_6Byte);
	if (ret != kIOReturnSuccess) {
		message(-2, "inq: SetCommandDescriptorBlock failed: %d", ret);
		(*task)->Release(task);
		return 1;
	}
	range.address = (IOVirtualAddress)&inq_data;
	range.length = sizeof(inq_data);
	ret = (*task)->SetScatterGatherEntries(task, &range, 1,
	      sizeof(inq_data), kSCSIDataTransfer_FromTargetToInitiator);
	if (ret != kIOReturnSuccess) {
		message(-2, "inq: SetScatterGatherEntries failed: %d", ret);
		(*task)->Release(task);
		return 1;
	}
	ret = (*task)->SetTimeoutDuration(task, 1000);
	if (ret != kIOReturnSuccess) {
		message(-2, "inq: SetTimeoutDuration failed: %d", ret);
		(*task)->Release(task);
		return 1;
	}
	ret = (*task)->ExecuteTaskSync(task, sense, status, &len);
	if (ret != kIOReturnSuccess) {
		message(-2, "inq: ExecuteTaskSync failed: %d", ret);
		(*task)->Release(task);
		return 1;
	}
	ret = (*task)->GetSCSIServiceResponse(task, response);
	if (ret != kIOReturnSuccess) {
		message(-2, "inq: GetSCSIServiceResponse failed: %d", ret);
		(*task)->Release(task);
		return 1;
	}
	if (*response != kSCSIServiceResponse_TASK_COMPLETE) {
		message(-2, "inq: response=%d", *response);
		(*task)->Release(task);
		return 1;
	}
	if (*status != kSCSITaskStatus_GOOD) {
		message(-2, "inq: status=%d", *status);
		(*task)->Release(task);
		return 1;
	}
	(*task)->Release(task);
	/* Copy vendor/product/revision stripping traiiling spaces */
	i = kINQUIRY_VENDOR_IDENTIFICATION_Length;
	while (i > 0 && inq_data.VENDOR_IDENTIFICATION[i - 1] == ' ') i--;
	memcpy(vend, inq_data.VENDOR_IDENTIFICATION, i);
	vend[i] = '\0';
	i = kINQUIRY_PRODUCT_IDENTIFICATION_Length;
	while (i > 0 && inq_data.PRODUCT_IDENTIFICATION[i - 1] == ' ') i--;
	memcpy(prod, inq_data.PRODUCT_IDENTIFICATION, i);
	prod[i] = '\0';
	i = kINQUIRY_PRODUCT_REVISION_LEVEL_Length;
	while (i > 0 && inq_data.PRODUCT_REVISION_LEVEL[i - 1] == ' ') i--;
	memcpy(rev, inq_data.PRODUCT_REVISION_LEVEL, i);
	rev[i] = '\0';
	return 0;
}

int ScsiIf::inquiry()
{
	return inq(impl_->scsi_, &impl_->response_, &impl_->status_, &impl_->sense_,
	           vendor_, product_, revision_);
}

#define MAX_SCAN 10

ScsiIf::ScanData *ScsiIf::scan(int *len, char *dev)
{
	ScanData *scanData;
	CFMutableDictionaryRef dict = NULL;
	CFMutableDictionaryRef sub = NULL;
	io_iterator_t iterator = 0;
	io_object_t object = 0;
	IOCFPlugInInterface **plugin = NULL;
	MMCDeviceInterface  **mmc = NULL;
	SCSITaskDeviceInterface **scsi = NULL;
	SCSIServiceResponse response;
	SCSITaskStatus status;
	int exclusive = 0;
	io_string_t path;
	kern_return_t err;
	SInt32 score;
	HRESULT herr;
	int ret;
	int i;
	
	/* Ignore dev. We don't support different kinds of busses that way. */
	/* Build matching dictionaries to find authoring decices. See init(). */
	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
	sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
	CFDictionarySetValue(sub, CFSTR(kIOPropertySCSITaskDeviceCategory),
	                          CFSTR(kIOPropertySCSITaskAuthoringDevice));
	CFDictionarySetValue(dict, CFSTR(kIOPropertyMatchKey), sub);
	IOServiceGetMatchingServices(kIOMasterPortDefault, dict, &iterator);
	if (!iterator) {
		message(-2, "scan: no iterator");
		*len = 0;
		return NULL;
	}
	scanData = new ScanData[MAX_SCAN]; *len = 0;
	for (i = 0; ; i++) {
		object = IOIteratorNext(iterator);
		if (!object) break;
		if (*len == MAX_SCAN) break;
		/* Get native (IO Registry) pathname of this device. */
		err = IORegistryEntryGetPath(object, kIOServicePlane, path);
		if (err == noErr) {
			scanData[*len].dev = strdupCC(path);
		}
		/* See init() for a description of the plugin/interface tour. */
		err = IOCreatePlugInInterfaceForService(object,
		       kIOMMCDeviceUserClientTypeID, kIOCFPlugInInterfaceID,
		       &plugin, &score);
		if (err != noErr) {
			message(-2, "scan: IOCreatePlugInInterfaceForService failed: %d", err);
			goto clean;
		}
		if (!plugin) {
			message(-2, "scan: no plugin");
			goto clean;
		}
		herr = (*plugin)->QueryInterface(plugin,
		        CFUUIDGetUUIDBytes(kIOMMCDeviceInterfaceID),
		        (LPVOID *)&mmc);
		if (herr != S_OK) {
			message(-2, "scan: QueryInterface failed: %d", herr);
			goto clean;
		}
		if (!mmc) {
			message(-2, "scan: no mmc");
			goto clean;
		}
		scsi = (*mmc)->GetSCSITaskDeviceInterface(mmc);
		if (!scsi) {
			message(-2, "scan: no scsi");
			goto clean;
		}
		err = (*scsi)->ObtainExclusiveAccess(scsi);
		if (err != noErr) {
			message(-2, "scan: ObtainExclusiveAccess failed: %d", err);
			goto clean;
		}
		ret = inq(scsi, &response, &status, NULL,
		          scanData[*len].vendor,
		          scanData[*len].product,
		          scanData[*len].revision);
		if (ret != 0) {
			message(-2, "scan: inq failed: %d", ret);
			goto clean;
		}
		(*len)++;
clean:
		if (exclusive) (*scsi)->ReleaseExclusiveAccess(scsi);
		if (scsi) (*scsi)->Release(scsi);
		if (mmc) (*mmc)->Release(mmc);
		if (plugin) IODestroyPlugInInterface(plugin);
		if (object) IOObjectRelease(object);
	}
	IOObjectRelease(iterator);
	return scanData;
}

#include "ScsiIf-common.cc"

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2007 Denis Leroy <denis@poolshark.org>
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

#include <config.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <glob.h>
#include <asm/param.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>

#include "ScsiIf.h"
#include "sg_err.h"
#include "log.h"
#include "util.h"

//
// SG_IO Linux SCSI interface
///

#ifndef SG_GET_RESERVED_SIZE
#define SG_GET_RESERVED_SIZE 0x2272
#endif

#ifndef SG_SET_RESERVED_SIZE
#define SG_SET_RESERVED_SIZE 0x2275
#endif

#ifndef SG_GET_VERSION_NUM
#define SG_GET_VERSION_NUM 0x2282
#endif

#ifndef SG_MAX_SENSE
#define SG_MAX_SENSE 16
#endif

#define CDRDAO_DEFAULT_TIMEOUT 30000

#define SYSFS_SCSI_DEVICES "/sys/bus/scsi/devices"

typedef unsigned char uchar;

class ScsiIfImpl
{
public:
    char* filename_; // user provided device name
    int   fd_;
    bool  readOnlyMode;

    int openScsiDevAsSg(const char* devname);
    int adjustReservedBuffer(int requestedSize);

    uchar sense_buffer[SG_MAX_SENSE];
    uchar sense_buffer_length;

    uchar last_sense_buffer_length;
    uchar last_command_status;

    int timeout_ms;
};


ScsiIf::ScsiIf(const char *dev)
{
    impl_ = new ScsiIfImpl;
    memset(impl_, 0, sizeof(ScsiIfImpl));

    impl_->filename_ = strdupCC(dev);
    impl_->fd_ = -1;
    impl_->sense_buffer_length = SG_MAX_SENSE;
    impl_->timeout_ms = CDRDAO_DEFAULT_TIMEOUT;
}

ScsiIf::~ScsiIf()
{
    if (impl_->fd_ >= 0)
	close(impl_->fd_);

    delete[] impl_->filename_;
    delete impl_;
}

// Opens and flushes scsi device.

int ScsiIf::init()
{
    int flags;
    int sg_version = 0;

    impl_->fd_ = open(impl_->filename_, O_RDWR | O_NONBLOCK | O_EXCL);

    if (impl_->fd_ < 0) {

	if (errno == EACCES) {
	    impl_->fd_ = open(impl_->filename_, O_RDONLY | O_NONBLOCK);

	    if (impl_->fd_ < 0) {
		goto failed;
	    }
	    impl_->readOnlyMode = true;
	    log_message(-1, "No permission to write to SCSI device."
			"Only read commands are supported.");
	} else {
	    goto failed;
	}
    }

    if (ioctl(impl_->fd_, SG_GET_VERSION_NUM, &sg_version) == 0) {
	log_message(3, "Detected SG driver version: %d.%d.%d",
		    sg_version / 10000,
		    (sg_version / 100) % 100, sg_version % 100);
	if (sg_version < 30000) {
	    log_message(-2, "SG interface under 3.0 not supported.");
	    return 1;
	}
    }

    maxDataLen_ = impl_->adjustReservedBuffer(64 * 1024);

    if (inquiry() != 0) {
	return 2;
    }

    return 0;

 failed:
    log_message(-2, "Unable to open SCSI device %s: %s.",
		impl_->filename_, strerror(errno));
    return 1;
}

// Sets given timeout value in seconds and returns old timeout. Return
// the previous timeout value.

int ScsiIf::timeout(int t)
{
    int old = impl_->timeout_ms / 1000;
    impl_->timeout_ms = t * 1000;

    return old;
}

// Sens a scsi command and send/receive data.

int ScsiIf::sendCmd(const uchar *cmd, int cmdLen, const uchar *dataOut,
		    int dataOutLen, uchar *dataIn, int dataInLen, int showMsg)
{
    int status;

    sg_io_hdr_t io_hdr;
    memset(&io_hdr, 0, sizeof(io_hdr));

    // Check SCSI cdb length.
    assert(cmdLen >= 0 && cmdLen <= 16);
    // Can't both input and output data.
    assert(!(dataOut && dataIn));

    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = cmdLen;
    io_hdr.cmdp = (unsigned char*)cmd;
    io_hdr.timeout = impl_->timeout_ms;
    io_hdr.sbp = impl_->sense_buffer;
    io_hdr.mx_sb_len = impl_->sense_buffer_length;
    io_hdr.flags = 1;
    
    if (dataOut) {
	io_hdr.dxferp = (void*)dataOut;
	io_hdr.dxfer_len = dataOutLen;
	io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    } else if (dataIn) {
	io_hdr.dxferp = dataIn;
	io_hdr.dxfer_len = dataInLen;
	io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    }

    log_message(4, "%s: Initiating SCSI command %s%s",
		impl_->filename_, sg_strcommand(cmd[0]),
		sg_strcmdopts(cmd));

    if (ioctl(impl_->fd_, SG_IO, &io_hdr) < 0) {
	int errnosave = errno;
	log_message((showMsg ? -2 : 3), "%s: SCSI command %s (0x%02x) "
		    "failed: %s.", impl_->filename_,
		    sg_strcommand(cmd[0]), cmd[0],
		    strerror(errnosave));
	return 1;
    }

    log_message(4, "%s: SCSI command %s (0x%02x) executed in %u ms, status=%d",
		impl_->filename_, sg_strcommand(cmd[0]),
		cmd[0], io_hdr.duration, io_hdr.status);

    impl_->last_sense_buffer_length = io_hdr.sb_len_wr;
    impl_->last_command_status = io_hdr.status;

    if (io_hdr.status) {
	if (io_hdr.sb_len_wr > 0)
	    return 2;
	else
	    return 1;
    }

    return 0;
}

const uchar *ScsiIf::getSense(int &len) const
{
    len = impl_->last_sense_buffer_length;
    return impl_->sense_buffer;
}

void ScsiIf::printError()
{
    sg_print_sense("\nSCSI command failed", impl_->sense_buffer);
}


int ScsiIf::inquiry()
{
    unsigned char cmd[6] = { INQUIRY, 0, 0, 0, 0x2c, 0 };
    unsigned char result[0x2c];
    int i;

    memset(result, 0, sizeof(result));

    if (sendCmd(cmd, 6, NULL, 0, result, 0x2c, 1) != 0) {
	log_message(-2, "Inquiry command failed on \"%s\"", impl_->filename_);
	return 1;
    }

    strncpy(vendor_, (char *)(result + 0x08), 8);
    vendor_[8] = 0;

    strncpy(product_, (char *)(result + 0x10), 16);
    product_[16] = 0;

    strncpy(revision_, (char *)(result + 0x20), 4);
    revision_[4] = 0;

    // Remove all trailing spaces.
    for (i = 7; i >= 0 && vendor_[i] == ' '; i--) {
	vendor_[i] = 0;
    }
    for (i = 15; i >= 0 && product_[i] == ' '; i--) {
	product_[i] = 0;
    }
    for (i = 3; i >= 0 && revision_[i] == ' '; i--) {
	revision_[i] = 0;
    }
  
    return 0;
}

// Scan implementation uses sysfs to 

ScsiIf::ScanData *ScsiIf::scan(int *len, char* scsi_dev_path)
{
    struct stat st;
    int matches = 0;
    unsigned i;
    ScanData* sdata = NULL;
    char* path = NULL;
    glob_t pglob;

    if (stat(SYSFS_SCSI_DEVICES, &st) != 0) {
	log_message(-2, "Unable to access sysfs filesystem at %s",
		    SYSFS_SCSI_DEVICES);
	goto fail;
    }

    path = (char*)alloca(strlen(SYSFS_SCSI_DEVICES) + 64);
    sprintf(path, "%s/*", SYSFS_SCSI_DEVICES);
    if (glob(path, 0, NULL, &pglob) != 0) {
	log_message(-2, "Unable to glob through sysfs filesystem (%d).",
		    errno);
	goto fail;
    }

    sdata = new ScanData[pglob.gl_pathc];

    for (i = 0; i < pglob.gl_pathc; i++) {
	int type;
	char rbuf[16];
	FILE* f;

	sprintf(path, "%s/type", pglob.gl_pathv[i]);
	f = fopen(path, "r");
	if (!f)
	    continue;
	int ret = fscanf(f, "%d", &type);
	fclose(f);

	if (ret != 1 || type != TYPE_ROM)
	    continue;

	// Now we have a CD-ROM device.
	memset(&sdata[matches].vendor, 0, sizeof(sdata[matches].vendor));
	memset(&sdata[matches].product, 0, sizeof(sdata[matches].product));
	memset(&sdata[matches].revision, 0, sizeof(sdata[matches].revision));

	// Copy vendor data
	sprintf(path, "%s/vendor", pglob.gl_pathv[i]);
	f = fopen(path, "r");
	if (!f)
	    continue;
	if (fread(sdata[matches].vendor, 8, 1, f) != 1) {
	    fclose(f);
	    continue;
	}
	fclose(f);

	// Copy product data
	sprintf(path, "%s/model", pglob.gl_pathv[i]);
	f = fopen(path, "r");
	if (!f)
	    continue;
	if (fread(sdata[matches].product, 16, 1, f) != 1) {
	    fclose(f);
	    continue;
	}
	fclose(f);

	// Copy revision data
	sprintf(path, "%s/rev", pglob.gl_pathv[i]);
	f = fopen(path, "r");
	if (!f)
	    continue;
	if (fread(sdata[matches].revision, 4, 1, f) != 1) {
	    fclose(f);
	    continue;
	}
	fclose(f);

	// figure out the block device
	glob_t bglob;
	char* devname = NULL;
	sprintf(path, "%s/block:*", pglob.gl_pathv[i]);
	if (glob(path, 0, NULL, &bglob) == 0) {

	    if (bglob.gl_pathc != 1) {
		globfree(&bglob);
		continue;
	    }

	    char* match = strrchr(bglob.gl_pathv[0], ':');
	    if (!match) {
		globfree(&bglob);
		continue;
	    }
	    devname = (char*)alloca(strlen(match));
	    strcpy(devname, match+1);
	} else {
	    sprintf(path, "%s/block/*", pglob.gl_pathv[i]);
	    if (glob(path, 0, NULL, &bglob) == 0) {
		
		if (bglob.gl_pathc != 1) {
		    globfree(&bglob);
		    continue;
		}
		char* match = strrchr(bglob.gl_pathv[0], '/');
		devname = (char*)alloca(strlen(match));
		strcpy(devname, match + 1);
	    }
	}

	if (devname) {
	    sdata[matches].dev = "/dev/";
	    sdata[matches].dev += devname;
	    globfree(&bglob);
	} else {
	    continue;
	}

	matches++;

    }
    globfree(&pglob);

    if (matches) {
	*len = matches;
	return sdata;
    }

    delete[] sdata;
 fail:
    *len = 0;
    return NULL;
}

#include "ScsiIf-common.cc"

int ScsiIfImpl::adjustReservedBuffer(int requestedSize)
{
    int maxTransferLength;

    if (ioctl(fd_, SG_SET_RESERVED_SIZE, &requestedSize) < 0) {
	log_message(-2, "SG_SET_RESERVED_SIZE ioctl failed: %s",
		    strerror(errno));
	return 0;
    }
    if (ioctl(fd_, SG_GET_RESERVED_SIZE, &maxTransferLength) < 0) {
	log_message(-2, "SG_GET_RESERVED_SIZE ioctl failed: %s",
		    strerror(errno));
	return 0;
    }

    log_message(4, "SG: Maximum transfer length: %ld", maxTransferLength);

    return maxTransferLength;
}

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

/* SCSI interface implemenation for FreeBSD.
 * Written by Max Khon <fjoe@iclub.nsu.ru>
 */

#include <config.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <camlib.h>
#include <cam/scsi/scsi_message.h>

#include "ScsiIf.h"
#include "log.h"

#define DEF_RETRY_COUNT 1

#include "decodeSense.cc"

class ScsiIfImpl {
public:
	char *			devname;
	struct cam_device *	dev;
	union ccb *		ccb;
	int			timeout;	/* timeout in ms */
};

ScsiIf::ScsiIf(const char *devname)
{
	impl_ = new ScsiIfImpl;
	impl_->devname = strdupCC(devname);
	impl_->dev = NULL;
	impl_->ccb = NULL;
	impl_->timeout = 5000;

	maxDataLen_ = 32 * 1024;
	vendor_[0] = 0;
	product_[0] = 0;
	revision_[0] = 0;
}

ScsiIf::~ScsiIf()
{
	if (impl_->ccb)
		cam_freeccb(impl_->ccb);
	if (impl_->dev) 
		cam_close_device(impl_->dev);
	delete[] impl_->devname;
	delete impl_;
}

// opens scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed
int ScsiIf::init()
{
	if ((impl_->dev = cam_open_device(impl_->devname, O_RDWR)) == NULL) {
		log_message(-2, "%s", cam_errbuf);
		return 1;
	}
  
	impl_->ccb = cam_getccb(impl_->dev);
	if (impl_->ccb == NULL) {
		log_message(-2, "init: error allocating ccb");
		return 1;
	}

	if (inquiry()) 
		return 2;

	return 0;
}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout
int ScsiIf::timeout(int t)
{
	int old = impl_->timeout;
	impl_->timeout = t*1000;
	return old/1000;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)
int ScsiIf::sendCmd(const unsigned char *cmd, int cmdLen, 
		    const unsigned char *dataOut, int dataOutLen,
		    unsigned char *dataIn, int dataInLen,
		    int showMessage)
{
	int		retval;
	int		flags = CAM_DIR_NONE;
	u_int8_t *	data_ptr;
	size_t		data_len;

	bzero(impl_->ccb, sizeof(union ccb));
	bcopy(cmd, &impl_->ccb->csio.cdb_io.cdb_bytes, cmdLen);

	if (dataOut && dataOutLen > 0) {
		data_ptr = (u_int8_t*) dataOut;
		data_len = dataOutLen;
		flags = CAM_DIR_OUT;
	}
	else if (dataIn && dataInLen > 0) {
		data_ptr = dataIn;
		data_len = dataInLen;
		flags = CAM_DIR_IN;
	}

	cam_fill_csio(&impl_->ccb->csio,
		      DEF_RETRY_COUNT,
		      NULL,
		      flags | CAM_DEV_QFRZDIS,
		      MSG_SIMPLE_Q_TAG,
		      data_ptr,
		      data_len,
		      SSD_FULL_SIZE,
		      cmdLen,
		      impl_->timeout);
	if ((retval = cam_send_ccb(impl_->dev, impl_->ccb)) < 0
	||  (impl_->ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		if (retval < 0) {
			log_message(-2, "sendCmd: error sending command");
			return 1;
		}

		if ((impl_->ccb->ccb_h.status & CAM_STATUS_MASK) ==
		    CAM_SCSI_STATUS_ERROR) {
                        if (showMessage)
			        printError();

			return 2;
		}
		return 1;
	}

	return 0;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
	len = impl_->ccb->csio.sense_len;
	return (const unsigned char*) &impl_->ccb->csio.sense_data;
}

void ScsiIf::printError()
{
        decodeSense((const unsigned char*) &impl_->ccb->csio.sense_data,
		    impl_->ccb->csio.sense_len);
}

int ScsiIf::inquiry()
{
	int i;
	struct scsi_inquiry_data inq_data;

	bzero(impl_->ccb, sizeof(union ccb));
	bzero(&inq_data, sizeof(inq_data));

	scsi_inquiry(&impl_->ccb->csio,
		     DEF_RETRY_COUNT,
		     NULL,
		     MSG_SIMPLE_Q_TAG,
		     (u_int8_t*) &inq_data,
		     sizeof(inq_data),
		     0,
		     0,
		     SSD_FULL_SIZE,
		     impl_->timeout);
	impl_->ccb->ccb_h.flags |= CAM_DEV_QFRZDIS;

	if (cam_send_ccb(impl_->dev, impl_->ccb) < 0) {
		if ((impl_->ccb->ccb_h.status & CAM_STATUS_MASK) !=
		    CAM_SCSI_STATUS_ERROR) {
			log_message(-2, "%s", cam_errbuf);
			return 1;
		}

		printError();
		return 1;
	}

	strncpy(vendor_, inq_data.vendor, 8);
	vendor_[8] = 0;

	strncpy(product_, inq_data.product, 16);
	product_[16] = 0;

	strncpy(revision_, inq_data.revision, 4);
	revision_[4] = 0;

	for (i = 7; i >= 0 && vendor_[i] == ' '; i--)
		vendor_[i] = 0;

	for (i = 15; i >= 0 && product_[i] == ' '; i--)
		product_[i] = 0;

	for (i = 3; i >= 0 && revision_[i] == ' '; i--)
		revision_[i] = 0;

	return 0;
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char* scsi_dev_path)
{
  *len = 0;
  return NULL;
}

#include "ScsiIf-common.cc"

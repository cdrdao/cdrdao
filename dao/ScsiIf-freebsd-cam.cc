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
#ifdef __DragonFly__
#include <bus/cam/scsi/scsi_message.h>
#else
#include <cam/scsi/scsi_message.h>
#endif

#include "ScsiIf.h"
#include "log.h"

#define DEF_RETRY_COUNT 1

#include "decodeSense.cc"

class ScsiIfFreeBSD : public ScsiIf
{
  public:
    explicit ScsiIfFreeBSD(const std::string &dev);
    ~ScsiIfFreeBSD() override;

    int init() override;
    int timeout(int) override;
    int sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                int dataInLen, int showMessage = 1) override;
    const u8 *getSense(int &len) const override;
    void printError() override;

  private:
    struct cam_device *camDev_ = NULL;
    union ccb *ccb_ = NULL;
    int timeout_ = 5000; /* timeout in ms */
};

ScsiIfFreeBSD::ScsiIfFreeBSD(const std::string &dev)
{
    dev_ = dev;
    maxDataLen_ = 32 * 1024;
}

ScsiIfFreeBSD::~ScsiIfFreeBSD()
{
    if (ccb_)
        cam_freeccb(ccb_);
    if (camDev_)
        cam_close_device(camDev_);
}

// opens scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed
int ScsiIfFreeBSD::init()
{
    if ((camDev_ = cam_open_device(dev_.c_str(), O_RDWR)) == NULL) {
        log_message(-2, "%s", cam_errbuf);
        return 1;
    }

    ccb_ = cam_getccb(camDev_);
    if (ccb_ == NULL) {
        log_message(-2, "init: error allocating ccb");
        return 1;
    }

    if (inquiry())
        return 2;

    return 0;
}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout
int ScsiIfFreeBSD::timeout(int t)
{
    int old = timeout_;
    timeout_ = t * 1000;
    return old / 1000;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)
int ScsiIfFreeBSD::sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                           int dataInLen, int showMessage)
{
    int retval;
    int flags = CAM_DIR_NONE;
    u_int8_t *data_ptr = NULL;
    size_t data_len = 0;

    memset(ccb_, 0, sizeof(union ccb));
    memmove(&ccb_->csio.cdb_io.cdb_bytes, cmd, cmdLen);

    if (dataOut && dataOutLen > 0) {
        data_ptr = (u_int8_t *)dataOut;
        data_len = dataOutLen;
        flags = CAM_DIR_OUT;
    } else if (dataIn && dataInLen > 0) {
        data_ptr = dataIn;
        data_len = dataInLen;
        flags = CAM_DIR_IN;
    }

    cam_fill_csio(&ccb_->csio, DEF_RETRY_COUNT, NULL, flags | CAM_DEV_QFRZDIS, MSG_SIMPLE_Q_TAG,
                  data_ptr, data_len, SSD_FULL_SIZE, cmdLen, timeout_);
    if ((retval = cam_send_ccb(camDev_, ccb_)) < 0 ||
        (ccb_->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
        if (retval < 0) {
            log_message(-2, "sendCmd: error sending command");
            return 1;
        }

        if ((ccb_->ccb_h.status & CAM_STATUS_MASK) == CAM_SCSI_STATUS_ERROR) {
            if (showMessage)
                printError();

            return 2;
        }
        return 1;
    }

    return 0;
}

const u8 *ScsiIfFreeBSD::getSense(int &len) const
{
    len = ccb_->csio.sense_len;
    return (const u8 *)&ccb_->csio.sense_data;
}

void ScsiIfFreeBSD::printError()
{
    decodeSense((const u8 *)&ccb_->csio.sense_data, ccb_->csio.sense_len);
}

std::shared_ptr<ScsiIf> ScsiIf::create(const std::string &dev)
{
    return std::make_shared<ScsiIfFreeBSD>(dev);
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char *scsi_dev_path)
{
    *len = 0;
    return NULL;
}

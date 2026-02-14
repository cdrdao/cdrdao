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

#include <sys/stat.h>
#include <sys/types.h>

#include <asm/param.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <fstream>

#include "ScsiIf.h"
#include "log.h"
#include "sg_err.h"
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

class ScsiIfLinux : public ScsiIf
{
public:
    ScsiIfLinux(const std::string& dev);
    virtual ~ScsiIfLinux() override;

    int init() override;
    int timeout(int) override;
    void printError() override;
    int sendCmd(const uchar *, int, const uchar *, int,
                uchar *, int, int) override;
    const uchar *getSense(int &len) const override;

    std::string filename_; // user provided device name
    int fd_ = -1;
    bool readOnlyMode;

private:
    int openScsiDevAsSg(const char *devname);
    int adjustReservedBuffer(int requestedSize);

    uchar sense_buffer[SG_MAX_SENSE];
    uchar sense_buffer_length;

    uchar last_sense_buffer_length;
    uchar last_command_status;

    int timeout_ms;
};

ScsiIfLinux::ScsiIfLinux(const std::string& dev)
    : filename_(dev)
{
    sense_buffer_length = SG_MAX_SENSE;
    timeout_ms = CDRDAO_DEFAULT_TIMEOUT;
}

ScsiIfLinux::~ScsiIfLinux()
{
    if (fd_ >= 0)
        close(fd_);
}

// Opens and flushes scsi device.

int ScsiIfLinux::init()
{
    int flags;
    int sg_version = 0;

    fd_ = open(filename_.c_str(), O_RDWR | O_NONBLOCK | O_EXCL);

    if (fd_ < 0) {

        if (errno == EACCES) {
            fd_ = open(filename_.c_str(), O_RDONLY | O_NONBLOCK);

            if (fd_ < 0) {
                goto failed;
            }
            readOnlyMode = true;
            log_message(-1, "No permission to write to SCSI device."
                        "Only read commands are supported.");
        } else {
            goto failed;
        }
    }

    if (ioctl(fd_, SG_GET_VERSION_NUM, &sg_version) == 0) {
        log_message(3, "Detected SG driver version: %d.%d.%d", sg_version / 10000,
                    (sg_version / 100) % 100, sg_version % 100);
        if (sg_version < 30000) {
            log_message(-2, "SG interface under 3.0 not supported.");
            return 1;
        }
    }

    maxDataLen_ = adjustReservedBuffer(64 * 1024);

    if (inquiry() != 0) {
        return 2;
    }

    return 0;

 failed:
    log_message(-2, "Unable to open SCSI device %s: %s.", filename_, strerror(errno));
    return 1;
}

// Sets given timeout value in seconds and returns old timeout. Return
// the previous timeout value.

int ScsiIfLinux::timeout(int t)
{
    int old = timeout_ms / 1000;
    timeout_ms = t * 1000;

    return old;
}

// Sens a scsi command and send/receive data.

int ScsiIfLinux::sendCmd(const uchar *cmd, int cmdLen, const uchar *dataOut, int dataOutLen,
                         uchar *dataIn, int dataInLen, int showMsg)
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
    io_hdr.cmdp = (unsigned char *)cmd;
    io_hdr.timeout = timeout_ms;
    io_hdr.sbp = sense_buffer;
    io_hdr.mx_sb_len = sense_buffer_length;
    io_hdr.flags = 1;

    if (dataOut) {
        io_hdr.dxferp = (void *)dataOut;
        io_hdr.dxfer_len = dataOutLen;
        io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
    } else if (dataIn) {
        io_hdr.dxferp = dataIn;
        io_hdr.dxfer_len = dataInLen;
        io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    }

    log_message(5, "%s: Initiating SCSI command %s%s", filename_.c_str(),
                sg_strcommand(cmd[0]), sg_strcmdopts(cmd));

    if (ioctl(fd_, SG_IO, &io_hdr) < 0) {
        int errnosave = errno;
        log_message((showMsg ? -2 : 3),
                    "[SCSI] %s (0x%02x) "
                    "failed: %s.",
                    sg_strcommand(cmd[0]), cmd[0], strerror(errnosave));
        return 1;
    }

    log_message(4, "[SCSI] %s (0x%02x) executed in %u ms, status=%d", sg_strcommand(cmd[0]), cmd[0],
                io_hdr.duration, io_hdr.status);

    last_sense_buffer_length = io_hdr.sb_len_wr;
    last_command_status = io_hdr.status;

    if (io_hdr.status) {
        if (io_hdr.sb_len_wr > 0)
            return 2;
        else
            return 1;
    }

    return 0;
}

const uchar *ScsiIfLinux::getSense(int &len) const
{
    len = last_sense_buffer_length;
    return sense_buffer;
}

void ScsiIfLinux::printError()
{
    sg_print_sense("\nSCSI command failed", sense_buffer);
}

int ScsiIfLinux::adjustReservedBuffer(int requestedSize)
{
    int maxTransferLength;

    if (ioctl(fd_, SG_SET_RESERVED_SIZE, &requestedSize) < 0) {
        log_message(-2, "SG_SET_RESERVED_SIZE ioctl failed: %s", strerror(errno));
        return 0;
    }
    if (ioctl(fd_, SG_GET_RESERVED_SIZE, &maxTransferLength) < 0) {
        log_message(-2, "SG_GET_RESERVED_SIZE ioctl failed: %s", strerror(errno));
        return 0;
    }

    log_message(4, "SG: Maximum transfer length: %ld", maxTransferLength);

    return maxTransferLength;
}

// ------------------------------------------------
//
// ScsiIf static methods.
//
//

ScsiIf* ScsiIf::create(const std::string& dev)
{
    return new ScsiIfLinux(dev);
}

// Scan implementation uses sysfs to

ScsiIf::ScanData *ScsiIf::scan(int *len, char *scsi_dev_path)
{
    struct stat st;
    int matches = 0;
    unsigned i;
    ScanData *sdata = NULL;
    char *path = NULL;
    glob_t pglob;

    if (stat(SYSFS_SCSI_DEVICES, &st) != 0) {
        log_message(-2, "Unable to access sysfs filesystem at %s", SYSFS_SCSI_DEVICES);
        goto fail;
    }

    path = (char *)alloca(strlen(SYSFS_SCSI_DEVICES) + 64);
    sprintf(path, "%s/*", SYSFS_SCSI_DEVICES);
    if (glob(path, 0, NULL, &pglob) != 0) {
        log_message(-2, "No devices found");
        goto fail;
    }

    sdata = new ScanData[pglob.gl_pathc];

    for (i = 0; i < pglob.gl_pathc; i++) {
        int type;
        char rbuf[16];
        FILE *f;

        sprintf(path, "%s/type", pglob.gl_pathv[i]);
        f = fopen(path, "r");
        if (!f)
            continue;
        int ret = fscanf(f, "%d", &type);
        fclose(f);

        if (ret != 1 || type != TYPE_ROM)
            continue;

        // Copy vendor data
        sprintf(path, "%s/vendor", pglob.gl_pathv[i]);
        {
            std::ifstream f(path);
            if (!f)
                continue;
            sdata[matches].vendor.resize(8);
            f.read(sdata[matches].vendor.data(), 8);
        }
        // Copy product data
        sprintf(path, "%s/model", pglob.gl_pathv[i]);
        {
            std::ifstream f(path);
            if (!f)
                continue;
            sdata[matches].product.resize(16);
            f.read(sdata[matches].product.data(), 16);
        }
        // Copy revision data
        sprintf(path, "%s/rev", pglob.gl_pathv[i]);
        {
            std::ifstream f(path);
            if (!f)
                continue;
            sdata[matches].revision.resize(4);
            f.read(sdata[matches].revision.data(), 4);
        }

        // figure out the block device
        glob_t bglob;
        char *devname = NULL;
        sprintf(path, "%s/block:*", pglob.gl_pathv[i]);
        if (glob(path, 0, NULL, &bglob) == 0) {

            if (bglob.gl_pathc != 1) {
                globfree(&bglob);
                continue;
            }

            char *match = strrchr(bglob.gl_pathv[0], ':');
            if (!match) {
                globfree(&bglob);
                continue;
            }
            devname = (char *)alloca(strlen(match));
            strcpy(devname, match + 1);
        } else {
            sprintf(path, "%s/block/*", pglob.gl_pathv[i]);
            if (glob(path, 0, NULL, &bglob) == 0) {

                if (bglob.gl_pathc != 1) {
                    globfree(&bglob);
                    continue;
                }
                char *match = strrchr(bglob.gl_pathv[0], '/');
                devname = (char *)alloca(strlen(match));
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

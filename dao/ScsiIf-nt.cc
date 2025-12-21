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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ntddscsi.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <windows.h>

#include "ScsiIf.h"
#include "log.h"

#include "decodeSense.cc"

typedef struct {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG Filler; // realign buffer to double word boundary
    UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

typedef struct _SCSI_INQUIRY_DEVICE {
    UCHAR Type;
    UCHAR TypeModifier;
    UCHAR Version;
    UCHAR Format;
    UCHAR AddLength; // n-4
    UCHAR Reserved[2];
    UCHAR Flags;
    char VendorId[8];
    char ProductId[16];
    char ProductRevLevel[4];
    char ProductRevDate[8];
} SCSI_INQUIRY_DEVICE;

//
// SCSI CDB operation codes
//

#define SCSIOP_INQUIRY 0x12
#define SCSIOP_MODE_SELECT 0x15
#define SCSIOP_MODE_SENSE 0x1A

class ScsiIfImpl
{
  public:
    std::string dev_;

    HANDLE hCD;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb;

    unsigned char senseBuffer_[32];

    int timeout_;
};

ScsiIf::ScsiIf(const char *dev)
{
    impl_ = new ScsiIfImpl;

    impl_->dev_ = "\\\\.\\";
    impl_->dev_.append({dev[0]});
    impl_->dev_.append(":");
    impl_->timeout_ = 30;
    impl_->hCD = INVALID_HANDLE_VALUE;

    vendor_[0] = 0;
    product_[0] = 0;
    revision_[0] = 0;
}

ScsiIf::~ScsiIf()
{
    if (impl_->hCD != INVALID_HANDLE_VALUE)
        CloseHandle(impl_->hCD);

    delete impl_;
}

// opens scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed

int ScsiIf::init()
{
    int i = 0;
    DWORD ol;

    SCSI_ADDRESS sa;
    IO_SCSI_CAPABILITIES ca;

    while (i++ < 3 && (impl_->hCD == INVALID_HANDLE_VALUE)) {
        impl_->hCD = CreateFile(impl_->dev_.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }

    if (impl_->hCD == INVALID_HANDLE_VALUE) {
        return 1;
    }

    if (DeviceIoControl(impl_->hCD, IOCTL_SCSI_GET_CAPABILITIES, NULL, 0, &ca,
                        sizeof(IO_SCSI_CAPABILITIES), &ol, NULL)) {
        maxDataLen_ = ca.MaximumTransferLength;
    } else {
        // Pick sensible default, most drives won't honor the GET_CAP
        maxDataLen_ = 64 * 1024;
    }

    if (inquiry() != 0)
        return 2;

    return 0;
}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout

int ScsiIf::timeout(int t)
{
    int old = impl_->timeout_;
    impl_->timeout_ = t;
    return old;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)

int ScsiIf::sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                    int dataInLen, int showMessage)
{
    int i = 10;

    DWORD er, il, ol;

    ZeroMemory(&impl_->sb, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));

    impl_->sb.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    impl_->sb.sptd.PathId = 0;
    impl_->sb.sptd.TargetId = 0; // impl_->scsi_id_;
    impl_->sb.sptd.Lun = 0;      // impl_->lun_;
    impl_->sb.sptd.CdbLength = cmdLen;
    impl_->sb.sptd.SenseInfoLength = 32;
    impl_->sb.sptd.TimeOutValue = impl_->timeout_;
    impl_->sb.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    memcpy(impl_->sb.sptd.Cdb, cmd, cmdLen);

    if (dataOut && dataOutLen) {
        impl_->sb.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
        impl_->sb.sptd.DataBuffer = (void *)dataOut;
        impl_->sb.sptd.DataTransferLength = dataOutLen;
    } else {
        impl_->sb.sptd.DataIn = SCSI_IOCTL_DATA_IN;
        impl_->sb.sptd.DataBuffer = dataIn;
        impl_->sb.sptd.DataTransferLength = dataInLen;
    }

    il = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);

    auto result = DeviceIoControl(impl_->hCD, IOCTL_SCSI_PASS_THROUGH_DIRECT, &impl_->sb, il,
                                  &impl_->sb, il, &ol, NULL);

    if (!result) {
        log_message(-2, "DeviceIoControl failed: %lu\n", GetLastError());
        return 1;
    }

    if (impl_->sb.sptd.ScsiStatus != 0)
        return 2;

    return 0;
}

const unsigned char *ScsiIf::getSense(int &len) const
{
    len = 32;
    return impl_->sb.ucSenseBuf;
}

void ScsiIf::printError()
{
    decodeSense(impl_->sb.ucSenseBuf, 32);
}

int ScsiIf::inquiry()
{
    unsigned char cmd[6];
    int i;

    SCSI_INQUIRY_DEVICE NTinqbuf;

    ZeroMemory(&NTinqbuf, sizeof(SCSI_INQUIRY_DEVICE));

    cmd[0] = 0x12; // INQUIRY
    cmd[1] = cmd[2] = cmd[3] = 0;
    cmd[4] = sizeof(NTinqbuf);
    cmd[5] = 0;

    if (sendCmd(cmd, 6, NULL, 0, (unsigned char *)&NTinqbuf, sizeof(NTinqbuf), 1) != 0) {
        log_message(-2, "Inquiry command failed on '%s'", impl_->dev_.c_str());
        return 1;
    }

    strncpy(vendor_, (char *)(NTinqbuf.VendorId), 8);
    vendor_[8] = 0;

    strncpy(product_, (char *)(NTinqbuf.ProductId), 16);
    product_[16] = 0;

    strncpy(revision_, (char *)(NTinqbuf.ProductRevLevel), 4);
    revision_[4] = 0;

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

#include "ScsiIf-common.cc"

ScsiIf::ScanData *ScsiIf::scan(int *len, char *scsi_dev_path)
{
    ScanData *sdata = NULL;

    DWORD drive_mask = GetLogicalDrives();
    std::vector<std::string> matches;

    if (drive_mask == 0) {
        log_message(0, "Error: Could not retrieve logical drives");
        return NULL;
    }

    for (int i = 0; i < 26; i++) {
        if (drive_mask & (1 << i)) {
            std::string path;
            path.append({(char)('A' + i)});
            path.append(":\\");
            UINT drive_type = GetDriveType(path.c_str());
            if (drive_type == DRIVE_CDROM) {
                matches.push_back(path);
            }
        }
    }

    if (matches.size() > 0) {
        *len = matches.size();
        sdata = new ScanData[matches.size()];

        for (int i = 0; i < matches.size(); i++) {
            ScsiIf sif(matches[i].c_str());
            sif.init();
            sdata[i].dev = matches[i];
            strcpy(sdata[i].vendor, sif.vendor());
            strcpy(sdata[i].product, sif.product());
            strcpy(sdata[i].revision, sif.revision());
        }
    }

    return sdata;
}

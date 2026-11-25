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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <ntddscsi.h>

#include "ScsiIf.h"
#include "log.h"

#include "decodeSense.cc"

typedef struct {
    SCSI_PASS_THROUGH_DIRECT sptd;
    ULONG Filler; // realign buffer to double word boundary
    UCHAR ucSenseBuf[32];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;

class ScsiIfNt : public ScsiIf
{
  public:
    explicit ScsiIfNt(const std::string &dev);
    ~ScsiIfNt() override;

    int init() override;
    int timeout(int) override;
    int sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                int dataInLen, int showMessage = 1) override;
    const u8 *getSense(int &len) const override;
    void printError() override;
    void release() override;

  private:
    bool openHandle();

    std::string devicePath_;
    HANDLE hCD_ = INVALID_HANDLE_VALUE;
    SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb_{};
    int lastSenseLen_ = 0;
    int timeout_ = 30;
    bool initialized_ = false;
};

ScsiIfNt::ScsiIfNt(const std::string &dev)
{
    dev_ = dev;
    if (dev.rfind("\\\\.\\", 0) == 0) {
        devicePath_ = dev;
    } else if (!dev.empty() && isalpha(static_cast<unsigned char>(dev[0]))) {
        devicePath_ =
            "\\\\.\\" +
            std::string(1, static_cast<char>(toupper(static_cast<unsigned char>(dev[0])))) + ":";
    } else {
        devicePath_ = dev;
    }
}

ScsiIfNt::~ScsiIfNt()
{
    release();
}

bool ScsiIfNt::openHandle()
{
    int i = 0;
    while (i++ < 3 && hCD_ == INVALID_HANDLE_VALUE) {
        hCD_ = CreateFileA(devicePath_.c_str(), GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }
    return hCD_ != INVALID_HANDLE_VALUE;
}

// opens scsi device
// return: 0: OK
//         1: device could not be opened
//         2: inquiry failed
int ScsiIfNt::init()
{
    DWORD ol = 0;
    IO_SCSI_CAPABILITIES ca{};

    if (!openHandle()) {
        return 1;
    }

    if (DeviceIoControl(hCD_, IOCTL_SCSI_GET_CAPABILITIES, NULL, 0, &ca,
                        sizeof(IO_SCSI_CAPABILITIES), &ol, NULL)) {
        maxDataLen_ = static_cast<int>(ca.MaximumTransferLength);
    } else {
        // Pick sensible default, most drives won't honor the GET_CAP
        maxDataLen_ = 64 * 1024;
    }

    initialized_ = true;

    if (inquiry() != 0)
        return 2;

    return 0;
}

void ScsiIfNt::release()
{
    if (hCD_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hCD_);
        hCD_ = INVALID_HANDLE_VALUE;
    }
}

// Sets given timeout value in seconds and returns old timeout.
// return: old timeout
int ScsiIfNt::timeout(int t)
{
    int old = timeout_;
    timeout_ = t;
    return old;
}

// sends a scsi command and receives data
// return 0: OK
//        1: scsi command failed (os level, no sense data available)
//        2: scsi command failed (sense data available)
int ScsiIfNt::sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                      int dataInLen, int showMessage)
{
    assert(cmdLen >= 0 && static_cast<size_t>(cmdLen) <= sizeof(sb_.sptd.Cdb));

    if (hCD_ == INVALID_HANDLE_VALUE) {
        if (!initialized_ || !openHandle()) {
            if (showMessage)
                log_message(-2, "SCSI device %s is not open", devicePath_.c_str());
            return 1;
        }
    }

    DWORD il = sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER);
    DWORD ol = 0;

    ZeroMemory(&sb_, sizeof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER));

    sb_.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sb_.sptd.PathId = 0;
    sb_.sptd.TargetId = 0;
    sb_.sptd.Lun = 0;
    sb_.sptd.CdbLength = static_cast<UCHAR>(cmdLen);
    sb_.sptd.SenseInfoLength = sizeof(sb_.ucSenseBuf);
    sb_.sptd.TimeOutValue = timeout_;
    sb_.sptd.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
    memcpy(sb_.sptd.Cdb, cmd, cmdLen);

    if (dataOut && dataOutLen > 0) {
        sb_.sptd.DataIn = SCSI_IOCTL_DATA_OUT;
        sb_.sptd.DataBuffer = const_cast<u8 *>(dataOut);
        sb_.sptd.DataTransferLength = dataOutLen;
    } else if (dataIn && dataInLen > 0) {
        sb_.sptd.DataIn = SCSI_IOCTL_DATA_IN;
        sb_.sptd.DataBuffer = dataIn;
        sb_.sptd.DataTransferLength = dataInLen;
    } else {
        sb_.sptd.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
        sb_.sptd.DataBuffer = NULL;
        sb_.sptd.DataTransferLength = 0;
    }

    BOOL result =
        DeviceIoControl(hCD_, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sb_, il, &sb_, il, &ol, NULL);

    if (!result) {
        if (showMessage)
            log_message(-2, "DeviceIoControl failed: %lu", GetLastError());
        return 1;
    }

    lastSenseLen_ = sb_.sptd.SenseInfoLength;
    if (lastSenseLen_ > static_cast<int>(sizeof(sb_.ucSenseBuf)))
        lastSenseLen_ = static_cast<int>(sizeof(sb_.ucSenseBuf));

    if (sb_.sptd.ScsiStatus != 0) {
        if (showMessage)
            printError();
        return 2;
    }

    return 0;
}

const u8 *ScsiIfNt::getSense(int &len) const
{
    len = lastSenseLen_;
    return sb_.ucSenseBuf;
}

void ScsiIfNt::printError()
{
    decodeSense(sb_.ucSenseBuf, lastSenseLen_);
}

std::shared_ptr<ScsiIf> ScsiIf::create(const std::string &dev)
{
    return std::make_shared<ScsiIfNt>(dev);
}

ScsiIf::ScanData *ScsiIf::scan(int *len, char *scsi_dev_path)
{
    (void)scsi_dev_path;
    *len = 0;

    DWORD drive_mask = GetLogicalDrives();
    if (drive_mask == 0) {
        log_message(0, "Error: Could not retrieve logical drives");
        return NULL;
    }

    std::vector<std::string> matches;
    for (int i = 0; i < 26; i++) {
        if (drive_mask & (1U << i)) {
            std::string path;
            path += static_cast<char>('A' + i);
            path += ":\\";
            UINT drive_type = GetDriveTypeA(path.c_str());
            if (drive_type == DRIVE_CDROM) {
                matches.push_back(path);
            }
        }
    }

    if (matches.empty()) {
        return NULL;
    }

    *len = static_cast<int>(matches.size());
    ScanData *sdata = new ScanData[matches.size()];

    for (size_t i = 0; i < matches.size(); i++) {
        auto sif = ScsiIf::create(matches[i]);
        sif->init();
        sdata[i].dev = matches[i];
        sdata[i].vendor = sif->vendor();
        sdata[i].product = sif->product();
        sdata[i].revision = sif->revision();
    }

    return sdata;
}

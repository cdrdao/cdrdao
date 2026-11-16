/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2026  Denis Leroy
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

// \file DeviceMonitorNetBSD.cc
//   \brief NetBSD/OpenBSD CD-ROM device monitor.
//
// Modelled on dao/ScsiIf-netbsd.cc: the raw CD-ROM device nodes in /dev
// (rcd<unit><rawpartition>, e.g. rcd0c) are opened, confirmed to be SCSI
// devices with SCIOCIDENTIFY, and queried with a SCSI INQUIRY for their
// vendor/model/revision. Plug/unplug is detected by re-scanning on a fixed
// interval in the worker thread.

#include "DeviceMonitor.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/scsiio.h>
#include <sys/types.h>
#include <unistd.h>

// getrawpartition() is declared in the system <util.h>, but the gcdmaster
// build adds -I../trackdb which also contains a util.h; include the system
// header by absolute path to avoid the clash, exactly as
// dao/ScsiIf-netbsd.cc does.
#include "/usr/include/util.h"

namespace
{

constexpr int POLL_INTERVAL_MS = 2000;

// Issue a SCSI INQUIRY on an open raw device and extract the trimmed
// vendor/product/revision strings. Returns false on failure.
bool inquiry(int fd, std::string& vendor, std::string& product, std::string& revision)
{
    char buf[44];
    struct scsireq screq = {/* flags */ SCCMD_READ,
                            /* timeout */ 1000,
                            /* cmd */ {0x12, 0, 0, 0, sizeof(buf), 0},
                            /* cmdlen */ 6,
                            /* databuf */ (caddr_t)&buf,
                            /* datalen */ sizeof(buf),
                            /* datalen_used */ 0,
                            /* sense */ {},
                            /* senselen */ SENSEBUFLEN,
                            /* senselen_used */ 0,
                            /* status */ 0,
                            /* retsts */ 0,
                            /* error */ 0};

    if (ioctl(fd, SCIOCCOMMAND, &screq) < 0 || screq.status != 0 || screq.retsts != SCCMD_OK)
        return false;

    // The INQUIRY data carries space-padded fixed-width identifier fields.
    auto field = [&](int from, int to) {
        while (to > from && buf[to - 1] == ' ')
            to--;
        return std::string(buf + from, to - from);
    };
    vendor = field(8, 16);
    product = field(16, 32);
    revision = field(32, 36);
    return true;
}

class NetBSDDeviceMonitor : public DeviceMonitor
{
  protected:
    void scanLoop() override
    {
        do {
            publish(enumerate());
        } while (!sleep(POLL_INTERVAL_MS));
    }

  private:
    std::vector<DeviceInfo> enumerate()
    {
        std::vector<DeviceInfo> result;

        DIR* dirp = opendir("/dev");
        if (!dirp)
            return result;

        // Raw CD-ROM nodes end with the raw-partition letter, e.g. rcd0c.
        const char rawPart = 'a' + getrawpartition();

        struct dirent* dp;
        while ((dp = readdir(dirp)) != NULL) {
            int l = strlen(dp->d_name);
            if (l <= 3 || strncmp(dp->d_name, "rcd", 3) != 0 ||
                !isdigit((unsigned char)dp->d_name[l - 2]) || dp->d_name[l - 1] != rawPart)
                continue;

            std::string node = std::string("/dev/") + dp->d_name;

            // O_NONBLOCK so an empty/locked drive can't stall the poll loop.
            int fd = open(node.c_str(), O_RDWR | O_NONBLOCK, 0);
            if (fd < 0)
                continue;

            struct scsi_addr saddr;
            DeviceInfo info;
            info.node = node;
            if (ioctl(fd, SCIOCIDENTIFY, &saddr) >= 0 &&
                inquiry(fd, info.vendor, info.product, info.revision)) {
                // Only report devices that identify themselves.
                if (!(info.vendor.empty() && info.product.empty()))
                    result.push_back(std::move(info));
            }
            close(fd);
        }

        closedir(dirp);
        return result;
    }
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<NetBSDDeviceMonitor>();
}

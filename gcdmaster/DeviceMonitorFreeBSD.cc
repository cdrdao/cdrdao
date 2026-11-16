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

// \file DeviceMonitorFreeBSD.cc
//   \brief FreeBSD/DragonFly CD-ROM device monitor.
//
// dao/ScsiIf-freebsd-cam.cc leaves scan() unimplemented, so this backend
// enumerates devices the way camcontrol(8) does: a XPT_DEV_MATCH transaction
// on /dev/xpt0 walks the CAM topology, yielding each device's INQUIRY data
// and the peripheral drivers attached to it. The "cd" peripheral attaches
// only to CD-ROM drives, so a cd<unit> peripheral combined with its device's
// vendor/model gives us exactly the optical recorders. Plug/unplug is
// detected by re-running the match on a fixed interval in the worker thread.

#include "DeviceMonitor.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <camlib.h>
#ifdef __DragonFly__
#include <bus/cam/scsi/scsi_all.h>
#else
#include <cam/scsi/scsi_all.h>
#endif

namespace
{

constexpr int POLL_INTERVAL_MS = 2000;
constexpr int MATCH_BATCH = 100;

// Copy a space/NUL-padded fixed-width INQUIRY field into a trimmed string.
std::string fixedField(const char* p, size_t n)
{
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\0'))
        n--;
    return std::string(p, n);
}

class FreeBSDDeviceMonitor : public DeviceMonitor
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

        int fd = open(XPT_DEVICE, O_RDWR);
        if (fd < 0)
            return result;

        auto* matches =
            (struct dev_match_result*)calloc(MATCH_BATCH, sizeof(struct dev_match_result));
        if (!matches) {
            close(fd);
            return result;
        }

        union ccb ccb;
        memset(&ccb, 0, sizeof(ccb));
        ccb.ccb_h.func_code = XPT_DEV_MATCH;
        ccb.cdm.match_buf_len = MATCH_BATCH * sizeof(struct dev_match_result);
        ccb.cdm.matches = matches;
        ccb.cdm.num_matches = 0;
        ccb.cdm.num_patterns = 0;
        ccb.cdm.pattern_buf_len = 0;

        // INQUIRY data for the device currently being walked; the periph
        // results that follow it in the match stream refer to this device.
        std::string vendor, product, revision;
        bool haveDevice = false;

        do {
            if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1)
                break;
            if (ccb.ccb_h.status != CAM_REQ_CMP ||
                (ccb.cdm.status != CAM_DEV_MATCH_LAST && ccb.cdm.status != CAM_DEV_MATCH_MORE))
                break;

            for (u_int i = 0; i < ccb.cdm.num_matches; i++) {
                const struct dev_match_result& m = ccb.cdm.matches[i];

                if (m.type == DEV_MATCH_DEVICE) {
                    const struct scsi_inquiry_data& inq = m.result.device_result.inq_data;
                    vendor = fixedField(inq.vendor, sizeof(inq.vendor));
                    product = fixedField(inq.product, sizeof(inq.product));
                    revision = fixedField(inq.revision, sizeof(inq.revision));
                    haveDevice = true;

                } else if (m.type == DEV_MATCH_PERIPH && haveDevice) {
                    const struct periph_match_result& p = m.result.periph_result;

                    // The "cd" peripheral driver attaches only to CD-ROMs.
                    if (strcmp(p.periph_name, "cd") != 0)
                        continue;
                    // Only report devices that identify themselves.
                    if (vendor.empty() && product.empty())
                        continue;

                    DeviceInfo info;
                    info.node = "/dev/" + std::string(p.periph_name) +
                                std::to_string(p.unit_number);
                    info.vendor = vendor;
                    info.product = product;
                    info.revision = revision;
                    result.push_back(std::move(info));
                }
            }
        } while (ccb.ccb_h.status == CAM_REQ_CMP && ccb.cdm.status == CAM_DEV_MATCH_MORE);

        free(matches);
        close(fd);
        return result;
    }
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<FreeBSDDeviceMonitor>();
}

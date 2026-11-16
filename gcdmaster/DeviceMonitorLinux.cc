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

// \file DeviceMonitorLinux.cc
//   \brief Linux CD-ROM device monitor.
//
// CD-ROM devices are discovered by walking the SCSI device tree exported
// by the kernel at /sys/bus/scsi/devices (this covers ATAPI, USB and real
// SCSI drives, all of which the kernel presents as SCSI). Each device of
// peripheral type "ROM" (5) that exposes vendor/model identification is
// reported. Plug/unplug is detected by re-scanning the tree on a fixed
// interval; the poll runs in the worker thread so the cost is hidden from
// the GUI.

#include "DeviceMonitor.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{

// Root of the kernel SCSI device tree.
const char* SYSFS_SCSI_DEVICES = "/sys/bus/scsi/devices";

// SCSI peripheral device type for CD/DVD drives.
constexpr int SCSI_TYPE_ROM = 5;

// Interval between device tree scans, in milliseconds.
constexpr int POLL_INTERVAL_MS = 2000;

// Read a single-line sysfs attribute and strip leading/trailing
// whitespace (sysfs strings are space-padded and newline-terminated).
std::string readAttr(const fs::path& path)
{
    std::ifstream f(path);
    if (!f)
        return std::string();

    std::string value;
    std::getline(f, value);

    auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

// Resolve the block device node (e.g. "/dev/sr0") backing the SCSI device
// rooted at 'devDir'. Returns an empty string if none can be determined.
std::string resolveNode(const fs::path& devDir)
{
    std::error_code ec;

    // Modern kernels: <devDir>/block/<name>
    fs::path blockDir = devDir / "block";
    if (fs::is_directory(blockDir, ec)) {
        for (const auto& entry : fs::directory_iterator(blockDir, ec)) {
            return "/dev/" + entry.path().filename().string();
        }
    }

    // Older kernels: <devDir>/block:<name>
    for (const auto& entry : fs::directory_iterator(devDir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("block:", 0) == 0)
            return "/dev/" + name.substr(6);
    }

    return std::string();
}

class LinuxDeviceMonitor : public DeviceMonitor
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
        std::error_code ec;

        fs::directory_iterator it(SYSFS_SCSI_DEVICES, ec);
        if (ec)
            return result;

        for (const auto& entry : it) {
            const fs::path devDir = entry.path();

            if (readAttr(devDir / "type") != std::to_string(SCSI_TYPE_ROM))
                continue;

            DeviceInfo info;
            info.vendor = readAttr(devDir / "vendor");
            info.product = readAttr(devDir / "model");
            info.revision = readAttr(devDir / "rev");

            // Only report devices that identify themselves.
            if (info.vendor.empty() && info.product.empty())
                continue;

            info.node = resolveNode(devDir);
            if (info.node.empty())
                continue;

            result.push_back(std::move(info));
        }

        return result;
    }
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<LinuxDeviceMonitor>();
}

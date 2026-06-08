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

// \file DeviceMonitorWindows.cc
//   \brief Windows CD-ROM device monitor.
//
// Windows identifies optical recorders by drive letter. The set of
// CD-ROM drives is enumerated with GetLogicalDrives()/GetDriveType() and
// each drive's vendor/model is read from its storage device descriptor
// (IOCTL_STORAGE_QUERY_PROPERTY), which works whether or not a disc is
// loaded. Plug/unplug is detected by re-scanning on a fixed interval in
// the worker thread, mirroring the Linux backend.

#include "DeviceMonitor.h"

#include <string>
#include <vector>

#include <windows.h>
#include <winioctl.h>

namespace
{

// Interval between drive scans, in milliseconds.
constexpr int POLL_INTERVAL_MS = 2000;

std::string trim(const std::string& s)
{
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Read vendor/model/revision for the drive with the given letter from its
// storage device descriptor. Returns false if the drive cannot be queried.
bool queryDevice(char letter, DeviceMonitor::DeviceInfo& info)
{
    std::string path = std::string("\\\\.\\") + letter + ":";

    // No access rights are needed for a metadata query.
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE buffer[1024] = {};
    DWORD returned = 0;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer,
                              sizeof(buffer), &returned, NULL);
    CloseHandle(h);
    if (!ok)
        return false;

    auto desc = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(buffer);

    // Offsets are byte positions into the returned buffer, or 0 if absent.
    auto field = [&](DWORD offset) -> std::string {
        if (offset == 0 || offset >= returned)
            return std::string();
        return trim(reinterpret_cast<const char*>(buffer) + offset);
    };

    info.vendor = field(desc->VendorIdOffset);
    info.product = field(desc->ProductIdOffset);
    info.revision = field(desc->ProductRevisionOffset);
    return true;
}

class WindowsDeviceMonitor : public DeviceMonitor
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

        DWORD mask = GetLogicalDrives();
        for (int i = 0; i < 26; i++) {
            if (!(mask & (1u << i)))
                continue;

            char letter = static_cast<char>('A' + i);
            std::string root = std::string(1, letter) + ":\\";
            if (GetDriveTypeA(root.c_str()) != DRIVE_CDROM)
                continue;

            DeviceInfo info;
            info.node = root;
            if (!queryDevice(letter, info))
                continue;

            // Only report devices that identify themselves.
            if (info.vendor.empty() && info.product.empty())
                continue;

            result.push_back(std::move(info));
        }

        return result;
    }
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<WindowsDeviceMonitor>();
}

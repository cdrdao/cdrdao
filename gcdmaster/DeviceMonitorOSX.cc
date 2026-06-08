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

// \file DeviceMonitorOSX.cc
//   \brief macOS CD-ROM device monitor, built on Disk Arbitration.
//
// A DASession is scheduled on the worker thread's run loop and disk
// appeared/disappeared callbacks maintain the set of optical devices.
// Disk Arbitration reports *media* rather than empty drives, so a CD-ROM
// drive shows up once a disc is inserted and disappears when it is
// ejected -- which is exactly the plug/unplug semantics the GUI needs.
//
// Only whole optical media that expose vendor/model identification are
// reported. The callbacks all run on the worker thread, so the device map
// needs no extra locking; publish() handles the thread-safe hand-off to
// the main GTK thread.

#include "DeviceMonitor.h"

#include <cstring>
#include <map>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

namespace
{

// How long each run-loop pass blocks before stop() is re-checked.
constexpr double RUNLOOP_INTERVAL_S = 0.25;

std::string trim(const std::string& s)
{
    auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return std::string();
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

// Fetch a CFString value from a disk description dictionary as UTF-8.
std::string dictString(CFDictionaryRef desc, CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(desc, key);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
        return std::string();

    CFStringRef str = static_cast<CFStringRef>(value);
    CFIndex max =
        CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
    std::string out(max, '\0');
    if (!CFStringGetCString(str, out.data(), max, kCFStringEncodingUTF8))
        return std::string();
    out.resize(std::strlen(out.c_str()));
    return out;
}

bool dictBool(CFDictionaryRef desc, CFStringRef key)
{
    CFTypeRef value = CFDictionaryGetValue(desc, key);
    if (!value || CFGetTypeID(value) != CFBooleanGetTypeID())
        return false;
    return CFBooleanGetValue(static_cast<CFBooleanRef>(value));
}

// Decide whether a disk is optical from its Disk Arbitration description.
// The media kind is the IOKit media class ("IOCDMedia", "IODVDMedia",
// "IOBDMedia", ...) and the media type is a human string ("CD-ROM",
// "DVD-R", ...); either is enough to recognise optical media.
bool isOpticalMedia(const std::string& mediaKind, const std::string& mediaType)
{
    const std::string both = mediaKind + " " + mediaType;
    return both.find("CD") != std::string::npos || both.find("DVD") != std::string::npos ||
           both.find("BD") != std::string::npos;
}

class OSXDeviceMonitor : public DeviceMonitor
{
  protected:
    void scanLoop() override
    {
        session_ = DASessionCreate(kCFAllocatorDefault);
        if (!session_)
            return;

        DASessionScheduleWithRunLoop(session_, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        DARegisterDiskAppearedCallback(session_, NULL, &OSXDeviceMonitor::diskAppeared, this);
        DARegisterDiskDisappearedCallback(session_, NULL, &OSXDeviceMonitor::diskDisappeared,
                                          this);

        // Disk Arbitration delivers a callback for every disk already
        // present right after registration, so the initial set is built
        // without an explicit scan. The bounded run-loop interval keeps
        // stop() responsive.
        while (!stopRequested())
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, RUNLOOP_INTERVAL_S, false);

        DAUnregisterCallback(session_, (void*)&OSXDeviceMonitor::diskAppeared, this);
        DAUnregisterCallback(session_, (void*)&OSXDeviceMonitor::diskDisappeared, this);
        DASessionUnscheduleFromRunLoop(session_, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        CFRelease(session_);
        session_ = nullptr;
    }

  private:
    static void diskAppeared(DADiskRef disk, void* context)
    {
        static_cast<OSXDeviceMonitor*>(context)->onAppeared(disk);
    }

    static void diskDisappeared(DADiskRef disk, void* context)
    {
        static_cast<OSXDeviceMonitor*>(context)->onDisappeared(disk);
    }

    void onAppeared(DADiskRef disk)
    {
        const char* bsdName = DADiskGetBSDName(disk);
        if (!bsdName)
            return;

        CFDictionaryRef desc = DADiskCopyDescription(disk);
        if (!desc)
            return;

        bool whole = dictBool(desc, kDADiskDescriptionMediaWholeKey);
        std::string mediaKind = dictString(desc, kDADiskDescriptionMediaKindKey);
        std::string mediaType = dictString(desc, kDADiskDescriptionMediaTypeKey);

        DeviceInfo info;
        info.node = std::string("/dev/") + bsdName;
        info.vendor = trim(dictString(desc, kDADiskDescriptionDeviceVendorKey));
        info.product = trim(dictString(desc, kDADiskDescriptionDeviceModelKey));
        info.revision = trim(dictString(desc, kDADiskDescriptionDeviceRevisionKey));
        CFRelease(desc);

        if (!whole || !isOpticalMedia(mediaKind, mediaType))
            return;
        if (info.vendor.empty() && info.product.empty())
            return;

        devmap_[info.node] = std::move(info);
        publishMap();
    }

    void onDisappeared(DADiskRef disk)
    {
        const char* bsdName = DADiskGetBSDName(disk);
        if (!bsdName)
            return;

        if (devmap_.erase(std::string("/dev/") + bsdName) > 0)
            publishMap();
    }

    void publishMap()
    {
        std::vector<DeviceInfo> devices;
        devices.reserve(devmap_.size());
        for (const auto& entry : devmap_)
            devices.push_back(entry.second);
        publish(std::move(devices));
    }

    DASessionRef session_ = nullptr;
    std::map<std::string, DeviceInfo> devmap_; // worker-thread only
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<OSXDeviceMonitor>();
}

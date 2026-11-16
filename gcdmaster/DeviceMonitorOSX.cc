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
//   \brief macOS CD-ROM device monitor, built on IOKit notifications.
//
// The monitor watches for the drives themselves (SCSI authoring devices,
// the same IOKit match ScsiIf-osx uses), not for the media in them, so a
// drive stays listed whether or not it holds a disc. Whenever a drive is
// attached or detached, the device list is rebuilt with ScsiIf::scan().
// Drives are reported by their stable name (serial number, or IOService
// path), the same name CdDevice::scan() uses.
//
// A drive whose disc is mounted is listed but flagged as in use, and its
// serial number can't be read until it is free. A Disk Arbitration
// "disappeared" callback triggers a rescan so that is picked up once the
// disc goes away.
//
// All callbacks run on the worker thread's run loop; publish() handles
// the thread-safe hand-off to the main GTK thread.

#include "DeviceMonitor.h"

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/scsi/SCSITaskLib.h>

#include "ScsiIf.h"

namespace
{

// How long each run-loop pass blocks before stop() is re-checked.
constexpr double RUNLOOP_INTERVAL_S = 0.25;

// Matching dictionary for CD/DVD authoring devices. Each call returns a
// new dictionary, since IOServiceAddMatchingNotification consumes one.
CFMutableDictionaryRef authoringDeviceMatch()
{
    auto dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks);
    auto sub = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                         &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(sub, CFSTR(kIOPropertySCSITaskDeviceCategory),
                         CFSTR(kIOPropertySCSITaskAuthoringDevice));
    CFDictionarySetValue(dict, CFSTR(kIOPropertyMatchKey), sub);
    CFRelease(sub);
    return dict;
}

// Consume all entries of a notification iterator, which (re)arms it.
void drain(io_iterator_t it)
{
    while (io_object_t obj = IOIteratorNext(it))
        IOObjectRelease(obj);
}

class OSXDeviceMonitor : public DeviceMonitor
{
  protected:
    void scanLoop() override
    {
        CFRunLoopRef runLoop = CFRunLoopGetCurrent();

        port_ = IONotificationPortCreate(kIOMainPortDefault);
        if (port_) {
            CFRunLoopAddSource(runLoop, IONotificationPortGetRunLoopSource(port_),
                               kCFRunLoopDefaultMode);
            if (IOServiceAddMatchingNotification(port_, kIOFirstMatchNotification,
                                                 authoringDeviceMatch(),
                                                 &OSXDeviceMonitor::ioChanged, this,
                                                 &addedIter_) == KERN_SUCCESS)
                drain(addedIter_);
            if (IOServiceAddMatchingNotification(port_, kIOTerminatedNotification,
                                                 authoringDeviceMatch(),
                                                 &OSXDeviceMonitor::ioChanged, this,
                                                 &removedIter_) == KERN_SUCCESS)
                drain(removedIter_);
        }

        session_ = DASessionCreate(kCFAllocatorDefault);
        if (session_) {
            DASessionScheduleWithRunLoop(session_, runLoop, kCFRunLoopDefaultMode);
            DARegisterDiskDisappearedCallback(session_, NULL, &OSXDeviceMonitor::diskDisappeared,
                                              this);
        }

        // Initial list.
        rescan();

        while (!stopRequested()) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, RUNLOOP_INTERVAL_S, false);
            if (rescanPending_) {
                rescanPending_ = false;
                rescan();
            }
        }

        if (session_) {
            DAUnregisterCallback(session_, (void*)&OSXDeviceMonitor::diskDisappeared, this);
            DASessionUnscheduleFromRunLoop(session_, runLoop, kCFRunLoopDefaultMode);
            CFRelease(session_);
            session_ = nullptr;
        }
        if (addedIter_) {
            IOObjectRelease(addedIter_);
            addedIter_ = 0;
        }
        if (removedIter_) {
            IOObjectRelease(removedIter_);
            removedIter_ = 0;
        }
        if (port_) {
            CFRunLoopRemoveSource(runLoop, IONotificationPortGetRunLoopSource(port_),
                                  kCFRunLoopDefaultMode);
            IONotificationPortDestroy(port_);
            port_ = nullptr;
        }
    }

  private:
    static void ioChanged(void* context, io_iterator_t it)
    {
        drain(it);
        static_cast<OSXDeviceMonitor*>(context)->rescanPending_ = true;
    }

    static void diskDisappeared(DADiskRef, void* context)
    {
        static_cast<OSXDeviceMonitor*>(context)->rescanPending_ = true;
    }

    // Rebuild the device list from a fresh SCSI scan. Callbacks only flag
    // a rescan, so a burst of events (a disc with many partitions going
    // away) results in a single scan per run-loop pass.
    void rescan()
    {
        int len = 0;
        ScsiIf::ScanData* sdata = ScsiIf::scan(&len);

        std::vector<DeviceInfo> devices;
        for (int i = 0; sdata && i < len; i++) {
            if (sdata[i].vendor.empty() && sdata[i].product.empty())
                continue;
            DeviceInfo info;
            // The stable name (serial number or IOService path), not the
            // drive number, which can change when drives come and go.
            info.node = sdata[i].id.empty() ? sdata[i].dev : sdata[i].id;
            info.vendor = sdata[i].vendor;
            info.product = sdata[i].product;
            info.revision = sdata[i].revision;
            devices.push_back(std::move(info));
        }
        delete[] sdata;

        publish(std::move(devices));
    }

    IONotificationPortRef port_ = nullptr;
    io_iterator_t addedIter_ = 0;
    io_iterator_t removedIter_ = 0;
    DASessionRef session_ = nullptr;
    bool rescanPending_ = false; // worker-thread only
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<OSXDeviceMonitor>();
}

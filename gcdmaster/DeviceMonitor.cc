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

#include "DeviceMonitor.h"

#include <algorithm>
#include <chrono>

// The Glib::Dispatcher must be constructed on the thread that runs the
// main loop (it attaches to the default main context), which is why it is
// created and connected here rather than in the worker thread.
DeviceMonitor::DeviceMonitor() : stop_(false), running_(false)
{
    dispatcher_.connect(sigc::mem_fun(*this, &DeviceMonitor::onDispatch));
}

DeviceMonitor::~DeviceMonitor()
{
    stop();
}

void DeviceMonitor::start()
{
    if (running_)
        return;

    {
        std::lock_guard<std::mutex> lock(stopMutex_);
        stop_ = false;
    }
    running_ = true;
    thread_ = std::thread(&DeviceMonitor::workerEntry, this);
}

void DeviceMonitor::stop()
{
    if (!running_)
        return;

    {
        std::lock_guard<std::mutex> lock(stopMutex_);
        stop_ = true;
    }
    stopCond_.notify_all();

    if (thread_.joinable())
        thread_.join();

    running_ = false;
}

std::vector<DeviceMonitor::DeviceInfo> DeviceMonitor::devices() const
{
    std::lock_guard<std::mutex> lock(devicesMutex_);
    return devices_;
}

void DeviceMonitor::workerEntry()
{
    scanLoop();
}

void DeviceMonitor::publish(std::vector<DeviceInfo> devices)
{
    // Normalize ordering so that a mere reordering by the backend does not
    // register as a change.
    std::sort(devices.begin(), devices.end(),
              [](const DeviceInfo& a, const DeviceInfo& b) { return a.node < b.node; });

    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        if (devices == devices_)
            return;
        devices_ = std::move(devices);
    }

    // Wake the main thread; onDispatch() will emit signal_changed() there.
    dispatcher_.emit();
}

bool DeviceMonitor::sleep(int ms)
{
    std::unique_lock<std::mutex> lock(stopMutex_);
    stopCond_.wait_for(lock, std::chrono::milliseconds(ms), [this] { return stop_; });
    return stop_;
}

bool DeviceMonitor::stopRequested() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(stopMutex_));
    return stop_;
}

void DeviceMonitor::onDispatch()
{
    signalChanged_.emit();
}

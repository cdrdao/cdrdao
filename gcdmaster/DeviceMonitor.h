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

// \file DeviceMonitor.h
//   \brief Background monitor for available CD-ROM devices.

#ifndef __DEVICE_MONITOR_H__
#define __DEVICE_MONITOR_H__

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <glibmm/dispatcher.h>
#include <sigc++/signal.h>
#include <sigc++/trackable.h>

// Watches the system for CD-ROM devices appearing and disappearing.
//
// Scanning for devices can block (it touches the kernel device tree and,
// on some platforms, the hardware), so the actual probing runs in a
// dedicated worker thread started by start(). Whenever the set of
// available devices changes, the monitor notifies listeners through
// signal_changed(), which is always emitted on the main GTK thread (via a
// Glib::Dispatcher) so that handlers may safely touch widgets.
//
// Only devices that expose vendor/model identification are reported;
// anonymous nodes are filtered out by the platform backend.
//
// This is an abstract base class. Obtain the platform-specific instance
// through create(). The threading, change notification and locking are
// all handled here; a backend only implements scanLoop().
class DeviceMonitor : public sigc::trackable
{
  public:
    // Identification of a single CD-ROM device. All strings are trimmed
    // of trailing padding; 'node' is the path used to open the device
    // (e.g. "/dev/sr0" on Linux).
    struct DeviceInfo {
        std::string node;
        std::string vendor;
        std::string product;
        std::string revision;

        bool operator==(const DeviceInfo& o) const
        {
            return node == o.node && vendor == o.vendor &&
                   product == o.product && revision == o.revision;
        }
        bool operator!=(const DeviceInfo& o) const { return !(*this == o); }
    };

    // Create the monitor implementation for the current platform. The
    // returned monitor is not yet running; call start() to begin
    // watching.
    static std::unique_ptr<DeviceMonitor> create();

    virtual ~DeviceMonitor();

    // Begin watching for devices in the background. The worker thread
    // performs an initial scan immediately, so signal_changed() will
    // fire shortly after start() if any devices are present. Calling
    // start() on an already-running monitor has no effect.
    void start();

    // Stop the worker thread and block until it has finished. Safe to
    // call multiple times and from the destructor.
    void stop();

    // Return a snapshot of the currently available devices. Thread-safe:
    // may be called from any thread at any time, including from within a
    // signal_changed() handler.
    std::vector<DeviceInfo> devices() const;

    // Emitted on the main thread whenever the device list changes.
    sigc::signal<void()>& signal_changed() { return signalChanged_; }

  protected:
    DeviceMonitor();

    // Backend entry point, executed on the worker thread. Implementations
    // loop until stopRequested() returns true, calling publish() with a
    // fresh device list whenever they detect (or poll) a change. Use
    // sleep() to wait between polls so that stop() can interrupt promptly.
    virtual void scanLoop() = 0;

    // Replace the known device list. If the new list differs from the
    // current one, the change is recorded and signal_changed() is
    // scheduled on the main thread. Called from the worker thread.
    void publish(std::vector<DeviceInfo> devices);

    // Sleep for up to 'ms' milliseconds, returning early if stop() is
    // requested. Returns true if the monitor should stop. Called from the
    // worker thread.
    bool sleep(int ms);

    // Whether stop() has been requested. Called from the worker thread.
    bool stopRequested() const;

  private:
    void workerEntry();
    void onDispatch(); // runs on the main thread

    mutable std::mutex devicesMutex_;
    std::vector<DeviceInfo> devices_;

    std::mutex stopMutex_;
    std::condition_variable stopCond_;
    bool stop_;
    bool running_;

    std::thread thread_;
    Glib::Dispatcher dispatcher_;
    sigc::signal<void()> signalChanged_;
};

#endif

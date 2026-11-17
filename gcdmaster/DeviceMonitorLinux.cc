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
// reported.
//
// Plug/unplug is detected by listening to kernel uevents on a
// NETLINK_KOBJECT_UEVENT socket (the same source udev uses, readable by
// unprivileged processes), and rescanning sysfs whenever a block or SCSI
// device event arrives. This reacts within milliseconds without any
// dependency on libudev. Kernel media change events are forwarded as
// well. A slow periodic rescan remains as a safety net, and is used
// exclusively if the netlink socket cannot be opened.

#include "DeviceMonitor.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>

#include <errno.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"

namespace fs = std::filesystem;

namespace
{

// Root of the kernel SCSI device tree.
const char *SYSFS_SCSI_DEVICES = "/sys/bus/scsi/devices";

// SCSI peripheral device type for CD/DVD drives.
constexpr int SCSI_TYPE_ROM = 5;

// Interval between device tree scans when uevents are unavailable, in
// milliseconds.
constexpr int POLL_INTERVAL_MS = 2000;

// Safety-net rescan interval when uevents are available.
constexpr int FALLBACK_INTERVAL_MS = 30000;

// Delay of the follow-up rescan after a uevent. Removal events are sent
// before the sysfs entries disappear, and a SCSI device's block node
// shows up in a later event than the device itself, so the tree is
// scanned again once things have settled.
constexpr int SETTLE_DELAY_MS = 500;

// Opens a socket receiving kernel uevents, or returns -1.
int openUeventSocket()
{
    int fd = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_KOBJECT_UEVENT);
    if (fd < 0) {
        log_message(1, "DeviceMonitor: cannot open uevent socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_nl addr;
    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = 1; // kernel events (udev's own events are group 2)

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_message(1, "DeviceMonitor: cannot bind uevent socket: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

// What a batch of uevents asks for.
struct UeventResult {
    bool rescan = false;
    bool mediaChange = false;
};

// Reads all pending uevents from 'fd'. A kernel uevent is a datagram of
// NUL-separated strings: "ACTION@DEVPATH" followed by "KEY=VALUE" pairs.
UeventResult drainUevents(int fd)
{
    UeventResult result;
    char buf[8192];

    while (true) {
        struct sockaddr_nl from;
        socklen_t fromLen = sizeof(from);
        ssize_t len =
            recvfrom(fd, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromLen);
        if (len < 0) {
            // ENOBUFS means events were dropped: rescan to be safe.
            if (errno == ENOBUFS)
                result.rescan = true;
            if (errno == EINTR || errno == ENOBUFS)
                continue;
            break; // EAGAIN: drained
        }
        // Only trust messages from the kernel itself.
        if (fromLen != sizeof(from) || from.nl_pid != 0)
            continue;
        buf[len] = 0;

        bool relevant = false, mediaChange = false;
        for (char *p = buf; p < buf + len; p += strlen(p) + 1) {
            if (strcmp(p, "SUBSYSTEM=block") == 0 || strcmp(p, "SUBSYSTEM=scsi") == 0)
                relevant = true;
            else if (strcmp(p, "DISK_MEDIA_CHANGE=1") == 0 ||
                     strcmp(p, "SDEV_MEDIA_CHANGE=1") == 0)
                mediaChange = true;
        }
        if (relevant) {
            result.rescan = true;
            if (mediaChange)
                result.mediaChange = true;
        }
    }

    return result;
}

// Read a single-line sysfs attribute and strip leading/trailing
// whitespace (sysfs strings are space-padded and newline-terminated).
std::string readAttr(const fs::path &path)
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
std::string resolveNode(const fs::path &devDir)
{
    std::error_code ec;

    // Modern kernels: <devDir>/block/<name>
    fs::path blockDir = devDir / "block";
    if (fs::is_directory(blockDir, ec)) {
        for (const auto &entry : fs::directory_iterator(blockDir, ec)) {
            return "/dev/" + entry.path().filename().string();
        }
    }

    // Older kernels: <devDir>/block:<name>
    for (const auto &entry : fs::directory_iterator(devDir, ec)) {
        const std::string name = entry.path().filename().string();
        if (name.rfind("block:", 0) == 0)
            return "/dev/" + name.substr(6);
    }

    return std::string();
}

class LinuxDeviceMonitor : public DeviceMonitor
{
  public:
    LinuxDeviceMonitor() { wakeFd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK); }

    ~LinuxDeviceMonitor() override
    {
        // The worker polls wakeFd_, so it must be stopped before the fd
        // is closed.
        stop();
        if (wakeFd_ >= 0)
            close(wakeFd_);
    }

  protected:
    void wakeup() override
    {
        if (wakeFd_ >= 0) {
            uint64_t one = 1;
            [[maybe_unused]] auto r = write(wakeFd_, &one, sizeof(one));
        }
    }

    void scanLoop() override
    {
        int ueventFd = (wakeFd_ >= 0) ? openUeventSocket() : -1;

        if (ueventFd < 0) {
            // No uevents: plain periodic polling.
            do {
                publish(enumerate());
            } while (!sleep(POLL_INTERVAL_MS));
            return;
        }

        using Clock = std::chrono::steady_clock;
        std::optional<Clock::time_point> settleAt;

        // Initial scan.
        publish(enumerate());

        while (!stopRequested()) {
            int timeoutMs = FALLBACK_INTERVAL_MS;
            if (settleAt) {
                auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                *settleAt - Clock::now())
                                .count();
                timeoutMs = left > 0 ? (int)left : 0;
            }

            struct pollfd fds[2] = {{ueventFd, POLLIN, 0}, {wakeFd_, POLLIN, 0}};
            int n = ::poll(fds, 2, timeoutMs);
            if (n < 0 && errno != EINTR)
                break;
            if (stopRequested())
                break;

            bool rescan = (n == 0); // timeout: settle or safety-net rescan
            if (n == 0)
                settleAt.reset();

            if (fds[1].revents & POLLIN) {
                uint64_t v;
                [[maybe_unused]] auto r = read(wakeFd_, &v, sizeof(v));
            }
            if (takeRescanRequest())
                rescan = true;

            if (fds[0].revents & POLLIN) {
                auto events = drainUevents(ueventFd);
                if (events.rescan) {
                    rescan = true;
                    settleAt = Clock::now() + std::chrono::milliseconds(SETTLE_DELAY_MS);
                }
                if (events.mediaChange)
                    notifyMediaChange();
            }

            if (rescan)
                publish(enumerate());
        }

        close(ueventFd);
    }

  private:
    // Written by wakeup() to interrupt the worker's poll().
    int wakeFd_ = -1;

    std::vector<DeviceInfo> enumerate()
    {
        std::vector<DeviceInfo> result;
        std::error_code ec;

        fs::directory_iterator it(SYSFS_SCSI_DEVICES, ec);
        if (ec)
            return result;

        for (const auto &entry : it) {
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

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

// \file DeviceMonitorNull.cc
//   \brief Fallback device monitor for platforms without a native backend.
//
// Used on any host for which no DeviceMonitor implementation exists yet
// (currently everything but Linux). It reports no devices and never fires
// signal_changed(), so the GUI simply shows an empty device list rather
// than failing to link. Replace this with a real backend when adding
// MacOS or Windows support.

#include "DeviceMonitor.h"

namespace
{

class NullDeviceMonitor : public DeviceMonitor
{
  protected:
    void scanLoop() override
    {
        // Nothing to monitor; publish an empty list once and return so the
        // worker thread exits immediately.
        publish({});
    }
};

} // namespace

std::unique_ptr<DeviceMonitor> DeviceMonitor::create()
{
    return std::make_unique<NullDeviceMonitor>();
}

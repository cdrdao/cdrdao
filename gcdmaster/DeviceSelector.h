/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __DEVICE_LIST_H
#define __DEVICE_LIST_H

#include <gtk/gtk.h>
#include <gtkmm.h>
#include <memory>
#include <string>
#include <functional>
#include <optional>

class TocEdit;
#include "CdDevice.h"

class DeviceSelector : public Gtk::Frame
{
  public:
    DeviceSelector(std::optional<CdDevice::DeviceType> filterType = std::nullopt);
    ~DeviceSelector() {};

    CdDevice *selection();
    // Selects the first device if none is selected.
    void selectOne();
    // Selects the first listed device for which 'pred' is true, unless the
    // selected one already satisfies it. Returns false if none does.
    bool selectFirst(const std::function<bool(const CdDevice *)> &pred);
    void appendTableEntry(CdDevice *);
    // Rebuilds the list from CdDevice::deviceList(), keeping the current
    // selection if that device is still listed. The selector does this by
    // itself whenever the device list changes; calling it is only needed
    // after changing device settings.
    void import();
    // Refreshes the displayed status of all devices.
    void importStatus();

    sigc::signal<void()> signalChanged;

  private:
    void on_selection_changed(guint, guint);
    void onStatusChanged(CdDevice *);

    std::optional<CdDevice::DeviceType> filterType_;

    // Set while import() rebuilds the model, to suppress the intermediate
    // selection changes.
    bool importing_ = false;

    class ListColumns : public Glib::Object
    {
      public:
        CdDevice *device_;
        // The status label currently bound to this row, if any, so the
        // status can be updated in place without rebuilding the model.
        Gtk::Label *statusLabel_ = nullptr;

        static Glib::RefPtr<ListColumns> create(CdDevice *device)
        {
            return Glib::make_refptr_for_instance<ListColumns>(new ListColumns(device));
        }

      protected:
        ListColumns(CdDevice *device) : device_(device) {}
    };

    Gtk::ColumnView colView_;
    Glib::RefPtr<Gtk::SingleSelection> selectionModel_;
    Glib::RefPtr<Gio::ListStore<ListColumns>> listModel_;
};

#endif

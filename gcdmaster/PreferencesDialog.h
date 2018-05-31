/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2007  Denis Leroy
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

#ifndef __PREFERENCES_DIALOG_H
#define __PREFERENCES_DIALOG_H

#include <gtkmm.h>

#include "CdDevice.h"

class PreferencesDialog : public Gtk::Dialog
{
 public:
    PreferencesDialog(BaseObjectType* cobject,
		      const Glib::RefPtr<Gtk::Builder>& builder);
    virtual ~PreferencesDialog();

    static Glib::RefPtr<PreferencesDialog> create(Glib::RefPtr<Gtk::Builder>& builder);

    void show();
    void update(unsigned long level);

 protected:
    void read_from_settings();
    bool save_to_settings();
    void on_button_apply();
    void on_button_cancel();
    void on_button_ok();
    void on_selection_changed();
    void on_driver_changed();
    void on_dev_type_changed();
    void rescan_action();

    // import/export, as in transferring the gui data to and from the
    // CdDevice list.
    void import_devices();
    void export_devices();
    void import_status();
    void import_selected_row(Gtk::TreeIter);
    void export_selected_row(Gtk::TreeIter);
    void append_entry(CdDevice* dev);

    Gtk::Button* applyButton_;
    Gtk::Button* cancelButton_;
    Gtk::Button* okButton_;
    Gtk::FileChooserButton* tempDirEntry_;

    struct DeviceData {
        std::string dev;
        int driverId;
        CdDevice::DeviceType deviceType;
        unsigned long options;
    };

    class ListColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        ListColumns() { add(dev); add(description);  add(status); add(data); };
        Gtk::TreeModelColumn<std::string> dev;
        Gtk::TreeModelColumn<std::string> description;
        Gtk::TreeModelColumn<std::string> status;
        Gtk::TreeModelColumn<DeviceData*> data;
    };
    Gtk::TreeView* deviceList_;
    Glib::RefPtr<Gtk::ListStore> deviceListModel_;
    ListColumns deviceListColumns_;
    Gtk::TreeIter selectedRow_;
    Gtk::Entry* driverOptionsEntry_;
    Gtk::ComboBoxText* driverMenu_;
    Gtk::ComboBoxText* devtypeMenu_;
};

#endif

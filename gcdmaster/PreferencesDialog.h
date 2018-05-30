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
    void readFromGConf();
    bool saveToGConf();
    void on_button_apply();
    void on_button_cancel();
    void on_button_ok();
    void rescan_action();
    void import_devices();
    void import_status();

    Gtk::Button* applyButton_;
    Gtk::Button* cancelButton_;
    Gtk::Button* okButton_;
    Gtk::FileChooserButton* tempDirEntry_;

    struct DeviceData {
        std::string dev;
        int driverId;
        int deviceType;
        unsigned long options;
    };

    class ListColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        ListColumns() { add(dev); add(description);  add(status); };
        Gtk::TreeModelColumn<std::string> dev;
        Gtk::TreeModelColumn<std::string> description;
        Gtk::TreeModelColumn<std::string> status;
        Gtk::TreeModelColumn<DeviceData*> data;
    };
    Gtk::TreeView* deviceList_;
    Glib::RefPtr<Gtk::ListStore> deviceListModel_;
    ListColumns deviceListColumns_;
};

#endif

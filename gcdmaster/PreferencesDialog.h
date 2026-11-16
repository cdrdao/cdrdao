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
#include <unordered_map>

#include "CdDevice.h"
#include "DeviceSelector.h"
#include "gcdmaster.h"

class PreferencesDialog : public Gtk::Window
{
public:
    PreferencesDialog(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder);
    virtual ~PreferencesDialog();

    static PreferencesDialog* create(const Glib::RefPtr<Gtk::Builder>& builder,
                                     Gtk::Window& parent);

    void show_dialog(); // Renamed to avoid confusion with widget::show
    void update(unsigned long level);

protected:
    void read_from_settings();
    void save_to_settings();
    
    // UI Event Handlers
    void on_button_apply();
    void on_button_cancel();
    void on_button_ok();
    void on_button_reset();
    void on_selection_changed();
    void on_temp_dir_button_clicked();
    void on_cdrdao_exec_button_clicked();
    void on_options_activate();
    void rescan_action();

    void import_devices();
    void export_devices();
    void import_status();
    void import_selected_row(CdDevice* device);
    void export_selected_row(CdDevice* device);
    void append_entry(CdDevice* dev);

    // Widgets
    Gtk::Button* tempDirButton_ = nullptr;
    Gtk::Button* cdrdaoExecButton_ = nullptr;
    Gtk::Entry* driverOptionsEntry_ = nullptr;
    Gtk::DropDown* driverMenu_ = nullptr;
    Gtk::DropDown* devtypeMenu_ = nullptr;

    std::string selectedTempPath_;
    std::string selectedCdrdaoExec_;
    
    DeviceSelector* deviceSelector_;

    struct DeviceData {
        std::string driver;
        std::string deviceType;
        std::string options;
    };
    std::unordered_map<CdDevice*, struct DeviceData> dataMap_;
    CdDevice* selectedDevice_;
};

std::optional<Glib::ustring> dropDownGet(Gtk::DropDown* dd);
bool dropDownSet(Gtk::DropDown* dd, const Glib::ustring& val);

#endif

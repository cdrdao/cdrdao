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

#include <iostream>
#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "config.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "PreferencesDialog.h"
#include "CdDevice.h"
#include "ConfigManager.h"
#include "MessageBox.h"
#include "trackdb/TempFileManager.h"

PreferencesDialog* PreferencesDialog::create(const Glib::RefPtr<Gtk::Builder>& builder,
                                             Gtk::Window& parent)
{
    builder->add_from_resource("/org/gnome/gcdmaster/preferences.ui");
    auto window = Gtk::Builder::get_widget_derived<PreferencesDialog>(builder, "preferences-window");
    if (!window)
        throw std::runtime_error("preferences_window resource error");
    window->set_transient_for(parent);

    return window;
}

PreferencesDialog::PreferencesDialog(BaseObjectType* cobject,
                                     const Glib::RefPtr<Gtk::Builder>& builder) :
    Gtk::Window(cobject)
{
    set_modal(true);

    // In GTK4, buttons are often handled via signal_clicked or signal_response
    auto applyButton = builder->get_widget<Gtk::Button>("apply-button");
    auto okButton = builder->get_widget<Gtk::Button>("ok-button");
    auto cancelButton = builder->get_widget<Gtk::Button>("cancel-button");
    auto resetButton = builder->get_widget<Gtk::Button>("reset-button");
    
    tempDirButton_ = builder->get_widget<Gtk::Button>("temp-directory-button");
    tempDirButton_->signal_clicked().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_temp_dir_button_clicked));
    deviceList_ = builder->get_widget<Gtk::TreeView>("device-tree");
    driverOptionsEntry_ = builder->get_widget<Gtk::Entry>("driver-options");
    driverMenu_ = builder->get_widget<Gtk::ComboBoxText>("driver-list");
    devtypeMenu_ = builder->get_widget<Gtk::ComboBoxText>("device-type-list");

    if (!applyButton || !okButton || !cancelButton || !tempDirButton_ || !deviceList_ || !driverOptionsEntry_) {
        throw std::runtime_error("Unable to create all GUI widgets from builder file");
    }

    applyButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_apply));
    cancelButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_cancel));
    okButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_ok));
    resetButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_reset));

    if (auto rescanBtn = builder->get_widget<Gtk::Button>("rescan-button")) {
        rescanBtn->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::rescan_action));
    }

    // Setup Device List treeview
    deviceListModel_ = Gtk::ListStore::create(deviceListColumns_);
    deviceList_->set_model(deviceListModel_);
    deviceList_->append_column(_("Device"), deviceListColumns_.description);
    deviceList_->append_column(_("Status"), deviceListColumns_.status);
    deviceList_->get_column(0)->set_expand(true);

    deviceList_->get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_selection_changed));

    // Populate Driver Combo box
    if (driverMenu_) {
        for (const auto& str : CdDevice::driverNames())
            driverMenu_->append(str);
        driverMenu_->signal_changed().connect(sigc::mem_fun(*this, &PreferencesDialog::on_driver_changed));
    }

    // Populate Device type combo box
    if (devtypeMenu_) {
        for (const auto& str : CdDevice::deviceNames())
            devtypeMenu_->append(str);
        devtypeMenu_->signal_changed().connect(sigc::mem_fun(*this, &PreferencesDialog::on_dev_type_changed));
    }

    read_from_settings();
    import_devices();
}

PreferencesDialog::~PreferencesDialog() {}

void PreferencesDialog::show_dialog()
{
    read_from_settings();
    this->set_visible(true);
}

void PreferencesDialog::read_from_settings()
{
    selectedTempPath_ = CONFIG_MANAGER->getTempDir();
    if (tempDirButton_)
        tempDirButton_->set_label(selectedTempPath_.empty() ? _("Select Folder...") : selectedTempPath_);
}

bool PreferencesDialog::save_to_settings()
{
    if (selectedTempPath_.empty())
        return false;
    
    if (!tempFileManager.setTempDirectory(selectedTempPath_.c_str())) {
        ErrorBox::message(*this, _("The directory you entered cannot be used..."));
        read_from_settings();
        return false;
    }

    try {
        CONFIG_MANAGER->setTempDir(selectedTempPath_);
    } catch (const Glib::Error& error) {
        std::cerr << error.what() << std::endl;
    }
    return true;
}

void PreferencesDialog::update(unsigned long level)
{
    if (!is_visible()) return;

    if (level & UPD_CD_DEVICES)
        import_devices();
    else if (level & UPD_CD_DEVICE_STATUS)
        import_status();
}

void PreferencesDialog::on_button_apply()
{
    if (selectedRow_)
        export_selected_row(selectedRow_);
    export_devices();
    save_to_settings();
    guiUpdate(UPD_CD_DEVICES);
}

void PreferencesDialog::on_button_cancel()
{
    read_from_settings();
    hide();
}

void PreferencesDialog::on_button_ok()
{
    on_button_apply();
    hide();
}

void PreferencesDialog::on_button_reset()
{
    for (auto dev : CdDevice::deviceList())
        dev->autoSelectDriver();
    import_devices();
    save_to_settings();
    guiUpdate(UPD_CD_DEVICES);
}

void PreferencesDialog::on_selection_changed()
{
    auto new_sel = deviceList_->get_selection()->get_selected();

    if (selectedRow_ != new_sel) {
        if (selectedRow_)
            export_selected_row(selectedRow_);
        selectedRow_ = new_sel;
        import_selected_row(selectedRow_);
    }
}

void PreferencesDialog::on_driver_changed()
{
    if (selectedRow_ && driverMenu_) {
        DeviceData* data = (*selectedRow_)[deviceListColumns_.data];
        if (data) {
            data->driverId = CdDevice::driverName2Id(driverMenu_->get_active_text().c_str());
        }
    }
}

void PreferencesDialog::on_dev_type_changed()
{
    if (selectedRow_ && devtypeMenu_) {
        DeviceData* data = (*selectedRow_)[deviceListColumns_.data];
        if (data) {
            data->deviceType = CdDevice::devtypeName2Id(devtypeMenu_->get_active_text());
        }
    }
}

void PreferencesDialog::append_entry(CdDevice* dev)
{
    auto data = new DeviceData;
    data->dev = dev->dev();
    data->driverId = dev->driverId();
    data->options = dev->driverOptions();
    data->deviceType = dev->deviceType();

    auto iter = deviceListModel_->append();
    auto row = *iter;
    row[deviceListColumns_.dev] = dev->dev();
    row[deviceListColumns_.description] = dev->description();
    row[deviceListColumns_.status] = CdDevice::statusNames()[dev->status()];
    row[deviceListColumns_.data] = data;
}

void PreferencesDialog::import_devices()
{
    deviceList_->get_selection()->unselect_all();
    selectedRow_ = Gtk::TreeModel::iterator(); 
    deviceListModel_->clear();

    for (auto dev : CdDevice::deviceList()) {
        append_entry(dev);
    }

    if (!deviceListModel_->children().empty()) {
        deviceList_->get_selection()->select(deviceListModel_->children().begin());
    }
}

void PreferencesDialog::export_devices()
{
    for (auto row : deviceListModel_->children()) {
        DeviceData* data = row[deviceListColumns_.data];
        CdDevice* dev;
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {
            if (dev->driverId() != data->driverId || 
                dev->deviceType() != data->deviceType || 
                dev->driverOptions() != data->options) {
                
                dev->driverId(data->driverId);
                dev->deviceType(data->deviceType);
                dev->driverOptions(data->options);
                dev->manuallyConfigured(true);
            }
        }
    }
}

void PreferencesDialog::import_status()
{
    for (auto row : deviceListModel_->children()) {
        DeviceData* data = row[deviceListColumns_.data];
        CdDevice* dev;
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {
            row[deviceListColumns_.status] = CdDevice::statusNames()[dev->status()];
        }
    }
}

void PreferencesDialog::export_selected_row(const Gtk::TreeModel::iterator& iter)
{
    if (iter && driverOptionsEntry_) {
        DeviceData* data = (*iter)[deviceListColumns_.data];
        if (data) {
            data->options = std::strtoul(driverOptionsEntry_->get_buffer()->get_text().c_str(), nullptr, 0);
        }
    }
}

void PreferencesDialog::import_selected_row(const Gtk::TreeModel::iterator& iter)
{
    if (iter) {
        DeviceData* data = (*iter)[deviceListColumns_.data];
        driverMenu_->set_sensitive(true);
        driverMenu_->set_active_text(CdDevice::driverNames()[data->driverId]);
        devtypeMenu_->set_sensitive(true);
        devtypeMenu_->set_active_text(CdDevice::deviceNames()[data->deviceType]);
        driverOptionsEntry_->set_sensitive(true);
        
        char buf[48];
        snprintf(buf, sizeof(buf), "0x%lx", data->options);
        driverOptionsEntry_->get_buffer()->set_text(buf);
    } else {
        driverMenu_->set_sensitive(false);
        devtypeMenu_->set_sensitive(false);
        driverOptionsEntry_->set_sensitive(false);
    }
}

void PreferencesDialog::rescan_action()
{
    CdDevice::scan();
    guiUpdate(UPD_CD_DEVICES);
}

void PreferencesDialog::on_temp_dir_button_clicked()
{
    // Create the dialog
    auto dialog = new Gtk::FileChooserDialog(*this, _("Select Temporary Directory"),
                                             Gtk::FileChooser::Action::SELECT_FOLDER,
                                             true /* use_header_bar */);
    
    dialog->add_button(_("_Cancel"), Gtk::ResponseType::CANCEL);
    dialog->add_button(_("_Open"), Gtk::ResponseType::ACCEPT);
    dialog->set_modal(true);

    // Set existing path if available
    if (!selectedTempPath_.empty()) {
        dialog->set_file(Gio::File::create_for_path(selectedTempPath_));
    }

    // Handle the response
    dialog->signal_response().connect([this, dialog](int response_id) {
        if (response_id == (int)Gtk::ResponseType::ACCEPT) {
            auto file = dialog->get_file();
            if (file) {
                selectedTempPath_ = file->get_path();
                // Update button label to show current selection
                tempDirButton_->set_label(selectedTempPath_);
            }
        }
        delete dialog; // Clean up
    });

    dialog->show();
}

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

PreferencesDialog::PreferencesDialog(BaseObjectType* cobject,
				     const Glib::RefPtr<Gtk::Builder>& builder) :
	Gtk::Dialog(cobject)
{
    builder->get_widget("apply-button", applyButton_);
    builder->get_widget("ok-button", okButton_);
    builder->get_widget("cancel-button", cancelButton_);
    builder->get_widget("temp-directory", tempDirEntry_);
    builder->get_widget("device-tree", deviceList_);
    builder->get_widget("driver-options", driverOptionsEntry_);

    if (!applyButton_ || !okButton_ || !cancelButton_ || !tempDirEntry_ ||
        !deviceList_ || !driverOptionsEntry_) {
        std::cerr << "Unable to create all GUI widgets from glade file\n";
        exit(1);
    }

    applyButton_->signal_clicked()
        .connect(sigc::mem_fun(*this,
                               &PreferencesDialog::on_button_apply));
    cancelButton_->signal_clicked()
        .connect(sigc::mem_fun(*this,
                               &PreferencesDialog::on_button_cancel));
    okButton_->signal_clicked()
        .connect(sigc::mem_fun(*this,
                               &PreferencesDialog::on_button_ok));

    Gtk::Button* button = NULL;
    builder->get_widget("rescan-button", button);
    if (button)
        button->signal_clicked().
            connect(sigc::mem_fun(*this, &PreferencesDialog::rescan_action));

    // Setup Device List treeview.
    deviceListModel_ = Gtk::ListStore::create(deviceListColumns_);
    deviceList_->set_model(deviceListModel_);
    deviceList_->append_column(_("Device"), deviceListColumns_.description);
    deviceList_->append_column(_("Status"), deviceListColumns_.status);
    deviceList_->get_column(0)->set_expand();

    selectedRow_ = deviceList_->get_selection()->get_selected();
    deviceList_->get_selection()->signal_changed().
        connect(mem_fun(*this, &PreferencesDialog::on_selection_changed));

    // Populate Driver Combo box
    builder->get_widget("driver-list", driverMenu_);
    if (driverMenu_) {
        for (auto str : CdDevice::driverNames)
            driverMenu_->append(str.c_str());
        driverMenu_->signal_changed().connect(mem_fun(*this,
                                               &PreferencesDialog::on_driver_changed));
    }
    // Populate Device type combo box
    builder->get_widget("device-type-list", devtypeMenu_);
    if (devtypeMenu_) {
        for (auto str : CdDevice::devtypeNames)
            devtypeMenu_->append(str);
        devtypeMenu_->signal_changed().
            connect(mem_fun(*this,&PreferencesDialog::on_dev_type_changed));
    }

    read_from_settings();
    import_devices();
}

PreferencesDialog::~PreferencesDialog()
{
}

Glib::RefPtr<PreferencesDialog>
PreferencesDialog::create(Glib::RefPtr<Gtk::Builder>& builder)
{
    builder->add_from_resource("/org/gnome/gcdmaster/preferences.ui");

    PreferencesDialog* pd;
    builder->get_widget_derived("preferences", pd);
    if (!pd)
        throw std::runtime_error("No \"preferences\" resource");

    printf("Preferences window created.\n");
    return Glib::RefPtr<PreferencesDialog>(pd);
}

void PreferencesDialog::show()
{
    read_from_settings();
    Gtk::Dialog::show();
}

void PreferencesDialog::read_from_settings()
{
    const Glib::ustring text = configManager->get_string("temp-dir");
    printf("Setting temp-dir to %s\n", text.c_str());
    tempDirEntry_->set_filename(text);
}

bool PreferencesDialog::save_to_settings()
{
    const Glib::ustring text = tempDirEntry_->get_filename();

    if (!tempFileManager.setTempDirectory(text.c_str())) {

	ErrorBox errBox(_("The directory you entered cannot be used as a "
			  "temporary files directory."));
	errBox.run();
	read_from_settings();
	return false;
    }

    try {
	configManager->set("temp-dir", text);
    } catch (const Glib::Error& error) {
        std::cerr << error.what() << std::endl;
    }
    return true;
}

void PreferencesDialog::update(unsigned long level)
{
    if (!is_visible()) {
        printf("Preferences dialog ignored (%d)\n", level);
        return;
    }

    printf("Preferences dialog update (%d)\n", level);

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
    if (selectedRow_)
        export_selected_row(selectedRow_);
    save_to_settings();
    export_devices();
    hide();
}

void PreferencesDialog::on_selection_changed()
{
    Gtk::TreeIter new_sel = deviceList_->get_selection()->get_selected();

    if ((bool)selectedRow_ != (bool)new_sel || selectedRow_ != new_sel) {
        if (selectedRow_)
            export_selected_row(selectedRow_);
        selectedRow_ = new_sel;
        import_selected_row(selectedRow_);
    }
    printf("Device list selection changed\n");
}

void PreferencesDialog::on_driver_changed()
{
    DeviceData* data;

    printf("Device driver changed\n");
    if (selectedRow_ && (data = (*selectedRow_)[deviceListColumns_.data])) {
        data->driverId = CdDevice::driverName2Id(driverMenu_->get_active_text().c_str());
    }
}

void PreferencesDialog::on_dev_type_changed()
{
    DeviceData* data;

    printf("Device type changed\n");
    if (selectedRow_ && (data = (*selectedRow_)[deviceListColumns_.data])) {
        data->deviceType = CdDevice::devtypeName2Id(devtypeMenu_->get_active_text());
    }

}

void PreferencesDialog::append_entry(CdDevice* dev)
{
    DeviceData* data;

    data = new DeviceData;
    data->dev = dev->dev();
    data->driverId = dev->driverId();
    data->options = dev->driverOptions();
    data->deviceType = dev->deviceType();

    Gtk::TreeIter iter = deviceListModel_->append();
    Gtk::TreeModel::Row row = *iter;
    row[deviceListColumns_.dev] = dev->dev();
    row[deviceListColumns_.description] = dev->description();
    row[deviceListColumns_.status] = CdDevice::statusNames[dev->status()];
    row[deviceListColumns_.data] = data;
}

void PreferencesDialog::import_devices()
{
    deviceList_->get_selection()->unselect_all();
    selectedRow_ = deviceList_->get_selection()->get_selected();
    deviceListModel_->clear();

    for (int i = 0; i < CdDevice::count(); i++) {
        append_entry(CdDevice::at(i));
    }

    if (deviceListModel_->children().size() > 0) {
        deviceList_->columns_autosize();
        deviceList_->get_selection()->select(Gtk::TreeModel::Path((unsigned)1));
    }
}

void PreferencesDialog::export_devices()
{
    DeviceData* data;
    CdDevice* dev;

    Gtk::TreeNodeChildren ch = deviceListModel_->children();
    for (auto i = 0; i < ch.size(); i++) {
        Gtk::TreeRow row = ch[i];
        data = row[deviceListColumns_.data];
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {

            if (dev->driverId() != data->driverId) {
                dev->driverId(data->driverId);
                dev->manuallyConfigured(true);
            }
            if (dev->deviceType() != data->deviceType) {
                dev->deviceType(data->deviceType);
                dev->manuallyConfigured(true);
            }
            if (dev->driverOptions() != data->options) {
                dev->driverOptions(data->options);
                dev->manuallyConfigured(true);
            }
        }
    }
}


void PreferencesDialog::import_status()
{
    CdDevice* dev;
    DeviceData* data;

    Gtk::TreeNodeChildren ch = deviceListModel_->children();
    for (unsigned i = 0; i < ch.size(); i++) {
        Gtk::TreeRow row = ch[i];
        data = row[deviceListColumns_.data];
        if (data && (dev = CdDevice::find(data->dev.c_str()))) {
            row[deviceListColumns_.status] = CdDevice::statusNames[dev->status()];
        }
    }
}

void PreferencesDialog::export_selected_row(Gtk::TreeIter row)
{
    DeviceData* data;

    if (row) {
        data = (*row)[deviceListColumns_.data];

        if (data)
            data->options = strtoul(driverOptionsEntry_->get_text().c_str(), NULL, 0);
    }
}

void PreferencesDialog::import_selected_row(Gtk::TreeIter row)
{
    char buf[48];
    DeviceData* data;

    if (selectedRow_) {
        data = (*selectedRow_)[deviceListColumns_.data];
        driverMenu_->set_sensitive(true);
        driverMenu_->set_active_text(CdDevice::driverNames[data->driverId]);
        devtypeMenu_->set_sensitive(true);
        devtypeMenu_->set_active_text(CdDevice::devtypeNames[data->deviceType]);
        driverOptionsEntry_->set_sensitive(true);
        sprintf(buf, "0x%lx", data->options);
        driverOptionsEntry_->set_text(buf);

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

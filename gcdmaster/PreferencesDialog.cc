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

    if (!applyButton_ || !okButton_ || !cancelButton_ || !tempDirEntry_ ||
        !deviceList_) {
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

    readFromGConf();
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
    readFromGConf();
    Gtk::Dialog::show();
}

void PreferencesDialog::readFromGConf()
{
    const Glib::ustring text = configManager->get_string("temp-dir");
    printf("Setting temp-dir to %s\n", text.c_str());
    tempDirEntry_->set_filename(text);
}

bool PreferencesDialog::saveToGConf()
{
    const Glib::ustring text = tempDirEntry_->get_filename();

    if (!tempFileManager.setTempDirectory(text.c_str())) {

	ErrorBox errBox(_("The directory you entered cannot be used as a "
			  "temporary files directory."));
	errBox.run();
	readFromGConf();
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
    saveToGConf();
}

void PreferencesDialog::on_button_cancel()
{
    readFromGConf();
    hide();
}

void PreferencesDialog::on_button_ok()
{
    if (saveToGConf())
	hide();
}

void PreferencesDialog::import_devices()
{
    deviceListModel_->clear();

    for (int i = 0; i < CdDevice::count(); i++) {
        auto dev = CdDevice::at(i);

        Gtk::TreeIter iter = deviceListModel_->append();
        Gtk::TreeModel::Row row = *iter;
        row[deviceListColumns_.dev] = dev->dev();
        row[deviceListColumns_.description] = dev->description();
        row[deviceListColumns_.status] = CdDevice::status2string(dev->status());

        if (dev->status() == CdDevice::DEV_READY)
            deviceList_->get_selection()->select(iter);
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
            row[deviceListColumns_.status] = CdDevice::status2string(dev->status());
        }
    }
}

void PreferencesDialog::rescan_action()
{
    CdDevice::scan();
    guiUpdate(UPD_CD_DEVICES);
}

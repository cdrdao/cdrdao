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

    cdrdaoExecButton_ = builder->get_widget<Gtk::Button>("cdrdao-exec-button");
    cdrdaoExecButton_->signal_clicked().connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_cdrdao_exec_button_clicked));

    driverOptionsEntry_ = builder->get_widget<Gtk::Entry>("driver-options");
    driverOptionsEntry_->signal_activate().connect(
	sigc::mem_fun(*this, &PreferencesDialog::on_options_activate));
    driverMenu_ = builder->get_widget<Gtk::ComboBoxText>("driver-list");
    devtypeMenu_ = builder->get_widget<Gtk::ComboBoxText>("device-type-list");

    if (!(applyButton && okButton && cancelButton && resetButton && driverMenu_ && devtypeMenu_)) {
        throw std::runtime_error("Unable to create all GUI widgets from builder file");
    }

    applyButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_apply));
    cancelButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_cancel));
    okButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_ok));
    resetButton->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::on_button_reset));

    if (auto rescanBtn = builder->get_widget<Gtk::Button>("rescan-button")) {
        rescanBtn->signal_clicked().connect(sigc::mem_fun(*this, &PreferencesDialog::rescan_action));
    }

    auto deviceBox = builder->get_widget<Gtk::Box>("device-box");
    deviceSelector_ = Gtk::make_managed<DeviceSelector>();
    deviceSelector_->signalChanged.connect(
        sigc::mem_fun(*this, &PreferencesDialog::on_selection_changed));
    deviceBox->prepend(*deviceSelector_);


    // Populate Driver Combo box
    if (driverMenu_) {
        for (const auto& str : CdDevice::driverNames())
            driverMenu_->append(str);
    }

    // Populate Device type combo box
    if (devtypeMenu_) {
        for (const auto& str : CdDevice::deviceNames())
            devtypeMenu_->append(str);
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
    tempDirButton_->set_label(selectedTempPath_.empty()
			      ? _("Select Folder...")
			      : selectedTempPath_);

    selectedCdrdaoExec_ = CONFIG_MANAGER->getCdrdaoPath();
    cdrdaoExecButton_->set_label(selectedCdrdaoExec_.empty()
				 ? _("Select Executable...")
				 : selectedCdrdaoExec_);
}

void PreferencesDialog::save_to_settings()
{
    if (!selectedTempPath_.empty()) {
	if (!tempFileManager.setTempDirectory(selectedTempPath_.c_str())) {
	    ErrorBox::message(*this, _("The directory you entered cannot be used..."));
	    read_from_settings();
	} else {
	    try {
		CONFIG_MANAGER->setTempDir(selectedTempPath_);
	    } catch (const Glib::Error& error) {
		std::cerr << error.what() << std::endl;
	    }
	}
    }
    if (!selectedCdrdaoExec_.empty()) {
	try {
	    CONFIG_MANAGER->setCdrdaoPath(selectedCdrdaoExec_);
	} catch (const Glib::Error& error) {
	    std::cerr << error.what() << std::endl;
	}
    }
}

void PreferencesDialog::update(unsigned long level)
{
    log_message(1, "[update %x] PreferencesDialog", level);
    if (!is_visible()) return;

    if (level & UPD_CD_DEVICES)
        import_devices();
    else if (level & UPD_CD_DEVICE_STATUS)
        import_status();
}

void PreferencesDialog::on_button_apply()
{
    if (selectedDevice_)
        export_selected_row(selectedDevice_);
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
    auto new_sel = deviceSelector_->selection();

    if (selectedDevice_ != new_sel) {
        if (selectedDevice_)
            export_selected_row(selectedDevice_);
        selectedDevice_ = new_sel;
        import_selected_row(selectedDevice_);
    }
}

void PreferencesDialog::append_entry(CdDevice* dev)
{
    std::stringstream tohex;
    tohex << "0x" << std::hex << dev->driverOptions();
    
    DeviceData data{dev->driver(), dev->deviceTypeName(), tohex.str()};
    dataMap_[dev] = data;
}

void PreferencesDialog::import_devices()
{
    dataMap_.clear();
    for (auto dev : CdDevice::deviceList())
        append_entry(dev);
    deviceSelector_->import();
}

void PreferencesDialog::export_devices()
{
    for (auto *dev : CdDevice::deviceList()) {
        if (dataMap_.find(dev) != dataMap_.end()) {
            auto data = dataMap_[dev];
            dev->driver(data.driver);
            dev->deviceType(CdDevice::devtypeName2Id(data.deviceType));
	    try {
		dev->driverOptions(data.options.empty() ? 0 : std::stoul(data.options, nullptr, 0));
	    } catch (...) {
		ErrorBox::message(*this, _("Could not parse hex options value. Setting to 0."));
		dev->driverOptions(0);
	    }
            dev->manuallyConfigured(true);
        }
    }
}

void PreferencesDialog::import_status()
{
    deviceSelector_->importStatus();
}

void PreferencesDialog::export_selected_row(CdDevice* device)
{
    if (!device)
        return;
    auto data = dataMap_[device];
    data.driver = driverMenu_->get_active_text();
    data.deviceType = devtypeMenu_->get_active_text();
    data.options = driverOptionsEntry_->get_buffer()->get_text();
    dataMap_[device] = data;
}

void PreferencesDialog::import_selected_row(CdDevice* device)
{
    if (device) {
        auto data = dataMap_[device];
    
        driverMenu_->set_sensitive(true);
        driverMenu_->set_active_text(data.driver);
        devtypeMenu_->set_sensitive(true);
        devtypeMenu_->set_active_text(data.deviceType);
        driverOptionsEntry_->set_sensitive(true);
        driverOptionsEntry_->get_buffer()->set_text(data.options);
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

void PreferencesDialog::on_options_activate()
{
    auto val = driverOptionsEntry_->get_buffer()->get_text();
    auto hexvalue = 0;

    if (!val.empty()) {
	try {
	    hexvalue = std::stoul(val, nullptr, 16);
	} catch (...) {
	    log_message(0, "nope");
	    ErrorBox::message(*this, _("Please enter a hexadecimal value"));
	}
    }
    std::stringstream tohex;
    tohex << "0x" << std::hex << hexvalue;
    driverOptionsEntry_->get_buffer()->set_text(tohex.str());
}

void PreferencesDialog::on_temp_dir_button_clicked()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_modal(true);
    dialog->set_title(_("Select Temporary Directory"));

    if (!selectedTempPath_.empty()) {
        dialog->set_initial_folder(Gio::File::create_for_path(selectedTempPath_));
    }

    dialog->select_folder([this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
	    auto folder = dialog->select_folder_finish(result);
	    if (folder) {
		selectedTempPath_ = folder->get_path();
		tempDirButton_->set_label(selectedTempPath_);
            }
	} catch (...) {
	}
    });
}

void PreferencesDialog::on_cdrdao_exec_button_clicked()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_modal(true);
    dialog->set_title(_("Select cdrdao Executable"));

    if (!selectedCdrdaoExec_.empty())
	dialog->set_initial_file(Gio::File::create_for_path(selectedCdrdaoExec_));

    dialog->open([this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
	    auto file = dialog->open_finish(result);
	    if (file) {
		this->selectedCdrdaoExec_ = file->get_path();
		cdrdaoExecButton_->set_label(file->get_path());
	    }
	} catch (...) {
	}
    });
}

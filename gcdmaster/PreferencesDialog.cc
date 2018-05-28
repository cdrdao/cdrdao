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
#include "PreferencesDialog.h"
#include "ConfigManager.h"
#include "MessageBox.h"
#include "trackdb/TempFileManager.h"

PreferencesDialog::PreferencesDialog(BaseObjectType* cobject,
				     const Glib::RefPtr<Gtk::Builder>& builder) :
	Gtk::Dialog(cobject)
{
    builder->get_widget("ApplyButton", applyButton_);
    builder->get_widget("OkButton", okButton_);
    builder->get_widget("CancelButton", cancelButton_);
    builder->get_widget("TempDirectory", tempDirEntry_);
    // builder->get_widget("TempDirDialog", _tempDirDialog);
    // builder->get_widget("TempDialogButton", _browseButton);
    // builder->get_widget("TempBrowseCancel", _browseCancel);
    // builder->get_widget("TempBrowseOpen", _browseOpen);

    if (!applyButton_ || !okButton_ || !cancelButton_ || !tempDirEntry_) {
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
    // _browseButton->signal_clicked()
    //     .connect(sigc::mem_fun(*this,
    //     		       &PreferencesDialog::on_button_browse));
    // _browseCancel->signal_clicked()
    //     .connect(sigc::mem_fun(*this,
    //     		       &PreferencesDialog::on_button_browse_cancel));
    // _browseOpen->signal_clicked()
    //     .connect(sigc::mem_fun(*this,
    //     		       &PreferencesDialog::on_button_browse_open));

    // _tempDirDialog->hide();

    readFromGConf();
    // hide();
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

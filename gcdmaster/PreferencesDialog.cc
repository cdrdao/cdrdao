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
#include <glibmm/i18n.h>

#include "config.h"
#include "PreferencesDialog.h"
#include "MessageBox.h"
#include "trackdb/TempFileManager.h"
#include "ConfigManager.h"
#include "xcdrdao.h"

PreferencesDialog::PreferencesDialog(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& refBuilder)
	: Gtk::Dialog(cobject),
	  m_refBuilder(refBuilder)
{
    m_refBuilder->get_widget("ApplyButton", _applyButton);
    m_refBuilder->get_widget("OkButton", _okButton);
    m_refBuilder->get_widget("CancelButton", _cancelButton);
    m_refBuilder->get_widget("TempDirectory", _tempDirEntry);
    m_refBuilder->get_widget("TempDirDialog", _tempDirDialog);
    m_refBuilder->get_widget("TempDialogButton", _browseButton);
    m_refBuilder->get_widget("TempBrowseCancel", _browseCancel);
    m_refBuilder->get_widget("TempBrowseOpen", _browseOpen);

	if (!_applyButton || !_okButton || !_cancelButton || !_tempDirEntry
			|| !_tempDirDialog || !_browseButton || !_browseCancel
			|| !_browseOpen) {
		std::cerr << "Unable to create all GUI widgets from glade file\n";
		exit(1);
	}

	_applyButton->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_apply));
	_cancelButton->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_cancel));
	_okButton->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_ok));
	_browseButton->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_browse));
	_browseCancel->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_browse_cancel));
	_browseOpen->signal_clicked().connect(
			sigc::mem_fun(*this, &PreferencesDialog::on_button_browse_open));

    _tempDirDialog->hide();

    readFromGConf();
    Gtk::Dialog::hide();
}

PreferencesDialog::~PreferencesDialog()
{
}

void PreferencesDialog::show()
{
    readFromGConf();
    Gtk::Dialog::show();
}

void PreferencesDialog::readFromGConf()
{
  _tempDirEntry->set_text(configManager->getTempDir());
}

bool PreferencesDialog::saveToGConf()
{
  const Glib::ustring& text = _tempDirEntry->get_text();

  if (!tempFileManager.setTempDirectory(text.c_str())) {

    ErrorBox errBox(_("The directory you entered cannot be used as a "
        "temporary files directory."));
    errBox.run();
    readFromGConf();
    return false;
  }

  try {
    configManager->setTempDir(text);
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

void PreferencesDialog::on_button_browse()
{
    _tempDirDialog->show();
}

void PreferencesDialog::on_button_browse_cancel()
{
    _tempDirDialog->hide();
}

void PreferencesDialog::on_button_browse_open()
{
    _tempDirEntry->set_text(_tempDirDialog->get_filename());
    _tempDirDialog->hide();
}

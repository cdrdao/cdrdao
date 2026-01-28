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

#include "config.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "MessageBox.h"
#include "RecordHDTarget.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "TocEdit.h"
#include "guiUpdate.h"

RecordHDTarget::RecordHDTarget() : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    set_spacing(10);

    // device settings
    auto *recordOptionsFrame = Gtk::make_managed<Gtk::Frame>(_(" Record Options "));
    auto *grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(2);
    grid->set_column_spacing(10);
    grid->set_margin(5);

    recordOptionsFrame->set_child(*grid);
    append(*recordOptionsFrame);

    {
	auto *label = Gtk::make_managed<Gtk::Label>(_("Directory: "));
	grid->attach(*label, 0, 0);
	auto *dirButton = Gtk::make_managed<Gtk::Button>("temp-directory-button");
	dirButton->signal_clicked().connect(
	    sigc::mem_fun(*this, &RecordHDTarget::on_dir_button_clicked));
	grid->attach(*dirButton, 0, 1);
    }
    {
	auto *label = Gtk::make_managed<Gtk::Label>(_("Name: "));
	grid->attach(*label, 1, 0);
	fileNameEntry_ = Gtk::make_managed<Gtk::Entry>();
	grid->attach(*fileNameEntry_, 1, 1);
    }
}

void RecordHDTarget::on_dir_button_clicked()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_modal(true);
    dialog->set_title(_("Select Destination Folder"));

    dialog->select_folder([this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
	auto file = dialog->open_finish(result);
	this->dirEntry_ = file->get_path();
    });
}

std::string RecordHDTarget::getFilename()
{
    return fileNameEntry_->get_text();
}

std::string RecordHDTarget::getPath()
{
    return dirEntry_;
}

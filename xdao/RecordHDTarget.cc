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

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include <gnome.h>

#include "RecordHDTarget.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

RecordHDTarget::RecordHDTarget()
{
  Gtk::Table *table;
  Gtk::Label *label;

  active_ = 0;

  set_spacing(10);

  // device settings
  Gtk::Frame *recordOptionsFrame = new Gtk::Frame(string("Record Options"));

  table = new Gtk::Table(2, 2, false);
  table->set_row_spacings(2);
  table->set_col_spacings(10);
  table->set_border_width(5);
  table->show();

  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->show();
  vbox->pack_start(*table, false, false, 5);
  recordOptionsFrame->add(*vbox);
  
  pack_start(*recordOptionsFrame, false, false);
  recordOptionsFrame->show();

  label = new Gtk::Label(string("Directory: "));
  label->show();
  table->attach(*label, 0, 1, 0, 1, GTK_FILL);

  dirEntry_ = new Gnome::FileEntry("record_hd_target_dir_entry",
				   "Select Directory for Image");
  dirEntry_->set_directory(TRUE);
  dirEntry_->set_usize(30, 0);
  dirEntry_->show();

  table->attach(*dirEntry_, 1, 2, 0, 1);

  label = new Gtk::Label(string("Name: "));
  label->show();
  table->attach(*label, 0, 1, 1, 2, GTK_FILL);

  fileNameEntry_ = new Gtk::Entry;
  fileNameEntry_->show();
  table->attach(*fileNameEntry_, 1, 2, 1, 2);
}

void RecordHDTarget::start()
{
  active_ = 1;

  update(UPD_ALL);

  show();
}

void RecordHDTarget::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordHDTarget::update(unsigned long level)
{
  if (!active_)
    return;
}

void RecordHDTarget::cancelAction()
{
  stop();
}

Gtk::string RecordHDTarget::getFilename()
{
  return fileNameEntry_->get_text();
}

Gtk::string RecordHDTarget::getPath()
{
  Gtk::Entry *entry = static_cast<Gtk::Entry *>(dirEntry_->gtk_entry());
  return entry->get_text();
}


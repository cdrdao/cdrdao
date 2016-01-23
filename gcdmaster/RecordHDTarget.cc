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
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "RecordHDTarget.h"
#include "MessageBox.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

RecordHDTarget::RecordHDTarget()
{
  active_ = false;

  set_spacing(10);

  // device settings
  Gtk::Frame *recordOptionsFrame = manage(
      new Gtk::Frame(_(" Record Options ")));

  Gtk::Table *table = manage(new Gtk::Table(2, 2, false));
  table->set_row_spacings(2);
  table->set_col_spacings(10);
  table->set_border_width(5);

  recordOptionsFrame->add(*table);
  recordOptionsFrame->show_all();
  pack_start(*recordOptionsFrame, Gtk::PACK_SHRINK);

  Gtk::Label *label = manage(new Gtk::Label(_("Directory: ")));
  table->attach(*label, 0, 1, 0, 1, Gtk::FILL);

  dirEntry_ = manage(
      new Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER));
  table->attach(*dirEntry_, 1, 2, 0, 1);

  label = manage(new Gtk::Label(_("Name: ")));
  table->attach(*label, 0, 1, 1, 2, Gtk::FILL);

  fileNameEntry_ = manage(new Gtk::Entry);
  table->attach(*fileNameEntry_, 1, 2, 1, 2);
}

void RecordHDTarget::start()
{
  active_ = true;
  update(UPD_ALL);
  show();
}

void RecordHDTarget::stop()
{
  if (active_) {
    hide();
    active_ = false;
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

std::string RecordHDTarget::getFilename()
{
  return fileNameEntry_->get_text();
}

std::string RecordHDTarget::getPath()
{
  return dirEntry_->get_filename();
}


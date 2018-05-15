/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <gtkmm.h>
#include <gnome.h>

#include "RecordTocSource.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "DeviceList.h"

#include "util.h"

RecordTocSource::RecordTocSource(TocEdit *tocEdit)
{
  Gtk::Table *table;
  Gtk::Label *label;  

  active_ = false;
  tocEdit_ = tocEdit;

  // device settings
  Gtk::Frame *infoFrame = manage(new Gtk::Frame(_(" Project Information ")));
  
  table = manage(new Gtk::Table(5, 2, FALSE));
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  table->set_border_width(10);

  label = manage(new Gtk::Label(_("Project name: "), 1));
  table->attach(*label, 0, 1, 0, 1);
  table->attach(projectLabel_, 1, 2, 0, 1);

  label = manage(new Gtk::Label(_("Toc Type: "), 1));
  table->attach(*label, 0, 1, 1, 2);
  table->attach(tocTypeLabel_, 1, 2, 1, 2);

  label = manage(new Gtk::Label(_("Tracks: "), 1));
  table->attach(*label, 0, 1, 2, 3);
  table->attach(nofTracksLabel_, 1, 2, 2, 3);

  label = manage(new Gtk::Label(_("Length: "), 1));
  table->attach(*label, 0, 1, 3, 4);
  table->attach(tocLengthLabel_, 1, 2, 3, 4);

  infoFrame->add(*table);
  pack_start(*infoFrame, Gtk::PACK_SHRINK);
}

void RecordTocSource::start()
{
  active_ = true;
  update(UPD_ALL);
  show_all();
}

void RecordTocSource::stop()
{
  if (active_) {
    hide();
    active_ = false;
  }
}

void RecordTocSource::update(unsigned long level)
{
  update(level, tocEdit_);
}

void RecordTocSource::update(unsigned long level, TocEdit *tedit)
{
  if (!active_)
    return;

  if (tocEdit_ != tedit) {
    level = UPD_ALL;
    tocEdit_ = tedit;
  }

  if (tocEdit_ == NULL) {
    projectLabel_.set_text("");
    tocTypeLabel_.set_text("");
    nofTracksLabel_.set_text("");
    tocLengthLabel_.set_text("");
  }
  else {
    if (level & UPD_TOC_DATA) {
      char label[256];
      char buf[50];
      const Toc *toc = tocEdit_->toc();

      projectLabel_.set_text(tocEdit_->filename());

      tocTypeLabel_.set_text(toc->tocType2String(toc->tocType()));

      sprintf(label, "%d", toc->nofTracks());
      nofTracksLabel_.set_text(label);
      
      sprintf(buf, "%d:%02d:%02d", toc->length().min(),
	      toc->length().sec(), toc->length().frac());
      tocLengthLabel_.set_text(buf);
    }
  }
}

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

#include "RecordTocSource.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "DeviceList.h"

#include "util.h"

RecordTocSource::RecordTocSource()
{
  Gtk::VBox *vbox;
  Gtk::Label *label;
  
  active_ = 0;
  tocEdit_ = NULL;

  // device settings
  Gtk::Frame *infoFrame = new Gtk::Frame(string("Toc Information"));
  infoFrame->show();
  
  vbox = new Gtk::VBox;

  projectLabel_ = new Gtk::Label(string("Project name: "));
  projectLabel_->show();

  vbox->pack_start(*projectLabel_, FALSE, FALSE, 5);

  label = new Gtk::Label(string("More info here..."));
  label->show();  
  vbox->pack_start(*label, FALSE, FALSE, 5);

  infoFrame->add(*vbox);
  vbox->show();

  pack_start(*infoFrame);

//  show();
}

RecordTocSource::~RecordTocSource()
{
}

void RecordTocSource::start(TocEdit *tocEdit)
{
  active_ = 1;

//  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void RecordTocSource::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordTocSource::update(unsigned long level, TocEdit *tocEdit)
{
  char label[256];
  
  if (!active_)
    return;

  tocEdit_ = tocEdit;

  sprintf(label, "Project name: %s", tocEdit_->filename());
  projectLabel_->set(string(label));
  
//  if (level & UPD_CD_DEVICES)
//    DEVICES->import();
//  else if (level & UPD_CD_DEVICE_STATUS)
//    DEVICES->importStatus();
}

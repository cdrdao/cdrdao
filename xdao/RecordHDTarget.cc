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
#include "RecordCDSource.h"
#include "RecordTocSource.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "DeviceList.h"

#include "util.h"


RecordHDTarget::RecordHDTarget()
{
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Label *label;
  Gtk::Adjustment *adjustment;

  active_ = 0;
  tocEdit_ = NULL;

  fileNameEntry_ = new Gtk::Entry;

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  // device settings
  Gtk::Frame *recordOptionsFrame = new Gtk::Frame(string("Record Options"));

  table = new Gtk::Table(5, 3, FALSE);
  table->set_row_spacings(2);
  table->set_col_spacings(30);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, FALSE, FALSE, 5);
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  recordOptionsFrame->add(*vbox);
  vbox->show();
  hbox->show();
  table->show();
  
  contents->pack_start(*recordOptionsFrame, FALSE, FALSE);
  recordOptionsFrame->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Name: "));
  hbox->pack_start(*label, FALSE, FALSE);
  label->show();

  hbox->pack_start(*fileNameEntry_, FALSE, FALSE);
  fileNameEntry_->show();
  vbox->pack_start(*hbox, FALSE);
  hbox->show();

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents);
  contents->show();

  pack_start(*contentsHBox);
  contentsHBox->show();

//  show();
}

RecordHDTarget::~RecordHDTarget()
{
}


void RecordHDTarget::start(TocEdit *tocEdit)
{
  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void RecordHDTarget::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordHDTarget::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  tocEdit_ = tocEdit;

//  if (level & UPD_CD_DEVICES)
//    DEVICES->import();
//  else if (level & UPD_CD_DEVICE_STATUS)
//    DEVICES->importStatus();
}

void RecordHDTarget::cancelAction()
{
  stop();
}

void RecordHDTarget::startAction(RecordSourceType source,
		RecordTocSource *TOC, RecordCDSource *CD)
{
  int eject, simulate, speed, multiSession, reload;
  int started = 0;
  Toc *toc;
  string temp;
  char *fileName;
  char *buffer;
  int correction;
  unsigned int i;

//  if (tocEdit_ == NULL)
//    return;

  if (CD->DEVICES->selection().empty()) {
    MessageBox msg(parent, "Dump CD", 0, 
		   "Please select one reader device.", NULL);
    msg.run();
    return;
  }

  temp = fileNameEntry_->get_text();
  if (temp == "")
  {
    MessageBox msg(parent, "Dump CD", 0, 
		   "Please write a name for the image.", NULL);
    msg.run();
    return;
  }
  else
    fileName = strdup(temp.c_str());

//FIXME: This only tests current directory, user should be able to select
//       the directory from a gtk_file_selection dialog, so this should test
//       in the specified directory.

  buffer = g_strdup_printf("%s%s.toc", "./", fileName);

  if (access(buffer, R_OK) != -1) 
  {

    Ask2Box msg(parent, "Dump CD", 0, 1,
    		g_strdup_printf("The filename %s.toc already exists.",
    		fileName), "Do you want to OVERWRITE it?", "", NULL);

    switch (msg.run()) {
    case 1: // remove the file an continue
      if (unlink(buffer) == -1)
      {
        MessageBox msg(parent, "Dump CD", 0,
        	g_strdup_printf("Error deleting the file %s.toc", fileName),
		       NULL);
        msg.run();
        return;
      }
      break;
    default: // cancel
      return;
      break;
    }
  }
  free(buffer);


  correction = CD->getCorrection();

  Gtk::CList_Helpers::SelectionList selection = CD->DEVICES->selection();

  for (i = 0; i < selection.size(); i++) {
    DeviceList::DeviceData *data = (DeviceList::DeviceData*)selection[i].get_data();

    if (data != NULL) {
      CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

      if (dev != NULL) {
	if (dev->extractDao(fileName, correction) != 0) {
	  message(-2, "Cannot start reading.");
	}
	else {
	  started = 1;
	}
      }
    }
  }

  if (started)
    guiUpdate(UPD_CD_DEVICE_STATUS);
}

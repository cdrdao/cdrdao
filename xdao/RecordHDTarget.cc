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

  active_ = 0;
  tocEdit_ = NULL;

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  // device settings
  Gtk::Frame *recordOptionsFrame = new Gtk::Frame(string("Record Options"));

  table = new Gtk::Table(2, 2, FALSE);
  table->set_row_spacings(2);
  table->set_col_spacings(10);
  table->set_border_width(5);

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

  label = new Gtk::Label(string("Directory: "));
  label->show();
  table->attach(*label, 0, 1, 0, 1);

  dirEntry_ = new Gnome::FileEntry("record_hd_target_dir_entry",
				   "Select Directory for Image");
  dirEntry_->set_directory(TRUE);
  dirEntry_->show();

  table->attach(*dirEntry_, 1, 2, 0, 1);


  label = new Gtk::Label(string("Name: "));
  label->show();
  table->attach(*label, 0, 1, 1, 2);

  fileNameEntry_ = new Gtk::Entry;
  fileNameEntry_->show();
  table->attach(*fileNameEntry_, 1, 2, 1, 2);

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents);
  contents->show();

  pack_start(*contentsHBox);
  contentsHBox->show();
}

RecordHDTarget::~RecordHDTarget()
{
}


void RecordHDTarget::start(TocEdit *tedit)
{
  active_ = 1;

  update(UPD_CD_DEVICES, tedit);

  show();
}

void RecordHDTarget::stop()
{
  if (active_) {
    hide();
    active_ = 0;
    tocEdit_ = NULL;
  }
}

void RecordHDTarget::update(unsigned long level, TocEdit *tedit)
{
  if (!active_)
    return;

  if (tocEdit_ != tedit) {
    tocEdit_ = tedit;
  }

//  if (level & UPD_CD_DEVICES)
//    DEVICES->import();
//  else if (level & UPD_CD_DEVICE_STATUS)
//    DEVICES->importStatus();
}

void RecordHDTarget::cancelAction()
{
  stop();
}

void RecordHDTarget::startAction(RecordGenericDialog::RecordSourceType source,
				 RecordTocSource *TOC, RecordCDSource *CD)
{
  int started = 0;
  Gtk::string imageName;
  Gtk::string imagePath;
  Gtk::string binPath;
  Gtk::string tocPath;
  char *tmp, *p;
  int correction;
  unsigned int i;

  if (CD->DEVICES->selection().empty()) {
    MessageBox msg(parent, "Dump CD", 0, 
		   "Please select a reader device.", NULL);
    msg.run();
    return;
  }

  imageName = fileNameEntry_->get_text();

  if (imageName == "")
  {
    MessageBox msg(parent, "Dump CD", 0, 
		   "Please specify a name for the image.", NULL);
    msg.run();
    return;
  }

  tmp = strdupCC(imageName.c_str());

  if ((p = strrchr(tmp, '.')) != NULL && strcmp(p, ".toc") == 0)
    *p = 0;

  if (*tmp == 0 || strcmp(tmp, ".") == 0 || strcmp(tmp, "..") == 0) {
    MessageBox msg(parent, "Dump CD", 0, 
		   "The specified image name is invalid.", NULL);
    msg.run();

    delete[] tmp;
    return;
  }

  imageName = tmp;
  delete[] tmp;

  {  
    Gtk::Entry *entry = static_cast<Gtk::Entry *>(dirEntry_->gtk_entry());
    Gtk::string path = entry->get_text();
    const char *s = path.c_str();
    long len = strlen(s);

    if (len == 0) {
      imagePath = imageName;
    }
    else {
      imagePath = path;

      if (s[len - 1] != '/')
	imagePath += "/";

      imagePath += imageName;
    }
  }
  
  binPath = imagePath;
  binPath += ".bin";

  tocPath = imagePath;
  tocPath += ".toc";

  if (access(binPath.c_str(), R_OK) == 0) {
    Gtk::string s = "The image file \"";
    s += binPath;
    s += "\" already exists.";

    Ask2Box msg(parent, "Dump CD", 0, 1, s.c_str(),
    		"Do you want to overwrite it?", "", NULL);

    if (msg.run() != 1) 
      return;
  }

  if (access(tocPath.c_str(), R_OK) == 0) 
  {
    Gtk::string s = "The toc-file \"";
    s += tocPath;
    s += "\" already exists.";

    Ask2Box msg(parent, "Dump CD", 0, 1, s.c_str(),
    		"Do you want to overwrite it?", "", NULL);

    switch (msg.run()) {
    case 1: // remove the file an continue
      if (unlink(tocPath.c_str()) != -0)
      {
        MessageBox msg(parent, "Dump CD", 0,
		       "Cannot delete toc-file", tocPath.c_str(), NULL);
        msg.run();
        return;
      }
      break;
    default: // cancel
      return;
      break;
    }
  }

  correction = CD->getCorrection();

  Gtk::CList_Helpers::SelectionList selection = CD->DEVICES->selection();

  for (i = 0; i < selection.size(); i++) {
    DeviceList::DeviceData *data = (DeviceList::DeviceData*)selection[i].get_data();

    if (data != NULL) {
      CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

      if (dev != NULL) {
	if (dev->extractDao(imagePath.c_str(), correction) != 0) {
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

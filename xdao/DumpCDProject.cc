/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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

#include "guiUpdate.h"
#include "Project.h"
#include "DumpCDProject.h"
#include "RecordCDSource.h"
#include "RecordHDTarget.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "Settings.h"
#include "util.h"
#include <gnome.h>

DumpCDProject::DumpCDProject()
{
  set_title("Dump CD to disk");

  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  vbox->show();
  Gtk::HBox *hbox = new Gtk::HBox;
  hbox->set_spacing(10);
  hbox->show();
  vbox->pack_start(*hbox, false, false);
  set_contents(*vbox);

  // menu stuff
  miSave_->set_sensitive(false);
  miSaveAs_->set_sensitive(false);
  miEditTree_->hide();
  miRecord_->set_sensitive(false);
  tiSave_->set_sensitive(false);
  tiRecord_->hide();

  CDSource = new RecordCDSource(this);
  CDSource->onTheFlyOption(false);
  CDSource->start();
  HDTarget = new RecordHDTarget();
  HDTarget->start();

  hbox->pack_start(*CDSource);
  CDSource->show();
  hbox->pack_start(*HDTarget);
  HDTarget->show();

  hbox = new Gtk::HBox;
  hbox->set_spacing(10);
  hbox->show();

  Gtk::VBox *frameBox = new Gtk::VBox;
  frameBox->show();
  hbox->pack_start(*frameBox, true, false);

  Gnome::Pixmap *pixmap =
  	manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_dumpcd.png")));
  Gtk::Label *startLabel = manage(new Gtk::Label("      Start      "));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->clicked.connect(slot(this, &DumpCDProject::start));
  pixmap->show();
  startLabel->show();
  startBox->show();
  button->show();

  hbox->pack_start(*button, true, false);

  Gtk::HBox *hbox2 = new Gtk::HBox;
  hbox2->show();
  hbox2->pack_start(*hbox, true, false);
  vbox->pack_start(*hbox2, true, false);

  install_menu_hints();

  guiUpdate(UPD_ALL);
}

DumpCDProject::~DumpCDProject()
{
  delete CDSource;
  delete HDTarget;
}

void DumpCDProject::start()
{
  DeviceList *sourceList = CDSource->getDeviceList();

  if (sourceList->selection().empty()) {
    Gnome::Dialogs::ok(*this, "Please select one reader device");
    return;
  }

  //Read options
  int correction = CDSource->getCorrection();

  Gtk::string imageName = HDTarget->getFilename();

  if (imageName == "")
  {
//FIXME: Allow for a temporary file?
    Gnome::Dialogs::ok(*this, "Please specify a name for the image");
    return;
  }

  char *tmp, *p;
  tmp = strdupCC(imageName.c_str());

  if ((p = strrchr(tmp, '.')) != NULL && strcmp(p, ".toc") == 0)
    *p = 0;

  if (*tmp == 0 || strcmp(tmp, ".") == 0 || strcmp(tmp, "..") == 0) {
    Gnome::Dialogs::error(*this, "The specified image name is invalid");
    delete[] tmp;
    return;
  }

  imageName = tmp;
  delete[] tmp;

  Gtk::string imagePath;
  Gtk::string binPath;
  Gtk::string tocPath;
  {  
    Gtk::string path = HDTarget->getPath();
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

    Ask2Box msg(this, "Dump CD", 0, 1, s.c_str(),
    		"Do you want to overwrite it?", "", NULL);

    if (msg.run() != 1) 
      return;
  }

  if (access(tocPath.c_str(), R_OK) == 0) 
  {
    Gtk::string s = "The toc-file \"";
    s += tocPath;
    s += "\" already exists.";

    Ask2Box msg(this, "Dump CD", 0, 1, s.c_str(),
    		"Do you want to overwrite it?", "", NULL);

    switch (msg.run()) {
    case 1: // remove the file an continue
      if (unlink(tocPath.c_str()) != -0)
      {
        MessageBox msg(this, "Dump CD", 0,
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

  DeviceList::DeviceData *sourceData =
      (DeviceList::DeviceData*) sourceList->selection()[0].get_data();

  if (sourceData == NULL)
    return;

  CdDevice *readDevice = CdDevice::find(sourceData->bus, sourceData->id, sourceData->lun);

  if (readDevice == NULL)
    return;

  if (readDevice->extractDao(imagePath.c_str(), correction) != 0)
    Gnome::Dialogs::error(*this, "Cannot start reading");
  else
    guiUpdate(UPD_CD_DEVICE_STATUS);
}

bool DumpCDProject::closeProject()
{
  return true;  // Close the project
}

void DumpCDProject::update(unsigned long level)
{
  CDSource->update(level);
  HDTarget->update(level);

  if (level & UPD_CD_DEVICE_STATUS)
    CDSource->getDeviceList()->selectOne();
}


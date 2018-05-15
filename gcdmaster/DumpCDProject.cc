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
#include "Icons.h"

#include <gtkmm.h>
#include <gnome.h>

DumpCDProject::DumpCDProject(Gtk::Window *parent)
    : Project(parent)
{
  // Top vbox
  Gtk::VBox* top_vbox = manage(new Gtk::VBox);
  top_vbox->set_border_width(10);
  top_vbox->set_spacing(10);
  parent_ = parent;

  CDSource = manage(new RecordCDSource(parent_));
  CDSource->onTheFlyOption(false);
  CDSource->start();
  top_vbox->pack_start(*CDSource);

  HDTarget = manage(new RecordHDTarget());
  HDTarget->start();
  top_vbox->pack_start(*HDTarget, Gtk::PACK_SHRINK);

  Gtk::HButtonBox* bbox = manage(new Gtk::HButtonBox);
  bbox->set_spacing(10);

  Gtk::Image *pixmap = manage(new Gtk::Image(Icons::DUMPCD,
                                             Gtk::ICON_SIZE_DIALOG));
  Gtk::Label *startLabel = manage(new Gtk::Label(_("Start")));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);
  button->add(*startBox);
  button->signal_clicked().connect(mem_fun(*this, &DumpCDProject::start));
  bbox->pack_start(*button, Gtk::PACK_EXPAND_PADDING);
  top_vbox->pack_start(*bbox, Gtk::PACK_SHRINK);

  pack_start(*top_vbox);
  guiUpdate(UPD_ALL);
  show_all();
}

DumpCDProject::~DumpCDProject()
{
}

void DumpCDProject::start()
{
  DeviceList *sourceList = CDSource->getDeviceList();

  if (sourceList->selection().empty()) {
    Gtk::MessageDialog d(*parent_, _("Please select one reader device"),
                         Gtk::MESSAGE_INFO);
    d.run();
    return;
  }

  // Read options
  int correction = CDSource->getCorrection();
  int subChanReadMode = CDSource->getSubChanReadMode();

  std::string imageName = HDTarget->getFilename();

  if (imageName == "") {
    Gtk::MessageDialog d(*parent_, _("Please specify a name for the image"),
                       Gtk::MESSAGE_INFO);
    d.run();
    return;
  }

  char *tmp, *p;
  tmp = strdupCC(imageName.c_str());

  if ((p = strrchr(tmp, '.')) != NULL && strcmp(p, ".toc") == 0)
    *p = 0;

  if (*tmp == 0 || strcmp(tmp, ".") == 0 || strcmp(tmp, "..") == 0) {
    Gtk::MessageDialog d(*parent_, _("The specified image name is invalid"),
                         Gtk::MESSAGE_ERROR);
    d.run();
    delete[] tmp;
    return;
  }

  imageName = tmp;
  delete[] tmp;

  std::string imagePath;
  std::string binPath;
  std::string tocPath;
  {  
    std::string path = HDTarget->getPath();
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
    std::string s = _("The image file \"");
    s += binPath;
    s += _("\" already exists.");

    Ask2Box msg(parent_, _("Dump CD"), 0, 1, s.c_str(),
    		_("Do you want to overwrite it?"), "", NULL);

    if (msg.run() != 1) 
      return;
  }

  if (access(tocPath.c_str(), R_OK) == 0) 
  {
    std::string s = _("The toc-file \"");
    s += tocPath;
    s += _("\" already exists.");

    Ask2Box msg(parent_, _("Dump CD"), 0, 1, s.c_str(),
    		_("Do you want to overwrite it?"), "", NULL);

    switch (msg.run()) {
    case 1: // remove the file an continue
      if (unlink(tocPath.c_str()) != -0)
      {
        MessageBox msg(parent_, _("Dump CD"), 0,
		       _("Cannot delete toc-file"), tocPath.c_str(), NULL);
        msg.run();
        return;
      }
      break;
    default: // cancel
      return;
      break;
    }
  }

  std::string sourceData = sourceList->selection();

  if (sourceData.empty())
    return;

  CdDevice *readDevice = CdDevice::find(sourceData.c_str());

  if (readDevice == NULL)
    return;

  if (readDevice->extractDao(*parent_, imagePath.c_str(), correction,
                             subChanReadMode)
      != 0) {
    Gtk::MessageDialog d(*parent_, _("Cannot start reading"), Gtk::MESSAGE_ERROR);
    d.run();
  } else {
    guiUpdate(UPD_CD_DEVICE_STATUS);
  }
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

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
#include "DuplicateCDProject.h"
#include "RecordCDSource.h"
#include "RecordCDTarget.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "Settings.h"
#include <gnome.h>

DuplicateCDProject::DuplicateCDProject()
{
  set_title("Duplicate CD");

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
  CDSource->start();
  CDTarget = new RecordCDTarget(this);
  CDTarget->start();

  hbox->pack_start(*CDSource);
  CDSource->show();
  hbox->pack_start(*CDTarget);
  CDTarget->show();

  hbox = new Gtk::HBox;
  hbox->set_spacing(10);
  hbox->show();

  Gtk::VBox *frameBox = new Gtk::VBox;
  frameBox->show();
  simulate_rb = new Gtk::RadioButton("Simulate", 0);
  simulateBurn_rb = new Gtk::RadioButton("Simulate and Burn", 0);
  burn_rb = new Gtk::RadioButton("Burn", 0);

  frameBox->pack_start(*simulate_rb);
  simulate_rb->show();
  frameBox->pack_start(*simulateBurn_rb);
//  simulateBurn_rb->show();
  simulateBurn_rb->set_group(simulate_rb->group());
  frameBox->pack_start(*burn_rb);
  burn_rb->show();
  burn_rb->set_group(simulate_rb->group());

  hbox->pack_start(*frameBox, true, false);

  Gnome::Pixmap *pixmap =
  	manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/gcdmaster.png")));
  Gtk::Label *startLabel = manage(new Gtk::Label("      Start      "));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->clicked.connect(slot(this, &DuplicateCDProject::start));
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

DuplicateCDProject::~DuplicateCDProject()
{
  delete CDSource;
  delete CDTarget;
}

void DuplicateCDProject::start()
{
  DeviceList *sourceList = CDSource->getDeviceList();
  DeviceList *targetList = CDTarget->getDeviceList();

  if (sourceList->selection().empty()) {
    Gnome::Dialogs::ok(*this, "Please select one reader device");
    return;
  }

  if (targetList->selection().empty()) {
    Gnome::Dialogs::ok(*this, "Please select at least one recorder device");
    return;
  }

  //Read options
  int onTheFly = CDSource->getOnTheFly();
  if (onTheFly)
  {
    // We can't make on the fly copy with the same device, check that
    Gtk::CList_Helpers::SelectionList sourceSelection = sourceList->selection();
    Gtk::CList_Helpers::SelectionList targetSelection = targetList->selection();

    // We can only have one source device selected
    DeviceList::DeviceData *sourceData = (DeviceList::DeviceData*)sourceSelection[0].get_data();

    for (int i = 0; i < targetSelection.size(); i++) {
      DeviceList::DeviceData *targetData = (DeviceList::DeviceData*)targetSelection[i].get_data();
      if ((targetData->bus == sourceData->bus)
       && (targetData->id == sourceData->id))
      {
        // If the user selects the same device for reading and writing
        // we can't do on the fly copying. More complex situations with
        // multiple target devices are not handled
        if (gnome_config_get_bool(SET_DUPLICATE_ONTHEFLY_WARNING)) {
          Ask2Box msg(this, "Request", 1, 2, 
    		  "To duplicate a CD using the same device for reading and writing",
    		  "you need to copy the CD to disk before burning", "",
    	  	"Proceed and copy to disk before burning?", NULL);

          switch (msg.run()) {
          case 1: // proceed without on the fly
            CDSource->setOnTheFly(false);
            onTheFly = 0;
            if (msg.dontShowAgain())
            {
          	gnome_config_set_bool(SET_DUPLICATE_ONTHEFLY_WARNING, FALSE);
        	  gnome_config_sync();
            }
            break;
          default: // do not proceed
            return;
            break;
          }
        }
        else
        {
          CDSource->setOnTheFly(false);
          onTheFly = 0;
        }
      }
    }
  }

  int correction = CDSource->getCorrection();
  int subChanReadMode = CDSource->getSubChanReadMode();

  //Record options
  int simulate;
  if (simulate_rb->get_active())
    simulate = 1;
  else if (simulateBurn_rb->get_active())
    simulate = 2;
  else
    simulate = 0;

  int multiSession = CDTarget->getMultisession();
  int burnSpeed = CDTarget->getSpeed();
  int eject = CDTarget->checkEjectWarning(this);
  if (eject == -1)
    return;

  int reload = CDTarget->checkReloadWarning(this);
  if (reload == -1)
    return;

  int buffer = CDTarget->getBuffer();

  DeviceList::DeviceData *sourceData =
      (DeviceList::DeviceData*) sourceList->selection()[0].get_data();

  if (sourceData == NULL)
    return;

  CdDevice *readDevice = CdDevice::find(sourceData->bus, sourceData->id, sourceData->lun);

  if (readDevice == NULL)
    return;

  Gtk::CList_Helpers::SelectionList targetSelection = targetList->selection();

  for (int i = 0; i < targetSelection.size(); i++) {
    DeviceList::DeviceData *targetData =
        (DeviceList::DeviceData*)targetSelection[i].get_data();

    if (targetData == NULL)
      break;
  
    CdDevice *writeDevice = CdDevice::find(targetData->bus, targetData->id, targetData->lun);
  
    if (writeDevice == NULL)
      break;
  
    if (writeDevice->duplicateDao(simulate, multiSession, burnSpeed,
				  eject, reload, buffer, onTheFly, correction,
				  subChanReadMode, readDevice) != 0)
      Gnome::Dialogs::error(*this, "Cannot start disk-at-once duplication");
    else
      guiUpdate(UPD_CD_DEVICE_STATUS);
  }
}

bool DuplicateCDProject::closeProject()
{
  return true;  // Close the project
}

void DuplicateCDProject::update(unsigned long level)
{
  CDSource->update(level);
  CDTarget->update(level);

  if (level & UPD_CD_DEVICE_STATUS)
  {
    DeviceList *sourceList = CDSource->getDeviceList();
    DeviceList *targetList = CDTarget->getDeviceList();
  
    targetList->selectOne();
  
    Gtk::CList_Helpers::SelectionList targetSelection = targetList->selection();
  
    if (targetSelection.empty())
      sourceList->selectOne();
    else
      sourceList->selectOneBut(targetSelection);
  }
}

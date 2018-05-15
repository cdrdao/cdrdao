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
#include "Icons.h"

#include <gtkmm.h>
#include <gnome.h>

DuplicateCDProject::DuplicateCDProject(Gtk::Window *parent)
    : Project(parent)
{
  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  Gtk::HBox *hbox = manage(new Gtk::HBox);
  hbox->set_spacing(10);
  vbox->pack_start(*hbox);
  pack_start(*vbox);
  parent_ = parent;

  CDSource = new RecordCDSource(parent_);
  CDSource->start();
  CDTarget = new RecordCDTarget(parent_);
  CDTarget->start();

  hbox->pack_start(*CDSource);
  hbox->pack_start(*CDTarget);

  hbox = manage(new Gtk::HBox);
  hbox->set_spacing(10);

  Gtk::VBox *frameBox = new Gtk::VBox;
  simulate_rb = new Gtk::RadioButton(_("Simulate"), 0);
  simulateBurn_rb = new Gtk::RadioButton(_("Simulate and Burn"), 0);
  burn_rb = new Gtk::RadioButton(_("Burn"), 0);

  frameBox->pack_start(*simulate_rb);
  frameBox->pack_start(*simulateBurn_rb);
  Gtk::RadioButton::Group rb_group = simulate_rb->get_group();
  simulateBurn_rb->set_group(rb_group);
  frameBox->pack_start(*burn_rb);
  burn_rb->set_group(rb_group);

  hbox->pack_start(*frameBox, true, false);

  Gtk::Image *pixmap = manage(new Gtk::Image(Icons::GCDMASTER,
                                             Gtk::ICON_SIZE_DIALOG));
  Gtk::Label *startLabel = manage(new Gtk::Label(_("Start")));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->signal_clicked().connect(mem_fun(*this, &DuplicateCDProject::start));

  hbox->pack_start(*button, true, false);

  Gtk::HBox *hbox2 = new Gtk::HBox;
  hbox2->pack_start(*hbox, true, false);
  vbox->pack_start(*hbox2, Gtk::PACK_SHRINK);

  guiUpdate(UPD_ALL);
  show_all();
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

  std::string sourceData = sourceList->selection();
  std::string targetData = targetList->selection();

  if (sourceData.empty()) {
    Gtk::MessageDialog d(*parent_, _("Please select one reader device"),
                           Gtk::MESSAGE_INFO);
      d.run();
      return;
  }

  if (targetData.empty()) {
    Gtk::MessageDialog d(*parent_,
                         _("Please select at least one recorder device"),
                           Gtk::MESSAGE_INFO);
      d.run();
      return;
  }

  //Read options
  int onTheFly = CDSource->getOnTheFly();
  if (onTheFly) {
    // We can't make on the fly copy with the same device, check that
    // We can only have one source device selected

    if (sourceData == targetData) {

      // If the user selects the same device for reading and writing
      // we can't do on the fly copying. More complex situations with
      // multiple target devices are not handled
      if (gnome_config_get_bool(SET_DUPLICATE_ONTHEFLY_WARNING)) {
        Ask2Box msg(parent_, "Request", 1, 2,
                    _("To duplicate a CD using the same device for reading "
                      "and writing"),
                    _("you need to copy the CD to disk before burning"), "",
                    _("Proceed and copy to disk before burning?"), NULL);

        switch (msg.run()) {
        case 1: // proceed without on the fly
          CDSource->setOnTheFly(false);
          onTheFly = 0;
          if (msg.dontShowAgain()) {
            gnome_config_set_bool(SET_DUPLICATE_ONTHEFLY_WARNING, FALSE);
            gnome_config_sync();
          }
          break;
        default: // do not proceed
          return;
          break;
        }
      } else {
        CDSource->setOnTheFly(false);
        onTheFly = 0;
      }
    }
  }

  int correction = CDSource->getCorrection();
  int subChanReadMode = CDSource->getSubChanReadMode();

  // Record options
  int simulate;
  if (simulate_rb->get_active())
    simulate = 1;
  else if (simulateBurn_rb->get_active())
    simulate = 2;
  else
    simulate = 0;

  int multiSession = CDTarget->getMultisession();
  int burnSpeed = CDTarget->getSpeed();
  int eject = CDTarget->checkEjectWarning(parent_);
  if (eject == -1)
    return;

  int reload = CDTarget->checkReloadWarning(parent_);
  if (reload == -1)
    return;

  int buffer = CDTarget->getBuffer();

  CdDevice *readDevice = CdDevice::find(sourceData.c_str());
  if (readDevice == NULL)
    return;

  CdDevice *writeDevice = CdDevice::find(targetData.c_str());
  if (writeDevice == NULL)
    return;
  
  if (writeDevice->duplicateDao(*parent_, simulate, multiSession, burnSpeed,
                                eject, reload, buffer, onTheFly, correction,
                                subChanReadMode, readDevice) != 0) {
    Gtk::MessageDialog md(*parent_, _("Cannot start disk-at-once duplication"),
                          Gtk::MESSAGE_ERROR);
    md.run();
  } else {
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

    if (targetList->selection().empty()) {
      sourceList->selectOne();
    } else {
      sourceList->selectOneBut(targetList->selection().c_str());
    }
  }
}

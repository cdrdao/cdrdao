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

#include <gnome--.h>

#include "guiUpdate.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "BlankCDDialog.h"
#include "Settings.h"

#include <gnome.h>

BlankCDDialog::BlankCDDialog()
{
  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  vbox->show();
  add(*vbox);

  active_ = 0;
  moreOptionsDialog_ = 0;
  speed_ = 1;

  Devices = new DeviceList(CdDevice::CD_RW);
  vbox->pack_start(*Devices, false, false);

  // device settings
  Gtk::Frame *blankOptionsFrame = new Gtk::Frame(string("Blank Options"));
  Gtk::VBox *frameBox = new Gtk::VBox;
  frameBox->set_border_width(5);
  frameBox->set_spacing(5);
  frameBox->show();
  blankOptionsFrame->add(*frameBox);

  fastBlank_rb = new Gtk::RadioButton("Fast Blank - does not erase contents", 0);
  fullBlank_rb = new Gtk::RadioButton("Full Blank - erases contents, slower", 0);

  frameBox->pack_start(*fastBlank_rb);
  fastBlank_rb->show();
  frameBox->pack_start(*fullBlank_rb);
  fullBlank_rb->show();
  fullBlank_rb->set_group(fastBlank_rb->group());

  Gnome::StockPixmap *moreOptionsPixmap =
  	manage(new Gnome::StockPixmap(GNOME_STOCK_MENU_PROP));
  Gtk::Label *moreOptionsLabel = manage(new Gtk::Label("More Options"));
  Gtk::HBox *moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->set_border_width(2);
  Gtk::Button *moreOptionsButton = manage(new Gtk::Button());
  moreOptionsBox->pack_start(*moreOptionsPixmap, false, false, 3);
  moreOptionsBox->pack_start(*moreOptionsLabel, false, false, 4);
  moreOptionsButton->add(*moreOptionsBox);
  moreOptionsButton->clicked.connect(slot(this, &BlankCDDialog::moreOptions));
  moreOptionsPixmap->show();
  moreOptionsLabel->show();
  moreOptionsBox->show();
  moreOptionsButton->show();
  moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->show();
  frameBox->pack_start(*moreOptionsBox);
  moreOptionsBox->pack_end(*moreOptionsButton, false, false);

  vbox->pack_start(*blankOptionsFrame, false, false);
  blankOptionsFrame->show();

  Gnome::Pixmap *pixmap =
  	manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/gcdmaster.png")));
  Gtk::Label *startLabel = manage(new Gtk::Label("      Start      "));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->clicked.connect(slot(this, &BlankCDDialog::startAction));
  pixmap->show();
  startLabel->show();
  startBox->show();
  button->show();

  Gtk::HBox *hbox2 = new Gtk::HBox;
  hbox2->show();
  hbox2->pack_start(*button, true, false);
  vbox->pack_start(*hbox2, true, false);
}

void BlankCDDialog::moreOptions()
{
  if (!moreOptionsDialog_)
  {
    vector <string> buttons;
    buttons.push_back(GNOME_STOCK_BUTTON_CLOSE);
    moreOptionsDialog_ = new Gnome::Dialog("Blank options", buttons);

    moreOptionsDialog_->set_parent(*this);
    moreOptionsDialog_->set_close(true);

    Gtk::VBox *vbox = moreOptionsDialog_->get_vbox();
    Gtk::Frame *frame = new Gtk::Frame("More Blank Options");
    vbox->pack_start(*frame);
    vbox = new Gtk::VBox;
    vbox->set_border_width(10);
    vbox->set_spacing(5);
    vbox->show();
    frame->add(*vbox);
    frame->show();

    ejectButton_ = new Gtk::CheckButton(string("Eject the CD after blanking"), 0);
    ejectButton_->set_active(false);
    ejectButton_->show();
    vbox->pack_start(*ejectButton_);

    reloadButton_ = new Gtk::CheckButton(string("Reload the CD after writing, if necessary"), 0);
    reloadButton_->set_active(false);
    reloadButton_->show();
    vbox->pack_start(*reloadButton_);

    Gtk::HBox *hbox = new Gtk::HBox;
    hbox->show();
    Gtk::Label *label = new Gtk::Label(string("Speed: "), 0);
    label->show();
    hbox->pack_start(*label, false, false);
    
    Gtk::Adjustment *adjustment = new Gtk::Adjustment(1, 1, 20);
    speedSpinButton_ = new Gtk::SpinButton(*adjustment);
    speedSpinButton_->set_digits(0);
    speedSpinButton_->show();
    speedSpinButton_->set_sensitive(false);
    adjustment->value_changed.connect(SigC::slot(this, &BlankCDDialog::speedChanged));
    hbox->pack_start(*speedSpinButton_, false, false, 10);
  
    speedButton_ = new Gtk::CheckButton(string("Use max."), 0);
    speedButton_->set_active(true);
    speedButton_->show();
    speedButton_->toggled.connect(SigC::slot(this, &BlankCDDialog::speedButtonChanged));
    hbox->pack_start(*speedButton_, true, true);
    vbox->pack_start(*hbox);
  }

  moreOptionsDialog_->show();
}

void BlankCDDialog::start()
{
  if (active_) {
    get_window().raise();
  }

  active_ = 1;

  update(UPD_CD_DEVICES);

  show();
}

void BlankCDDialog::stop()
{
  if (active_) {
    hide();
    if (moreOptionsDialog_)
      moreOptionsDialog_->hide();
    active_ = 0;
  }
}

void BlankCDDialog::update(unsigned long level)
{
  if (!active_)
    return;

  set_title("Blank CD Rewritable");

  if (level & UPD_CD_DEVICES)
    Devices->import();
  else if (level & UPD_CD_DEVICE_STATUS)
    Devices->importStatus();

  Devices->selectOne();
}

gint BlankCDDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void BlankCDDialog::startAction()
{
  if (Devices->selection().empty()) {
    Gnome::Dialogs::ok(*this, "Please select at least one recorder device");
    return;
  }

  int fast;
  if (fastBlank_rb->get_active())
    fast = 1;
  else
    fast = 0;

  int eject = checkEjectWarning(this);
  int reload = checkReloadWarning(this);
  int burnSpeed = getSpeed();

  Gtk::CList_Helpers::SelectionList targetSelection = Devices->selection();

  for (int i = 0; i < targetSelection.size(); i++) {
    DeviceList::DeviceData *targetData =
        (DeviceList::DeviceData*)targetSelection[i].get_data();

    if (targetData == NULL)
      break;
  
    CdDevice *writeDevice = CdDevice::find(targetData->bus, targetData->id, targetData->lun);
  
    if (writeDevice == NULL)
      break;

    if (writeDevice->blank(fast, burnSpeed, eject, reload) != 0)
      Gnome::Dialogs::error(*this, "Cannot start blanking");
    else
      guiUpdate(UPD_CD_DEVICE_STATUS);
  }
  stop();
}

void BlankCDDialog::speedButtonChanged()
{
  if (speedButton_->get_active())
  {
    speedSpinButton_->set_sensitive(false);
  }
  else
  {
    speedSpinButton_->set_sensitive(true);
  }
}

void BlankCDDialog::speedChanged()
{
//FIXME: get max burn speed from selected burner(s)
  int new_speed;

  new_speed = speedSpinButton_->get_value_as_int();

  if ((new_speed % 2) == 1)
  {
    if (new_speed > 2)
    {
      if (new_speed > speed_)
      {
        new_speed = new_speed + 1;
      }
      else
      {
        new_speed = new_speed - 1;
      }
    }
    speedSpinButton_->set_value(new_speed);
  }
  speed_ = new_speed;
}

bool BlankCDDialog::getEject()
{
  if (moreOptionsDialog_)
    return ejectButton_->get_active() ? 1 : 0;
  else
    return 0; 
}

int BlankCDDialog::checkEjectWarning(Gtk::Window *parent)
{
  // If ejecting the CD after recording is requested issue a warning message
  // because buffer under runs may occur for other devices that are recording.
  if (getEject())
  {
    if (gnome_config_get_bool(SET_RECORD_EJECT_WARNING)) {
      Ask3Box msg(parent, "Request", 1, 2, 
    		"Ejecting a CD may block the SCSI bus and",
  		  "cause buffer under runs when other devices",
    		"are still recording.", "",
  	  	"Keep the eject setting anyway?", NULL);
  
      switch (msg.run()) {
      case 1: // keep eject setting
        if (msg.dontShowAgain())
        {
          gnome_config_set_bool(SET_RECORD_EJECT_WARNING, FALSE);
          gnome_config_sync();
        }
        return 1;
        break;
      case 2: // don't keep eject setting
        ejectButton_->set_active(false);
        return 0;
        break;
      default: // cancel
        return -1;
        break;
      }
    }
    return 1;
  }
  return 0;  
}

bool BlankCDDialog::getReload()
{
  if (moreOptionsDialog_)
    return reloadButton_->get_active() ? 1 : 0;
  else
    return 0; 
}

int BlankCDDialog::checkReloadWarning(Gtk::Window *parent)
{
  // The same is true for reloading the disk.
  if (getReload())
  {
    if (gnome_config_get_bool(SET_RECORD_RELOAD_WARNING)) {
      Ask3Box msg(parent, "Request", 1, 2, 
  		"Reloading a CD may block the SCSI bus and",
  		"cause buffer under runs when other devices",
  		"are still recording.", "",
  		"Keep the reload setting anyway?", NULL);

      switch (msg.run()) {
      case 1: // keep reload setting
        if (msg.dontShowAgain())
        {
      	gnome_config_set_bool(SET_RECORD_RELOAD_WARNING, FALSE);
          gnome_config_sync();
        }
        return 1;
        break;
      case 2: // don't keep reload setting
        reloadButton_->set_active(false);
        return 0;
        break;
      default: // cancel
        return -1;
        break;
      }
    }
    return 1;
  }
  return 0;
}

int BlankCDDialog::getSpeed()
{
  if (moreOptionsDialog_)
  {
   if (speedButton_->get_active())
     return 0;
   else
     return speed_;
  }
  return 0;
}

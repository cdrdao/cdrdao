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

#include <libgnomeuimm.h>

#include "guiUpdate.h"
#include "DeviceList.h"
#include "MessageBox.h"
#include "BlankCDDialog.h"
#include "Settings.h"
#include "Icons.h"

#include <gtkmm.h>
#include <gnome.h>

BlankCDDialog::BlankCDDialog()
{
  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(10);
  vbox->set_spacing(10);
  add(*vbox);

  active_ = false;
  moreOptionsDialog_ = 0;
  speed_ = 1;

  Devices = new DeviceList(CdDevice::CD_RW);
  vbox->pack_start(*Devices, true, true);

  // device settings
  Gtk::Frame *blankOptionsFrame = new Gtk::Frame(_(" Blank Options "));
  Gtk::VBox *frameBox = new Gtk::VBox;
  frameBox->set_border_width(5);
  frameBox->set_spacing(5);
  blankOptionsFrame->add(*frameBox);

  fastBlank_rb =
    new Gtk::RadioButton(_("Fast Blank - does not erase contents"), 0);
  fullBlank_rb =
    new Gtk::RadioButton(_("Full Blank - erases contents, slower"), 0);

  Gtk::RadioButton::Group rb_group = fastBlank_rb->get_group();
  fullBlank_rb->set_group(rb_group);

  frameBox->pack_start(*fastBlank_rb);
  frameBox->pack_start(*fullBlank_rb);

  Gtk::Image *moreOptionsPixmap =
      manage(new Gtk::Image(Gtk::StockID(Gtk::Stock::PROPERTIES),
                            Gtk::ICON_SIZE_SMALL_TOOLBAR));
  Gtk::Label *moreOptionsLabel = manage(new Gtk::Label(_("More Options")));
  Gtk::HBox *moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->set_border_width(2);
  Gtk::Button *moreOptionsButton = manage(new Gtk::Button());
  moreOptionsBox->pack_start(*moreOptionsPixmap, false, false, 3);
  moreOptionsBox->pack_start(*moreOptionsLabel, false, false, 4);
  moreOptionsButton->add(*moreOptionsBox);
  moreOptionsButton->signal_clicked().
    connect(mem_fun(*this, &BlankCDDialog::moreOptions));
  moreOptionsBox = manage(new Gtk::HBox);
  frameBox->pack_start(*moreOptionsBox);
  moreOptionsBox->pack_end(*moreOptionsButton, false, false);

  vbox->pack_start(*blankOptionsFrame, false, false);

  Gtk::Image *pixmap = manage(new Gtk::Image(Icons::GCDMASTER,
                                             Gtk::ICON_SIZE_DIALOG));
  Gtk::Label *startLabel = manage(new Gtk::Label(_("Start")));
  Gtk::VBox *startBox = manage(new Gtk::VBox);
  Gtk::Button *button = manage(new Gtk::Button());
  startBox->pack_start(*pixmap, false, false);
  startBox->pack_start(*startLabel, false, false);

  button->add(*startBox);
  button->signal_clicked().connect(mem_fun(*this, &BlankCDDialog::startAction));

  Gtk::HBox *hbox2 = manage(new Gtk::HBox);
  hbox2->set_spacing(20);
  hbox2->set_border_width(10);
  hbox2->pack_start(*button);

  Gtk::Button* cancel_but =
    manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CANCEL)));
  cancel_but->signal_clicked().connect(mem_fun(*this, &BlankCDDialog::stop));
  hbox2->pack_start(*cancel_but);
  vbox->pack_start(*hbox2, Gtk::PACK_SHRINK);

  show_all_children();
}

void BlankCDDialog::moreOptions()
{
  if (!moreOptionsDialog_) {
	  moreOptionsDialog_ = new Gtk::MessageDialog(*this, _("Blank options"), false, 
                                                Gtk::MESSAGE_QUESTION,
						      Gtk::BUTTONS_CLOSE, true);

    Gtk::VBox *vbox = moreOptionsDialog_->get_vbox();
    Gtk::Frame *frame = new Gtk::Frame(_(" More Blank Options "));
    vbox->pack_start(*frame);
    vbox = new Gtk::VBox;
    vbox->set_border_width(10);
    vbox->set_spacing(5);
    frame->add(*vbox);

    ejectButton_ = new Gtk::CheckButton(_("Eject the CD after blanking"), 0);
    ejectButton_->set_active(false);
    vbox->pack_start(*ejectButton_);

    reloadButton_ =
      new Gtk::CheckButton(_("Reload the CD after writing, if necessary"), 0);
    reloadButton_->set_active(false);
    vbox->pack_start(*reloadButton_);

    Gtk::HBox *hbox = new Gtk::HBox;
    Gtk::Label *label = new Gtk::Label(_("Speed: "), 0);
    hbox->pack_start(*label, false, false);
    
    Gtk::Adjustment *adjustment = new Gtk::Adjustment(1, 1, 20);
    speedSpinButton_ = new Gtk::SpinButton(*adjustment);
    speedSpinButton_->set_digits(0);
    speedSpinButton_->set_sensitive(false);
    adjustment->signal_value_changed().
      connect(mem_fun(*this, &BlankCDDialog::speedChanged));
    hbox->pack_start(*speedSpinButton_, false, false, 10);
  
    speedButton_ = new Gtk::CheckButton(_("Use max."), 0);
    speedButton_->set_active(true);
    speedButton_->signal_toggled().
      connect(mem_fun(*this, &BlankCDDialog::speedButtonChanged));
    hbox->pack_start(*speedButton_, true, true);
    vbox->pack_start(*hbox);
    moreOptionsDialog_->show_all_children();
  }

  moreOptionsDialog_->show();
  moreOptionsDialog_->run();
  moreOptionsDialog_->hide();
}

void BlankCDDialog::start(Gtk::Window& parent)
{
  present();
  active_ = true;
  parent_ = &parent;
  update(UPD_CD_DEVICES);
}

void BlankCDDialog::stop()
{
  hide();
  if (moreOptionsDialog_)
    moreOptionsDialog_->hide();
  active_ = false;
}

void BlankCDDialog::update(unsigned long level)
{
  if (!active_)
    return;

  set_title(_("Blank CD Rewritable"));

  if (level & UPD_CD_DEVICES)
    Devices->import();
  else if (level & UPD_CD_DEVICE_STATUS)
  {
    Devices->importStatus();
    Devices->selectOne();
  }
}

bool BlankCDDialog::on_delete_event(GdkEventAny*)
{
  stop();
  return 1;
}

void BlankCDDialog::startAction()
{
  if (Devices->selection().empty()) {
    Gtk::MessageDialog d(*this,
                         _("Please select at least one recorder device"),
                         Gtk::MESSAGE_WARNING);
    d.run();
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

  std::string targetData = Devices->selection();

  CdDevice *writeDevice = CdDevice::find(targetData.c_str());
  
  if (writeDevice) {
    if (writeDevice->blank(parent_, fast, burnSpeed, eject, reload) != 0) {
      Gtk::MessageDialog d(*this, _("Cannot start blanking"),
                           Gtk::MESSAGE_ERROR);
      d.run();
    } else
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
      Ask3Box msg(parent, _("Request"), 1, 2, 
                  _("Ejecting a CD may block the SCSI bus and"),
  		  _("cause buffer under runs when other devices"),
                  _("are still recording."), "",
                  _("Keep the eject setting anyway?"), NULL);
  
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
      Ask3Box msg(parent, _("Request"), 1, 2, 
                  _("Reloading a CD may block the SCSI bus and"),
                  _("cause buffer under runs when other devices"),
                  _("are still recording."), "",
                  _("Keep the reload setting anyway?"), NULL);

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

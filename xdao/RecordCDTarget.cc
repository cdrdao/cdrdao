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

#include "RecordCDTarget.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "DeviceList.h"

#include "util.h"

#define MAX_SPEED_ID 7

static RecordCDTarget::SpeedTable SPEED_TABLE[MAX_SPEED_ID + 1] = {
  { 0, "Max" },
  { 1, "1x" },
  { 2, "2x" },
  { 4, "4x" },
  { 6, "6x" },
  { 8, "8x" },
  { 10, "10x" },
  { 12, "12x" }
};


RecordCDTarget::RecordCDTarget()
{
  int i;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Label *label;
  Gtk::Adjustment *adjustment;

  active_ = 0;
  tocEdit_ = NULL;

//  set_title(string("Record"));
//  set_usize(0, 300);

  Gtk::Menu *menu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i <= MAX_SPEED_ID; i++) {
    mi = manage(new Gtk::MenuItem(SPEED_TABLE[i].name));
    mi->activate.connect(bind(slot(this, &RecordCDTarget::setSpeed), i));
    mi->show();
    menu->append(*mi);
  }

  speedMenu_ = new Gtk::OptionMenu;
  speedMenu_->set_menu(menu);

  speed_ = 0;
  speedMenu_->set_history(speed_);

//  startButton_ = new Gtk::Button(string(" Start "));

  Gtk::RadioButton_Helpers::Group simWriteGroup;

  simulateButton_ = new Gtk::RadioButton(simWriteGroup, string("Simulate"));
  writeButton_ = new Gtk::RadioButton(simWriteGroup, string("Write"));
  
  closeSessionButton_ = new Gtk::CheckButton(string("Close Disk"));
  closeSessionButton_->set_active(true);

  ejectButton_ = new Gtk::CheckButton(string("Eject"));
  ejectButton_->set_active(false);

  reloadButton_ = new Gtk::CheckButton(string("Reload"));
  reloadButton_->set_active(false);

  DEVICES = new DeviceList();

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  contents->pack_start(*DEVICES, TRUE, TRUE);

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
  
  hbox = new Gtk::HBox;
  hbox->pack_start(*simulateButton_, FALSE, FALSE);
  simulateButton_->show();
  table->attach(*hbox, 0, 1, 0, 1);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*writeButton_, FALSE, FALSE);
  writeButton_->show();
  table->attach(*hbox, 0, 1, 1, 2);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*closeSessionButton_, FALSE, FALSE);
  closeSessionButton_->show();
  table->attach(*hbox, 1, 2, 0, 1);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*ejectButton_, FALSE, FALSE);
  ejectButton_->show();
  table->attach(*hbox, 1, 2, 1, 2);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*reloadButton_, FALSE);
  reloadButton_->show();
  table->attach(*hbox, 1, 2, 2, 3);
  hbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Speed: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*speedMenu_, FALSE);
  speedMenu_->show();
  table->attach(*hbox, 0, 1, 2, 3);
  hbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Buffer: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  adjustment = new Gtk::Adjustment(10, 10, 1000);
  bufferSpinButton_ = new Gtk::SpinButton(*adjustment);
  hbox->pack_start(*bufferSpinButton_, FALSE);
  bufferSpinButton_->show();
  label = new Gtk::Label(string("audio seconds "));
  hbox->pack_start(*label, FALSE);
  label->show();
  bufferRAMLabel_ = new Gtk::Label(string("= 1.72 Mb buffer."));
  hbox->pack_start(*bufferRAMLabel_, FALSE);
  bufferRAMLabel_->show();
  adjustment->value_changed.connect(SigC::slot(this, &RecordCDTarget::updateBufferRAMLabel));
  table->attach(*hbox, 0, 2, 3, 4);
  hbox->show();
  
  contents->pack_start(*recordOptionsFrame, FALSE, FALSE);
  recordOptionsFrame->show();

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();

  pack_start(*contentsHBox, TRUE, TRUE, 10);
  contentsHBox->show();

  show();

}

RecordCDTarget::~RecordCDTarget()
{
}


void RecordCDTarget::start(TocEdit *tocEdit)
{
  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void RecordCDTarget::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordCDTarget::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  tocEdit_ = tocEdit;

  if (level & UPD_CD_DEVICES)
    DEVICES->import();
  else if (level & UPD_CD_DEVICE_STATUS)
    DEVICES->importStatus();
}

void RecordCDTarget::cancelAction()
{
  stop();
}

void RecordCDTarget::startAction()
{
  int eject, simulate, speed, multiSession, reload;
  int started = 0;
  Toc *toc;
  int buffer;
  unsigned int i;

  if (tocEdit_ == NULL)
    return;

  if (DEVICES->selection().empty()) {
    MessageBox msg(parent, "Record", 0, 
		   "Please select at least one recorder device.", NULL);
    msg.run();
    return;
  }

  toc = tocEdit_->toc();

  if (toc->nofTracks() == 0 || toc->length().lba() < 300) {
    MessageBox msg(parent, "Cannot Record", 0,
		   "Current toc contains no tracks or",
		   "length of at least one track is < 4 seconds.", NULL);
    msg.run();
    return;
  }

  switch (toc->checkCdTextData()) {
  case 0: // OK
    break;
  case 1: // warning
    {
      Ask2Box msg(parent, "CD-TEXT Inconsistency", 0, 2,
		  "Inconsistencies were detected in the defined CD-TEXT data",
		  "which may produce undefined results. See the console",
		  "output for more details.",
		  "",
		  "Do you want to proceed anyway?", NULL);

      if (msg.run() != 1)
	return;
    }
    break;
  default: // error
    {
      MessageBox msg(parent, "CD-TEXT Error", 0, 
		     "The defined CD-TEXT data is erroneous or incomplete.",
		     "See the console output for more details.", NULL);
      msg.run();
      return;
    }
    break;
  }

  simulate = writeButton_->get_active() ? 0 : 1;
  multiSession = closeSessionButton_->get_active() ? 0 : 1;
  speed = SPEED_TABLE[speed_].speed;
  eject = ejectButton_->get_active() ? 1 : 0;
  reload = reloadButton_->get_active() ? 1 : 0;
  buffer = bufferSpinButton_->get_value_as_int();

  // If ejecting the CD after recording is requested issue a warning message
  // because buffer under runs may occur for other devices that are recording.
  if (eject && gnome_config_get_bool(SET_RECORD_EJECT_WARNING)) {
    Ask3Box msg(parent, "Record", 1, 2, 
		"Ejecting a CD may block the SCSI bus and",
		"cause buffer under runs when other devices",
		"are still recording.", "",
		"Keep the eject setting anyway?", NULL);

    switch (msg.run()) {
    case 1: // keep eject setting
      if (msg.dontShowAgain())
	gnome_config_set_bool(SET_RECORD_EJECT_WARNING, FALSE);
      break;
    case 2: // don't keep eject setting
      eject = 0;
      ejectButton_->set_active(false);
      break;
    default: // cancel
      return;
      break;
    }
  }

  // The same is true for reloading the disk.
  if (reload && gnome_config_get_bool(SET_RECORD_RELOAD_WARNING)) {
    Ask3Box msg(parent, "Record", 1, 2, 
		"Reloading a CD may block the SCSI bus and",
		"cause buffer under runs when other devices",
		"are still recording.", "",
		"Keep the reload setting anyway?", NULL);

    switch (msg.run()) {
    case 1: // keep eject setting
      if (msg.dontShowAgain())
	gnome_config_set_bool(SET_RECORD_RELOAD_WARNING, FALSE);
      break;
    case 2: // don't keep eject setting
      reload = 0;
      reloadButton_->set_active(false);
      break;
    default: // cancel
      return;
      break;
    }
  }
  
  Gtk::CList_Helpers::SelectionList selection = DEVICES->selection();

  for (i = 0; i < selection.size(); i++) {
    DeviceList::DeviceData *data = (DeviceList::DeviceData*)selection[i].get_data();

    if (data != NULL) {
      CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

      if (dev != NULL) {
	if (dev->recordDao(tocEdit_, simulate, multiSession, speed,
			   eject, reload, buffer) != 0) {
	  message(-2, "Cannot start disk-at-once recording.");
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

void RecordCDTarget::setSpeed(int s)
{
  if (s >= 0 && s <= MAX_SPEED_ID)
    speed_ = s;
}

void RecordCDTarget::updateBufferRAMLabel()
{
  char label[20];
  
  sprintf(label, "= %0.2f Mb buffer.", bufferSpinButton_->get_value_as_float() * 0.171875);
  bufferRAMLabel_->set(string(label));
}

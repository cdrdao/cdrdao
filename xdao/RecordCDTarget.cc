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
#include "RecordCDSource.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "DeviceList.h"

#include "util.h"

RecordCDTarget::RecordCDTarget()
{
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Table *table2;
  Gtk::Label *label;
  Gtk::Adjustment *adjustment;
  Gtk::Tooltips *toolTips;

  active_ = 0;
  tocEdit_ = NULL;

//  set_title(string("Record"));
//  set_usize(0, 300);

  speed_ = 1;

//  startButton_ = new Gtk::Button(string(" Start "));

  toolTips = new Gtk::Tooltips;
  toolTips->force_window();

  DEVICES = new DeviceList(CdDevice::CD_R);

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  contents->pack_start(*DEVICES, TRUE, TRUE);

  // device settings
  Gtk::Frame *recordOptionsFrame = new Gtk::Frame(string("Record Options"));

  table = new Gtk::Table(8, 1, FALSE);
  table->set_row_spacings(2);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, FALSE, FALSE, 5);
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  recordOptionsFrame->add(*vbox);
  vbox->show();
  hbox->show();
  table->show();

  toolTips->set_tip(*table, "Right click to get a menu");
  
  simulateButton_ = new Gtk::CheckButton(string("Simulation - no real write is done"), 0);
  simulateButton_->set_active(true);
  simulateButton_->show();
  table->attach(*simulateButton_, 0, 1, 0, 1);

//  toolTips->set_tip(*simulateButton_, "Right click to get a menu");

  closeSessionButton_ = new Gtk::CheckButton(string("Close disk - no further writing possible!"), 0);
  closeSessionButton_->set_active(true);
  closeSessionButton_->show();
  table->attach(*closeSessionButton_, 0, 1, 1, 2);

  ontheflyButton_ = new Gtk::CheckButton(string("On the fly - no image file is created"), 0);
  ontheflyButton_->set_active(false);
  ontheflyButton_->show();
  table->attach(*ontheflyButton_, 0, 3, 2, 3);

  ejectButton_ = new Gtk::CheckButton(string("Eject the CD after writing"), 0);
  ejectButton_->set_active(false);
  ejectButton_->show();
  table->attach(*ejectButton_, 0, 3, 3, 4);

  reloadButton_ = new Gtk::CheckButton(string("Reload the CD after writing, if necessary"), 0);
  reloadButton_->set_active(false);
  reloadButton_->show();
  table->attach(*reloadButton_, 0, 3, 4, 5);

  table2 = new Gtk::Table(2, 3, FALSE);
  table2->set_col_spacings(10);
  table2->show();

  label = new Gtk::Label(string("Speed: "), 0);
  label->show();
  table2->attach(*label, 0, 1, 0, 1);
  
  adjustment = new Gtk::Adjustment(1, 1, 20);
  speedSpinButton_ = new Gtk::SpinButton(*adjustment);
  speedSpinButton_->set_digits(0);
  speedSpinButton_->show();
  speedSpinButton_->set_sensitive(false);
  adjustment->value_changed.connect(SigC::slot(this, &RecordCDTarget::speedChanged));
  table2->attach(*speedSpinButton_, 1, 2, 0, 1);

  speedButton_ = new Gtk::CheckButton(string("Use max."), 0);
  speedButton_->set_active(true);
  speedButton_->show();
  speedButton_->toggled.connect(SigC::slot(this, &RecordCDTarget::speedButtonChanged));
  table2->attach(*speedButton_, 2, 3, 0, 1);

  label = new Gtk::Label(string("Buffer: "), 0);
  label->show();
  table2->attach(*label, 0, 1, 1, 2);
  adjustment = new Gtk::Adjustment(10, 10, 1000);
  bufferSpinButton_ = new Gtk::SpinButton(*adjustment);
  bufferSpinButton_->show();
  table2->attach(*bufferSpinButton_, 1, 2, 1, 2);
  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("audio seconds "));
  hbox->pack_start(*label, FALSE);
  label->show();
  bufferRAMLabel_ = new Gtk::Label(string("= 1.72 Mb buffer."), 0);
  hbox->pack_start(*bufferRAMLabel_, TRUE, TRUE);
  bufferRAMLabel_->show();
  adjustment->value_changed.connect(SigC::slot(this, &RecordCDTarget::updateBufferRAMLabel));
  table2->attach(*hbox, 2, 3, 1, 2);
  hbox->show();

  table->attach(*table2, 0, 1, 5, 6);
  
  contents->pack_start(*recordOptionsFrame, FALSE, FALSE);
  recordOptionsFrame->show();

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents);
  contents->show();

  pack_start(*contentsHBox);
  contentsHBox->show();

//  show();
}

RecordCDTarget::~RecordCDTarget()
{
}


void RecordCDTarget::start(TocEdit *tedit,
			   RecordGenericDialog::RecordSourceType source)
{
  active_ = 1;
  
  update(UPD_ALL, tedit, source);

  show();
}

void RecordCDTarget::stop()
{
  if (active_) {
    hide();
    active_ = 0;
    tocEdit_ = NULL;
  }
}

void RecordCDTarget::update(unsigned long level,
			    RecordGenericDialog::RecordSourceType source)
{
  update(level, tocEdit_, source);
}
void RecordCDTarget::update(unsigned long level, TocEdit *tedit,
			    RecordGenericDialog::RecordSourceType source)
{
  if (!active_)
    return;

  if (tocEdit_ != tedit) {
    tocEdit_ = tedit;
  }

  if (level & UPD_CD_DEVICES)
    DEVICES->import();
  else if (level & UPD_CD_DEVICE_STATUS)
    DEVICES->importStatus();

  if (source == RecordGenericDialog::S_TOC)
    ontheflyButton_->set_sensitive(false);
  else if (source == RecordGenericDialog::S_CD)
    ontheflyButton_->set_sensitive(true);
}

void RecordCDTarget::cancelAction()
{
  stop();
}

void RecordCDTarget::startAction(RecordGenericDialog::RecordSourceType source,
				 RecordTocSource *TOC, RecordCDSource *CD)
{
  int eject, simulate, speed, multiSession, reload, onthefly;
  int correction;
  int started = 0;
  Toc *toc;
  int buffer;
  unsigned int i;

//Note: There is duplicated code here, it is easier to separate things
//      than an if at every single line.
  if (source == RecordGenericDialog::S_TOC)
  {
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

    simulate = simulateButton_->get_active() ? 1 : 0;
    multiSession = closeSessionButton_->get_active() ? 0 : 1;
    if (speedButton_->get_active())
      speed = 0;
    else
      speed = speed_;
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
	  if (dev->recordDao(tocEdit_, simulate, multiSession,
			     speed, eject, reload, buffer) != 0) {
	    message(-2, "Cannot start disk-at-once recording.");
	  }
	  else {
	    started = 1;
	  }
	}
      }
    }
  }
  // We are doing CD to CD copy
  else if (source == RecordGenericDialog::S_CD)
  {
    if (CD->DEVICES->selection().empty()) {
      MessageBox msg(parent, "Record", 0, 
		     "Please select one reader device.", NULL);
      msg.run();
      return;
    }

    if (DEVICES->selection().empty()) {
      MessageBox msg(parent, "Record", 0, 
		     "Please select at least one recorder device.", NULL);
      msg.run();
      return;
    }

    //Read options
    correction = CD->getCorrection();

    //Record options
    simulate = simulateButton_->get_active() ? 1 : 0;
    multiSession = closeSessionButton_->get_active() ? 0 : 1;
    onthefly = ontheflyButton_->get_active() ? 1 : 0;
    if (speedButton_->get_active())
      speed = 0;
    else
      speed = speed_;
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

    //First we get the CD reader (the "r" stands for reader)
    Gtk::CList_Helpers::SelectionList r_selection = CD->DEVICES->selection();
    DeviceList::DeviceData *r_data =
			(DeviceList::DeviceData*)r_selection[0].get_data();
    if (r_data != NULL) {
      CdDevice *readdev = CdDevice::find(r_data->bus, r_data->id, r_data->lun);
    if (readdev != NULL) {

      Gtk::CList_Helpers::SelectionList selection = DEVICES->selection();

      for (i = 0; i < selection.size(); i++) {
        DeviceList::DeviceData *data = (DeviceList::DeviceData*)selection[i].get_data();

        if (data != NULL) {
          CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

        if (dev != NULL) {
      	if (dev->duplicateDao(simulate, multiSession, speed,
			       eject, reload, buffer, onthefly, correction, readdev) != 0) {
	      message(-2, "Cannot start disk-at-once duplication.");
  	  }
  	  else {
  	    started = 1;
	    }
        }
      }
      }
      }
    }

  }

    if (started)
      guiUpdate(UPD_CD_DEVICE_STATUS);
}

void RecordCDTarget::updateBufferRAMLabel()
{
  char label[20];
  
  sprintf(label, "= %0.2f Mb buffer.", bufferSpinButton_->get_value_as_float() * 0.171875);
  bufferRAMLabel_->set(string(label));
}

void RecordCDTarget::speedButtonChanged()
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

void RecordCDTarget::speedChanged()
{
//FIXME: Should only allow 1, 2, 4, 6, 8, 10, 12, 14, ...
//FIXME: Can someone come with a cleaner solution???
  gint new_speed;

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

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
/*
 * $Log: RecordDialog.cc,v $
 * Revision 1.3  2000/04/16 20:31:59  andreasm
 * Fixed radio button stuff.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:33  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/09/07 11:16:16  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: RecordDialog.cc,v 1.3 2000/04/16 20:31:59 andreasm Exp $";

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "RecordDialog.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "util.h"

#define MAX_SPEED_ID 5

static RecordDialog::SpeedTable SPEED_TABLE[MAX_SPEED_ID + 1] = {
  { 0, "Max" },
  { 1, "1x" },
  { 2, "2x" },
  { 4, "4x" },
  { 6, "6x" },
  { 8, "8x" }
};


RecordDialog::RecordDialog()
{
  int i;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Label *label;

  active_ = 0;
  tocEdit_ = NULL;

  set_title(string("Record"));
  set_usize(0, 300);

/*llanero
  speedMenuFactory_ = new Gtk::ItemFactory_Menu("<Main>");

  for (i = 0; i <= MAX_SPEED_ID; i++) {
    string s("/");
    s += SPEED_TABLE[i].name;

    speedMenuFactory_->create_item(s, 0, "<Item>", ItemFactoryConnector<RecordDialog, int>(this, &RecordDialog::setSpeed, i));
  }

  speedMenu_ = new Gtk::OptionMenu;
  speedMenu_->set_menu(speedMenuFactory_->get_menu_widget(string("")));

  speed_ = 0;
  speedMenu_->set_history(speed_);

  startButton_ = new Gtk::Button(string(" Start "));
*/
  Gtk::RadioButton_Helpers::Group simWriteGroup;

  simulateButton_ = new Gtk::RadioButton(simWriteGroup, string("Simulate"));
  writeButton_ = new Gtk::RadioButton(simWriteGroup, string("Write"));
  
  closeSessionButton_ = new Gtk::CheckButton(string("Close Disk"));
  closeSessionButton_->set_active(true);

  ejectButton_ = new Gtk::CheckButton(string("Eject"));
  ejectButton_->set_active(false);

  reloadButton_ = new Gtk::CheckButton(string("Reload"));
  reloadButton_->set_active(false);

  list_ = new Gtk::CList(6);

  list_->set_column_title(0, string("Bus"));
  list_->set_column_justification(0, GTK_JUSTIFY_CENTER);

  list_->set_column_title(1, string("Id"));
  list_->set_column_justification(1, GTK_JUSTIFY_CENTER);

  list_->set_column_title(2, string("Lun"));
  list_->set_column_justification(2, GTK_JUSTIFY_CENTER);

  list_->set_column_title(3, string("Vendor"));
  list_->set_column_justification(3, GTK_JUSTIFY_LEFT);

  list_->set_column_title(4, string("Model"));
  list_->set_column_justification(4, GTK_JUSTIFY_LEFT);

  list_->set_column_title(5, string("Status"));
  list_->set_column_justification(5, GTK_JUSTIFY_LEFT);

  list_->column_titles_show();
  list_->column_titles_passive();
  list_->set_selection_mode(GTK_SELECTION_MULTIPLE);


  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);


  // available device list
  Gtk::HBox *listHBox = new Gtk::HBox;
  Gtk::VBox *listVBox = new Gtk::VBox;

  hbox = new Gtk::HBox;

  hbox->pack_start(*list_, TRUE, TRUE);
  list_->show();

  Gtk::Adjustment *adjust = new Gtk::Adjustment(0.0, 0.0, 0.0);

  Gtk::VScrollbar *scrollBar = new Gtk::VScrollbar(*adjust);
  hbox->pack_start(*scrollBar, FALSE, FALSE);
  scrollBar->show();

  list_->set_vadjustment(*adjust);


  listHBox->pack_start(*hbox, TRUE, TRUE, 5);
  hbox->show();
  listVBox->pack_start(*listHBox, TRUE, TRUE, 5);
  listHBox->show();

  Gtk::Frame *listFrame = new Gtk::Frame(string("Available Recorder Devices"));
  listFrame->add(*listVBox);
  listVBox->show();
  contents->pack_start(*listFrame, TRUE, TRUE);
  listFrame->show();

  // device settings
  Gtk::Frame *recordOptionsFrame = new Gtk::Frame(string("Record Options"));

  table = new Gtk::Table(3, 2, FALSE);
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
  table->attach(*hbox, 2, 3, 0, 1);
  hbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Recording Speed: "));
  hbox->pack_start(*label, FALSE);
  label->show();
//llanero  hbox->pack_start(*speedMenu_, FALSE);
  speedMenu_->show();
  table->attach(*hbox, 1, 2, 1, 2);
  hbox->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*reloadButton_, FALSE);
  reloadButton_->show();
  table->attach(*hbox, 2, 3, 1, 2);
  hbox->show();
  
  
  contents->pack_start(*recordOptionsFrame, FALSE, FALSE);
  recordOptionsFrame->show();


  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();

  get_vbox()->pack_start(*contentsHBox, TRUE, TRUE, 10);
  contentsHBox->show();

  get_vbox()->show();


  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  bbox->pack_start(*startButton_);
  startButton_->show();
  startButton_->clicked.connect(SigC::slot(this,&RecordDialog::startAction));
  
  Gtk::Button *cancelButton = new Gtk::Button(string(" Cancel "));
  bbox->pack_start(*cancelButton);
  cancelButton->show();
  cancelButton->clicked.connect(SigC::slot(this,&RecordDialog::cancelAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();
}

RecordDialog::~RecordDialog()
{
  DeviceData *data;

  while (list_->rows() > 0) {
    data = (DeviceData*)list_->get_row_data(0);
    delete data;
    list_->remove_row(0);
  }

  delete list_;
  list_ = NULL;

  delete startButton_;
  startButton_ = NULL;
}


void RecordDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void RecordDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  tocEdit_ = tocEdit;

  if (level & UPD_CD_DEVICES)
    import();
  else if (level & UPD_CD_DEVICE_STATUS)
    importStatus();
}


gint RecordDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void RecordDialog::cancelAction()
{
  stop();
}

void RecordDialog::startAction()
{
  int eject, simulate, speed, multiSession, reload;
  int started = 0;
  Toc *toc;

  if (tocEdit_ == NULL)
    return;

  if (list_->selbegin() == list_->selend()) {
    MessageBox msg(this, "Record", 0, 
		   "Please select at least one recorder device.", NULL);
    msg.run();
    return;
  }

  toc = tocEdit_->toc();

  if (toc->nofTracks() == 0 || toc->length().lba() < 300) {
    MessageBox msg(this, "Cannot Record", 0,
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
      Ask2Box msg(this, "CD-TEXT Inconsistency", 0, 2,
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
      MessageBox msg(this, "CD-TEXT Error", 0, 
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

  // If ejecting the CD after recording is requested issue a warning message
  // because buffer under runs may occur for other devices that are recording.
  if (eject && SETTINGS->getInteger(SET_RECORD_EJECT_WARNING) != 0) {
    Ask3Box msg(this, "Record", 1, 2, 
		"Ejecting a CD may block the SCSI bus and",
		"cause buffer under runs when other devices",
		"are still recording.", "",
		"Keep the eject setting anyway?", NULL);

    switch (msg.run()) {
    case 1: // keep eject setting
      if (msg.dontShowAgain())
	SETTINGS->set(SET_RECORD_EJECT_WARNING, 0);
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
  if (reload && SETTINGS->getInteger(SET_RECORD_RELOAD_WARNING) != 0) {
    Ask3Box msg(this, "Record", 1, 2, 
		"Reloading a CD may block the SCSI bus and",
		"cause buffer under runs when other devices",
		"are still recording.", "",
		"Keep the reload setting anyway?", NULL);

    switch (msg.run()) {
    case 1: // keep eject setting
      if (msg.dontShowAgain())
	SETTINGS->set(SET_RECORD_RELOAD_WARNING, 0);
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
  

  Gtk::CList::seliterator itr;

  for (itr = list_->selbegin(); itr != list_->selend(); itr++) {
    DeviceData *data = (DeviceData*)list_->get_row_data(*itr);

    if (data != NULL) {
      CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

      if (dev != NULL) {
	if (dev->recordDao(tocEdit_, simulate, multiSession, speed,
			   eject, reload) != 0) {
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

void RecordDialog::appendTableEntry(CdDevice *dev)
{
  DeviceData *data;
  char buf[50];
  string idStr;
  string busStr;
  string lunStr;
  const gchar *rowStr[6];

  data = new DeviceData;
  data->bus = dev->bus();
  data->id = dev->id();
  data->lun = dev->lun();

  sprintf(buf, "%d", data->bus);
  busStr = buf;
  rowStr[0] = busStr.c_str();

  sprintf(buf, "%d", data->id);
  idStr = buf;
  rowStr[1] = idStr.c_str();

  sprintf(buf, "%d", data->lun);
  lunStr = buf;
  rowStr[2] = lunStr.c_str();

  rowStr[3] = dev->vendor();
  rowStr[4] = dev->product();
  
  rowStr[5] = CdDevice::status2string(dev->status());

  list_->append(rowStr);
  list_->set_row_data(list_->rows() - 1, data);

  if (dev->status() == CdDevice::DEV_READY)
    list_->set_selectable(list_->rows() - 1, true);
  else
    list_->set_selectable(list_->rows() - 1, false);
}

void RecordDialog::import()
{
  CdDevice *drun;
  DeviceData *data;
  int i;

  list_->freeze();

  while (list_->rows() > 0) {
    data = (DeviceData*)list_->get_row_data(0);
    delete data;
    list_->remove_row(0);
  }

  for (drun = CdDevice::first(); drun != NULL; drun = CdDevice::next(drun)) {
    if (drun->driverId() > 0 &&
	(drun->deviceType() == CdDevice::CD_R ||
	 drun->deviceType() == CdDevice::CD_RW)) {
      appendTableEntry(drun);
    }
  }

  list_->thaw();

  if (list_->rows() > 0) {
    list_->columns_autosize();
    list_->moveto(0, 0, 0.0, 0.0);

    // select first selectable device
    for (i = 0; i < list_->rows(); i++) {
      if (list_->get_selectable(i)) {
	list_->cause_select_row(i, 0);
	break;
      }
    }
  }
}

void RecordDialog::importStatus()
{
  int i;
  DeviceData *data;
  CdDevice *dev;

  for (i = 0; i < list_->rows(); i++) {
    if ((data = (DeviceData*)list_->get_row_data(i)) != NULL &&
	(dev = CdDevice::find(data->bus, data->id, data->lun)) != NULL) {
      if (dev->status() == CdDevice::DEV_READY) {
	list_->set_selectable(i, true);
      }
      else {
	list_->cause_unselect_row(i, 0);
	list_->set_selectable(i, false);
      }

      list_->set_text(i, 5, string(CdDevice::status2string(dev->status())));
    }
  }

  list_->columns_autosize();

}

void RecordDialog::setSpeed(int s)
{
  if (s >= 0 && s <= MAX_SPEED_ID)
    speed_ = s;
}

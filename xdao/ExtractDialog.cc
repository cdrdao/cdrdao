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
#include <unistd.h>

#include "ExtractDialog.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "util.h"

#define MAX_SPEED_ID 8

#define MAX_CORRECTION_ID 3

static ExtractDialog::SpeedTable SPEED_TABLE[MAX_SPEED_ID + 1] = {
  { 0, "Max" },
  { 20, "20x" },
  { 15, "15x" },
  { 10, "10x" },
  { 8, "8x" },
  { 6, "6x" },
  { 4, "4x" },
  { 2, "2x" },
  { 1, "1x" }
};

static ExtractDialog::CorrectionTable CORRECTION_TABLE[MAX_CORRECTION_ID + 1] = {
  { 3, "Jitter + scratch" },
  { 2, "Jitter + checks" },
  { 1, "Jitter correction" },
  { 0, "No checking" }
};


ExtractDialog::ExtractDialog()
{
  int i;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Label *label;

  active_ = 0;
  tocEdit_ = NULL;

  set_title(string("Extract"));
  set_usize(0, 400);

  Gtk::Menu *menu = manage(new Gtk::Menu);
  Gtk::MenuItem *mi;

  for (i = 0; i <= MAX_SPEED_ID; i++) {
    mi = manage(new Gtk::MenuItem(SPEED_TABLE[i].name));
    mi->activate.connect(bind(slot(this, &ExtractDialog::setSpeed), i));
    mi->show();
    menu->append(*mi);
  }

  speedMenu_ = new Gtk::OptionMenu;
  speedMenu_->set_menu(menu);

  speed_ = 0;
  speedMenu_->set_history(speed_);

  Gtk::Menu *menuCorrection = manage(new Gtk::Menu);
  Gtk::MenuItem *miCorr;

  for (i = 0; i <= MAX_CORRECTION_ID; i++) {
    miCorr = manage(new Gtk::MenuItem(CORRECTION_TABLE[i].name));
    miCorr->activate.connect(bind(slot(this, &ExtractDialog::setCorrection), i));
    miCorr->show();
    menuCorrection->append(*miCorr);
  }

  correctionMenu_ = new Gtk::OptionMenu;
  correctionMenu_->set_menu(menuCorrection);

  correction_ = 0;
  correctionMenu_->set_history(correction_);


  fileNameEntry_ = new Gtk::Entry;

  startButton_ = new Gtk::Button(string(" Start "));

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
  list_->set_selection_mode(GTK_SELECTION_SINGLE);


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

  Gtk::Frame *listFrame = new Gtk::Frame(string("Available Reader Devices"));
  listFrame->add(*listVBox);
  listVBox->show();
  contents->pack_start(*listFrame, TRUE, TRUE);
  listFrame->show();

  // device settings
  Gtk::Frame *extractOptionsFrame = new Gtk::Frame(string("Read Options"));

  vbox = new Gtk::VBox(TRUE, TRUE);
  extractOptionsFrame->add(*vbox);
  vbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Read Speed: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*speedMenu_, FALSE);
  speedMenu_->show();
  vbox->pack_start(*hbox, FALSE);
  hbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Correction Method: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*correctionMenu_, FALSE);
  correctionMenu_->show();
  vbox->pack_start(*hbox, FALSE);
  hbox->show();

  onTheFlyButton_ = new Gtk::CheckButton(string("On the Fly"), 0);
  onTheFlyButton_->set_active(false);
  vbox->pack_start(*onTheFlyButton_);
  onTheFlyButton_->show();

  continueOnErrorButton_ = new Gtk::CheckButton(string("Continue if errors found"), 0);
  continueOnErrorButton_->set_active(false);
  vbox->pack_start(*continueOnErrorButton_);
  continueOnErrorButton_->show();

  ignoreIncorrectTOCButton_ = new Gtk::CheckButton(string("Ignore incorrect TOC"), 0);
  ignoreIncorrectTOCButton_->set_active(false);
  vbox->pack_start(*ignoreIncorrectTOCButton_);
  ignoreIncorrectTOCButton_->show();

  readCDTEXTButton_ = new Gtk::CheckButton(string("Try to read CD TEXT data"), 0);
  readCDTEXTButton_->set_active(true);
  vbox->pack_start(*readCDTEXTButton_);
  readCDTEXTButton_->show();

//FIXME: This should go in RECORD!
  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Name: "));
  hbox->pack_start(*label, FALSE, FALSE);
  label->show();
  hbox->pack_start(*fileNameEntry_, FALSE, FALSE);
  fileNameEntry_->show();
  vbox->pack_start(*hbox, FALSE);
  hbox->show();

  contents->pack_start(*extractOptionsFrame, FALSE, FALSE);
  extractOptionsFrame->show();


  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents, TRUE, TRUE, 10);
  contents->show();

  get_vbox()->pack_start(*contentsHBox, TRUE, TRUE, 10);
  contentsHBox->show();

  get_vbox()->show();


  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  bbox->pack_start(*startButton_);
  startButton_->show();
  startButton_->clicked.connect(SigC::slot(this,&ExtractDialog::startAction));
  
  Gtk::Button *cancelButton = new Gtk::Button(string(" Cancel "));
  bbox->pack_start(*cancelButton);
  cancelButton->show();
  cancelButton->clicked.connect(SigC::slot(this,&ExtractDialog::cancelAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();
}

ExtractDialog::~ExtractDialog()
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


void ExtractDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void ExtractDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void ExtractDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  tocEdit_ = tocEdit;

  if (level & UPD_CD_DEVICES)
    import();
  else if (level & UPD_CD_DEVICE_STATUS)
    importStatus();
}


gint ExtractDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void ExtractDialog::cancelAction()
{
  stop();
}

void ExtractDialog::startAction()
{
  int eject, simulate, speed, multiSession, reload;
  int started = 0;
  Toc *toc;
  string temp;
  char *fileName;
  char *buffer;
  int correction;
  
  if (tocEdit_ == NULL)
    return;

  if (list_->selbegin() == list_->selend()) {
    MessageBox msg(this, "Extract", 0, 
		   "Please select one reader device.", NULL);
    msg.run();
    return;
  }

  temp = fileNameEntry_->get_text();
  if (temp == "")
  {
    MessageBox msg(this, "Extract", 0, 
		   "Please write a name for the image.", NULL);
    msg.run();
    return;
  }
  else
    fileName = strdup(temp.c_str());

//FIXME: This only tests current directory, user should be able to select
//       the directory from a gtk_file_selection dialog, so this should test
//       in the specified directory.

  buffer = g_strdup_printf("%s%s.toc", "./", fileName);

  if (access(buffer, R_OK) != -1) 
  {

    Ask2Box msg(this, "Extract", 0, 1,
    		g_strdup_printf("The filename %s.toc already exists.",
    		fileName), "Do you want to OVERWRITE it?", "", NULL);

    switch (msg.run()) {
    case 1: // remove the file an continue
      if (unlink(buffer) == -1)
      {
        MessageBox msg(this, "Extract", 0,
        	g_strdup_printf("Error deleting the file %s.toc", "",
        	fileName), NULL);
        msg.run();
        return;
      }
      break;
    default: // cancel
      return;
      break;
    }
  }
  free(buffer);


  speed = SPEED_TABLE[speed_].speed;
  correction = CORRECTION_TABLE[correction_].correction;

  Gtk::CList::seliterator itr;

  for (itr = list_->selbegin(); itr != list_->selend(); itr++) {
    DeviceData *data = (DeviceData*)list_->get_row_data(*itr);

    if (data != NULL) {
      CdDevice *dev = CdDevice::find(data->bus, data->id, data->lun);

      if (dev != NULL) {
	if (dev->extractDao(fileName, correction) != 0) {
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

void ExtractDialog::appendTableEntry(CdDevice *dev)
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

void ExtractDialog::import()
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
	(drun->deviceType() == CdDevice::CD_ROM ||
	 drun->deviceType() == CdDevice::CD_R ||
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

void ExtractDialog::importStatus()
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

void ExtractDialog::setSpeed(int s)
{
  if (s >= 0 && s <= MAX_SPEED_ID)
    speed_ = s;
}

void ExtractDialog::setCorrection(int s)
{
  if (s >= 0 && s <= MAX_CORRECTION_ID)
    correction_ = s;
}

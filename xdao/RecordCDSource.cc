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

#include "RecordCDSource.h"
#include "MessageBox.h"
#include "xcdrdao.h"
#include "Settings.h"

#include "CdDevice.h"
#include "guiUpdate.h"
#include "TocEdit.h"

#include "util.h"

#include "DeviceList.h"

#define MAX_CORRECTION_ID 3

static RecordCDSource::CorrectionTable CORRECTION_TABLE[MAX_CORRECTION_ID + 1] = {
  { 3, "Jitter + scratch" },
  { 2, "Jitter + checks" },
  { 1, "Jitter correction" },
  { 0, "No checking" }
};


RecordCDSource::RecordCDSource()
{
  int i;
  Gtk::HBox *hbox;
  Gtk::VBox *vbox;
  Gtk::Table *table;
  Gtk::Label *label;

  active_ = 0;
  tocEdit_ = NULL;

  speed_ = 1;

  Gtk::Menu *menuCorrection = manage(new Gtk::Menu);
  Gtk::MenuItem *miCorr;

  for (i = 0; i <= MAX_CORRECTION_ID; i++) {
    miCorr = manage(new Gtk::MenuItem(CORRECTION_TABLE[i].name));
    miCorr->activate.connect(bind(slot(this, &RecordCDSource::setCorrection), i));
    miCorr->show();
    menuCorrection->append(*miCorr);
  }

  correctionMenu_ = new Gtk::OptionMenu;
  correctionMenu_->set_menu(menuCorrection);

  correction_ = 0;
  correctionMenu_->set_history(correction_);


  DEVICES = new DeviceList(CD_ROM);

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  contents->pack_start(*DEVICES, TRUE, TRUE);

  // device settings
  Gtk::Frame *extractOptionsFrame = new Gtk::Frame(string("Read Options"));

  vbox = new Gtk::VBox(TRUE, TRUE);
  extractOptionsFrame->add(*vbox);
  vbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Speed: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  vbox->pack_start(*hbox, FALSE);
//  hbox->show();

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Correction Method: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*correctionMenu_, FALSE);
  correctionMenu_->show();
  vbox->pack_start(*hbox, FALSE);
  hbox->show();

  continueOnErrorButton_ = new Gtk::CheckButton(string("Continue if errors found"), 0);
  continueOnErrorButton_->set_active(false);
  vbox->pack_start(*continueOnErrorButton_);
//  continueOnErrorButton_->show();

  ignoreIncorrectTOCButton_ = new Gtk::CheckButton(string("Ignore incorrect TOC"), 0);
  ignoreIncorrectTOCButton_->set_active(false);
  vbox->pack_start(*ignoreIncorrectTOCButton_);
//  ignoreIncorrectTOCButton_->show();

  contents->pack_start(*extractOptionsFrame, FALSE, FALSE);
  extractOptionsFrame->show();


  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents);
  contents->show();

  pack_start(*contentsHBox);
  contentsHBox->show();

//  show();
}

RecordCDSource::~RecordCDSource()
{
}


void RecordCDSource::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_CD_DEVICES, tocEdit);

  show();
}

void RecordCDSource::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordCDSource::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  tocEdit_ = tocEdit;

  if (level & UPD_CD_DEVICES)
    DEVICES->import();
  else if (level & UPD_CD_DEVICE_STATUS)
    DEVICES->importStatus();
}

void RecordCDSource::setCorrection(int s)
{
  if (s >= 0 && s <= MAX_CORRECTION_ID)
    correction_ = s;
}

int RecordCDSource::getCorrection()
{
return CORRECTION_TABLE[correction_].correction;
}
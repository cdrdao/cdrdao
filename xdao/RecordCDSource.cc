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
  Gtk::Table *table2;
  Gtk::Label *label;
  Gtk::Adjustment *adjustment;

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


  DEVICES = new DeviceList(CdDevice::CD_ROM);

  Gtk::VBox *contents = new Gtk::VBox;
  contents->set_spacing(10);

  contents->pack_start(*DEVICES, TRUE, TRUE);

  // device settings
  Gtk::Frame *extractOptionsFrame = new Gtk::Frame(string("Read Options"));

  table = new Gtk::Table(8, 1, FALSE);
  table->set_row_spacings(2);
  table->set_border_width(5);
  table->show();

  extractOptionsFrame->add(*table);

  table2 = new Gtk::Table(8, 3, FALSE);
  table->set_col_spacings(10);
  table2->show();

  label = new Gtk::Label(string("Speed: "), 0);
  label->show();
  table2->attach(*label, 0, 1, 0, 1, 0);

  adjustment = new Gtk::Adjustment(1, 1, 50);
  speedSpinButton_ = new Gtk::SpinButton(*adjustment);
  speedSpinButton_->set_digits(0);
  speedSpinButton_->show();
  speedSpinButton_->set_sensitive(false);
  adjustment->value_changed.connect(SigC::slot(this, &RecordCDSource::speedChanged));
  table2->attach(*speedSpinButton_, 1, 2, 0, 1, 0);

  speedButton_ = new Gtk::CheckButton(string("Use max."));
  speedButton_->set_active(true);
  speedButton_->show();
  speedButton_->toggled.connect(SigC::slot(this, &RecordCDSource::speedButtonChanged));
  table2->attach(*speedButton_, 2, 3, 0, 1, 0);

//  table->attach(*table2, 0, 1, 0, 1);

  hbox = new Gtk::HBox;
  label = new Gtk::Label(string("Correction Method: "));
  hbox->pack_start(*label, FALSE);
  label->show();
  hbox->pack_start(*correctionMenu_, FALSE);
  correctionMenu_->show();
  table->attach(*hbox, 0, 1, 1, 2);
  hbox->show();

  continueOnErrorButton_ = new Gtk::CheckButton(string("Continue if errors found"), 0);
  continueOnErrorButton_->set_active(false);
//  continueOnErrorButton_->show();
//  table->attach(*continueOnErrorButton_, 0, 1, 2, 3);

  ignoreIncorrectTOCButton_ = new Gtk::CheckButton(string("Ignore incorrect TOC"), 0);
  ignoreIncorrectTOCButton_->set_active(false);
//  ignoreIncorrectTOCButton_->show();
//  table->attach(*ignoreIncorrectTOCButton_, 0, 1, 3, 4);

  contents->pack_start(*extractOptionsFrame, FALSE, FALSE);
  extractOptionsFrame->show();

  Gtk::HBox *contentsHBox = new Gtk::HBox;

  contentsHBox->pack_start(*contents);
  contents->show();

  pack_start(*contentsHBox, FALSE, FALSE);
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

  update(UPD_CD_DEVICES);

  show();
}

void RecordCDSource::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void RecordCDSource::update(unsigned long level)
{
  if (!active_)
    return;

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

void RecordCDSource::speedButtonChanged()
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

void RecordCDSource::speedChanged()
{
  //Do some validating here. speed_ <= MAX read speed of CD.
  speed_ = speedSpinButton_->get_value_as_int();
}

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

#include "util.h"

#include "DeviceList.h"

#define MAX_CORRECTION_ID 3

static RecordCDSource::CorrectionTable CORRECTION_TABLE[MAX_CORRECTION_ID + 1] = {
  { 3, "Jitter + scratch" },
  { 2, "Jitter + checks" },
  { 1, "Jitter correction" },
  { 0, "No checking" }
};


RecordCDSource::RecordCDSource(Gtk::Window *parent)
{
  parent_ = parent;
  active_ = 0;
//  onTheFly_ = 0;
  correction_ = 0;
  speed_ = 1;
  moreOptionsDialog_ = 0;
  set_spacing(10);

  DEVICES = new DeviceList(CdDevice::CD_ROM);
  pack_start(*DEVICES, false, false);

  // device settings
  Gtk::Frame *extractOptionsFrame = new Gtk::Frame(string("Read Options"));
  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(5);
  vbox->set_spacing(5);
  vbox->show();
  extractOptionsFrame->add(*vbox);
  
  onTheFlyButton_ = new Gtk::CheckButton(string("Copy to disk before burning"), 0);
  onTheFlyButton_->set_active(true);
  onTheFlyButton_->show();
  vbox->pack_start(*onTheFlyButton_);

  Gtk::HBox *hbox = new Gtk::HBox;
//  hbox->show();
  Gtk::Label *label = new Gtk::Label(string("Speed: "), 0);
  label->show();
  hbox->pack_start(*label, false, false);

  Gtk::Adjustment *adjustment = new Gtk::Adjustment(1, 1, 50);
  speedSpinButton_ = new Gtk::SpinButton(*adjustment);
  speedSpinButton_->set_digits(0);
  speedSpinButton_->show();
  speedSpinButton_->set_sensitive(false);
  adjustment->value_changed.connect(SigC::slot(this, &RecordCDSource::speedChanged));
  hbox->pack_start(*speedSpinButton_, false, false, 10);

  speedButton_ = new Gtk::CheckButton(string("Use max."), 0);
  speedButton_->set_active(true);
  speedButton_->show();
  speedButton_->toggled.connect(SigC::slot(this, &RecordCDSource::speedButtonChanged));
  hbox->pack_start(*speedButton_, true, true);
  vbox->pack_start(*hbox);

  Gnome::StockPixmap *moreOptionsPixmap =
  	manage(new Gnome::StockPixmap(GNOME_STOCK_MENU_PROP));
  Gtk::Label *moreOptionsLabel = manage(new Gtk::Label("More Options"));
  Gtk::HBox *moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->set_border_width(2);
  Gtk::Button *moreOptionsButton = manage(new Gtk::Button());
  moreOptionsBox->pack_start(*moreOptionsPixmap, false, false, 3);
  moreOptionsBox->pack_start(*moreOptionsLabel, false, false, 4);
  moreOptionsButton->add(*moreOptionsBox);
  moreOptionsButton->clicked.connect(slot(this, &RecordCDSource::moreOptions));
  moreOptionsPixmap->show();
  moreOptionsLabel->show();
  moreOptionsBox->show();
  moreOptionsButton->show();
  moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->show();
  vbox->pack_start(*moreOptionsBox);
  moreOptionsBox->pack_end(*moreOptionsButton, false, false);

  pack_start(*extractOptionsFrame, false, false);
  extractOptionsFrame->show();
}

RecordCDSource::~RecordCDSource()
{
  if (moreOptionsDialog_)
    delete moreOptionsDialog_;
}

void RecordCDSource::start()
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

void RecordCDSource::moreOptions()
{
  if (!moreOptionsDialog_)
  {
    Gtk::HBox *hbox;
    Gtk::Label *label;

    vector <string> buttons;
    buttons.push_back(GNOME_STOCK_BUTTON_CLOSE);
    moreOptionsDialog_ = new Gnome::Dialog("Source options", buttons);

    moreOptionsDialog_->set_parent(*parent_);
    moreOptionsDialog_->set_close(true);

    Gtk::VBox *vbox = moreOptionsDialog_->get_vbox();
    Gtk::Frame *frame = new Gtk::Frame("More Source Options");
    vbox->pack_start(*frame);
    vbox = new Gtk::VBox;
    vbox->set_border_width(10);
    vbox->set_spacing(5);
    vbox->show();
    frame->add(*vbox);
    frame->show();

    continueOnErrorButton_ = new Gtk::CheckButton(string("Continue if errors found"), 0);
    continueOnErrorButton_->set_active(false);
//    continueOnErrorButton_->show();
    vbox->pack_start(*continueOnErrorButton_);

    ignoreIncorrectTOCButton_ = new Gtk::CheckButton(string("Ignore incorrect TOC"), 0);
    ignoreIncorrectTOCButton_->set_active(false);
//    ignoreIncorrectTOCButton_->show();
    vbox->pack_start(*ignoreIncorrectTOCButton_);

    Gtk::Menu *menuCorrection = manage(new Gtk::Menu);
    Gtk::MenuItem *miCorr;
	
    for (int i = 0; i <= MAX_CORRECTION_ID; i++) {
      miCorr = manage(new Gtk::MenuItem(CORRECTION_TABLE[i].name));
      miCorr->activate.connect(bind(slot(this, &RecordCDSource::setCorrection), i));
      miCorr->show();
      menuCorrection->append(*miCorr);
    }
  
    correctionMenu_ = new Gtk::OptionMenu;
    correctionMenu_->set_menu(menuCorrection);
  
    correctionMenu_->set_history(correction_);
  
    hbox = new Gtk::HBox;
    label = new Gtk::Label(string("Correction Method: "));
    hbox->pack_start(*label, FALSE);
    label->show();
    hbox->pack_start(*correctionMenu_, FALSE);
    correctionMenu_->show();
    hbox->show();
    vbox->pack_start(*hbox);
  }

  moreOptionsDialog_->show();
}

void RecordCDSource::setCorrection(int s)
{
  if (s >= 0 && s <= MAX_CORRECTION_ID)
    correction_ = s;
}

bool RecordCDSource::getOnTheFly()
{
  return onTheFlyButton_->get_active() ? 0 : 1;
}

void RecordCDSource::setOnTheFly(bool active)
{
  onTheFlyButton_->set_active(!active);
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

void RecordCDSource::onTheFlyOption(bool visible)
{
  if (visible)
    onTheFlyButton_->show();
  else
    onTheFlyButton_->hide();
}

void RecordCDSource::selectOne()
{
  if (DEVICES->selection().empty()) {
    Gtk::CList *clist = DEVICES->getCList();
    if (clist->get_rows())
    {
      clist->row(0).select();
    }
    return;
  }
}


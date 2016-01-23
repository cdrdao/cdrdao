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

#include "config.h"
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "RecordCDSource.h"
#include "xcdrdao.h"

#include "CdDevice.h"
#include "guiUpdate.h"

#include "util.h"

#include "DeviceList.h"

#define MAX_CORRECTION_ID 3

static RecordCDSource::CorrectionTable CORRECTION_TABLE[MAX_CORRECTION_ID + 1] = {
  { 3, N_("Jitter + scratch (slow)") },
  { 2, N_("Jitter + checks") },
  { 1, N_("Jitter correction") },
  { 0, N_("No checking (fast)") }
};

#define MAX_SUBCHAN_READ_MODE_ID 2

static RecordCDSource::SubChanReadModeTable SUBCHAN_READ_MODE_TABLE[MAX_SUBCHAN_READ_MODE_ID + 1] = {
  { 0, N_("none") },
  { 1, N_("R-W packed") },
  { 2, N_("R-W raw") }
};


RecordCDSource::RecordCDSource(Gtk::Window *parent)
{
  parent_ = parent;
  active_ = 0;
//  onTheFly_ = 0;
  correction_ = 0;
  speed_ = 1;
  subChanReadMode_ = 0;
  moreOptionsDialog_ = 0;

  set_spacing(10);

  DEVICES = new DeviceList(CdDevice::CD_ROM);
  pack_start(*DEVICES);

  // device settings
  Gtk::Frame *extractOptionsFrame = new Gtk::Frame(_(" Read Options "));
  Gtk::VBox *vbox = new Gtk::VBox;
  vbox->set_border_width(5);
  vbox->set_spacing(5);
  vbox->show();
  extractOptionsFrame->add(*vbox);
  
  onTheFlyButton_ = new Gtk::CheckButton(_("Copy to disk before burning"), 0);
  onTheFlyButton_->set_active(true);
  onTheFlyButton_->show();
  vbox->pack_start(*onTheFlyButton_);

  Gtk::HBox *hbox = new Gtk::HBox;
  Gtk::Label *label = new Gtk::Label(_("Speed: "), 0);
  label->show();
  hbox->pack_start(*label, false, false);
  hbox->show();

  Glib::RefPtr<Gtk::Adjustment> adjustment = Gtk::Adjustment::create(1, 1, 50);
  speedSpinButton_ = new Gtk::SpinButton(adjustment);
  speedSpinButton_->set_digits(0);
  speedSpinButton_->show();
  speedSpinButton_->set_sensitive(false);
  adjustment->signal_value_changed().connect(sigc::mem_fun(*this, &RecordCDSource::speedChanged));
  hbox->pack_start(*speedSpinButton_, false, false, 10);

  speedButton_ = new Gtk::CheckButton(_("Use max."), 0);
  speedButton_->set_active(true);
  speedButton_->show();
  speedButton_->signal_toggled().connect(sigc::mem_fun(*this, &RecordCDSource::speedButtonChanged));
  hbox->pack_start(*speedButton_, true, true);
  vbox->pack_start(*hbox);

  Gtk::Image* moreOptionsPixmap =
  	manage(new Gtk::Image(Gtk::StockID(Gtk::Stock::PROPERTIES),
                              Gtk::ICON_SIZE_SMALL_TOOLBAR));
  Gtk::Label *moreOptionsLabel = manage(new Gtk::Label(_("More Options")));
  Gtk::HBox *moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->set_border_width(2);
  Gtk::Button *moreOptionsButton = manage(new Gtk::Button());
  moreOptionsBox->pack_start(*moreOptionsPixmap, false, false, 3);
  moreOptionsBox->pack_start(*moreOptionsLabel, false, false, 4);
  moreOptionsButton->add(*moreOptionsBox);
  moreOptionsButton->signal_clicked().connect(mem_fun(*this, &RecordCDSource::moreOptions));
  moreOptionsPixmap->show();
  moreOptionsLabel->show();
  moreOptionsBox->show();
  moreOptionsButton->show();
  moreOptionsBox = manage(new Gtk::HBox);
  moreOptionsBox->show();
  vbox->pack_start(*moreOptionsBox);
  moreOptionsBox->pack_end(*moreOptionsButton, false, false);

  pack_start(*extractOptionsFrame, Gtk::PACK_SHRINK);
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
      get_parent_window()->raise();
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
    Gtk::Table *table;
    Gtk::Label *label;

    table = new Gtk::Table(2, 4, false);
    table->set_row_spacings(2);
    table->set_col_spacings(10);
    table->set_border_width(5);

    moreOptionsDialog_ = new Gtk::MessageDialog(*parent_, _("Source options"),false, 
                                                Gtk::MESSAGE_QUESTION,
                                                Gtk::BUTTONS_CLOSE, true);

    Gtk::Box *box = moreOptionsDialog_->get_vbox();
    Gtk::Frame *frame = new Gtk::Frame(_(" More Source Options "));
    box->pack_start(*frame);
    Gtk::VBox *vbox = new Gtk::VBox;
    vbox->set_border_width(10);
    vbox->set_spacing(5);
    frame->add(*vbox);

    vbox->pack_start(*table);

    continueOnErrorButton_ =
      new Gtk::CheckButton(_("Continue if errors found"), 0);
    continueOnErrorButton_->set_active(false);
    table->attach(*continueOnErrorButton_, 0, 1, 0, 1);

    ignoreIncorrectTOCButton_ = new Gtk::CheckButton(_("Ignore incorrect TOC"),
                                                     0);
    ignoreIncorrectTOCButton_->set_active(false);
    table->attach(*ignoreIncorrectTOCButton_, 0, 1, 1, 2);

    for (int i = 0; i <= MAX_CORRECTION_ID; i++) {
      correctionMenu_.append(CORRECTION_TABLE[i].name);
    }
    correctionMenu_.signal_changed().connect(sigc::mem_fun(*this, &RecordCDSource::setCorrection));
    correctionMenu_.set_active(correction_);

    Gtk::Alignment *align;

    label = new Gtk::Label(_("Audio Correction Method:"));
    align = new Gtk::Alignment(0, 0.5, 0, 1);
    align->add(*label);
    table->attach(*align, 0, 1, 2, 3, Gtk::FILL);
    table->attach(correctionMenu_, 1, 2, 2, 3);

    for (int i = 0; i <= MAX_SUBCHAN_READ_MODE_ID; i++) {
      subChanReadModeMenu_.append(SUBCHAN_READ_MODE_TABLE[i].name);
    }
    subChanReadModeMenu_.signal_changed().connect(sigc::mem_fun(*this, &RecordCDSource::setSubChanReadMode));
    subChanReadModeMenu_.set_active(subChanReadMode_);

    label = new Gtk::Label(_("Sub-Channel Reading Mode:"));
    align = new Gtk::Alignment(0, 0.5, 0, 1);
    align->add(*label);
    table->attach(*align, 0, 1, 3, 4, Gtk::FILL);
    table->attach(subChanReadModeMenu_, 1, 2, 3, 4);
  }

  moreOptionsDialog_->show_all();
  moreOptionsDialog_->run();
  moreOptionsDialog_->hide();
}

void RecordCDSource::setCorrection()
{
  int s = correctionMenu_.get_active_row_number();
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

void RecordCDSource::setSubChanReadMode()
{
  int m = subChanReadModeMenu_.get_active_row_number();
  if (m >= 0 && m <= MAX_SUBCHAN_READ_MODE_ID)
    subChanReadMode_ = m;
}

int RecordCDSource::getSubChanReadMode()
{
  return SUBCHAN_READ_MODE_TABLE[subChanReadMode_].mode;
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


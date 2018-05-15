/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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

#include <gtkmm.h>
#include <gnome.h>

#include "AddSilenceDialog.h"

#include "TocEdit.h"
#include "TocEditView.h"
#include "guiUpdate.h"

#include "Sample.h"


AddSilenceDialog::AddSilenceDialog()
{
  Gtk::Button *button;
  Gtk::VBox *vbox;
  Gtk::HBox *hbox;

  tocEditView_ = NULL;
  active_ = false;
  mode_ = M_APPEND;

  Gtk::Frame *frame = new Gtk::Frame(_(" Length of Silence "));

  Gtk::Table *table = new Gtk::Table(4, 2, false);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, true, true, 5);
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, false, false, 5);
  frame->add(*vbox);
  
  Gtk::Label *label = new Gtk::Label(_("Minutes:"));
  table->attach(*label, 0, 1, 0, 1, Gtk::SHRINK);
  table->attach(minutes_, 1, 2, 0, 1);

  label = new Gtk::Label(_("Seconds:"));
  table->attach(*label, 0, 1, 1, 2, Gtk::SHRINK);
  table->attach(seconds_, 1, 2, 1, 2);

  label = new Gtk::Label(_("Frames:"));
  table->attach(*label, 0, 1, 2, 3, Gtk::SHRINK);
  table->attach(frames_, 1, 2, 2, 3);

  label = new Gtk::Label(_("Samples:"));
  table->attach(*label, 0, 1, 3, 4, Gtk::SHRINK);
  table->attach(samples_, 1, 2, 3, 4);

  hbox = new Gtk::HBox;
  hbox->pack_start(*frame, true, true, 10);

  get_vbox()->pack_start(*hbox, false, false, 10);

  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD);

  applyButton_ = new Gtk::Button(Gtk::StockID(Gtk::Stock::APPLY));
  bbox->pack_start(*applyButton_);
  applyButton_->signal_clicked().connect(mem_fun(*this, &AddSilenceDialog::applyAction));

  button = new Gtk::Button(Gtk::StockID(Gtk::Stock::CLEAR));
  bbox->pack_start(*button);
  button->signal_clicked().connect(mem_fun(*this, &AddSilenceDialog::clearAction));

  button = new Gtk::Button(Gtk::StockID(Gtk::Stock::CLOSE));
  bbox->pack_start(*button);
  button->signal_clicked().connect(mem_fun(*this, &AddSilenceDialog::closeAction));

  get_action_area()->pack_start(*bbox);
  show_all_children();
}

AddSilenceDialog::~AddSilenceDialog()
{
}

void AddSilenceDialog::mode(Mode m)
{
  mode_ = m;

  switch (mode_) {
  case M_APPEND:
    set_title(_("Append Silence"));
    break;
  case M_INSERT:
    set_title(_("Insert Silence"));
    break;
  }
}

void AddSilenceDialog::start(TocEditView *view)
{
  active_ = true;
  update(UPD_ALL, view);
  present();
  tocEditView_ = view;
}

void AddSilenceDialog::stop()
{
  hide();
  active_ = false;
}

void AddSilenceDialog::update(unsigned long level, TocEditView *view)
{
  if (!active_)
    return;

  if (view == NULL) {
    applyButton_->set_sensitive(false);
    tocEditView_ = NULL;
    return;
  }

  std::string s(view->tocEdit()->filename());
  s += " - ";
  s += APP_NAME;
  if (view->tocEdit()->tocDirty())
    s += "(*)";
  set_title(s);

  if ((level & UPD_EDITABLE_STATE) || tocEditView_ == NULL) {
    applyButton_->set_sensitive(view->tocEdit()->editable() ? true : false);
  }

  tocEditView_ = view;
}


bool AddSilenceDialog::on_delete_event(GdkEventAny*)
{
  stop();
  return 1;
}

void AddSilenceDialog::closeAction()
{
  stop();
}

void AddSilenceDialog::clearAction()
{
  minutes_.set_text("");
  seconds_.set_text("");
  frames_.set_text("");
  samples_.set_text("");
}

void AddSilenceDialog::applyAction()
{
  unsigned long length = 0;
  char buf[20];
  long val;
  TocEdit *tocEdit;
  
  if (tocEditView_ == NULL)
    return;

  tocEdit = tocEditView_->tocEdit();

  if (!tocEdit->editable())
    return;

  const char *s = minutes_.get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * 60 * 75 * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    minutes_.set_text(buf);
  }

  s = seconds_.get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * 75 * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    seconds_.set_text(buf);
  }

  s = frames_.get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    frames_.set_text(buf);
  }
  
  s = samples_.get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val;
    sprintf(buf, "%ld", val);
    samples_.set_text(buf);
  }
  
  if (length > 0) {
    unsigned long pos;

    switch (mode_) {
    case M_APPEND:
      tocEdit->appendSilence(length);
      update (UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL, tocEditView_);
      signal_tocModified (UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL);
      signal_fullView();
      signal_tocModified(UPD_SAMPLES);
      break;
    case M_INSERT:
      if (tocEditView_->sampleMarker(&pos)) {
        if (tocEdit->insertSilence(length, pos) == 0) {
          tocEditView_->sampleSelect(pos, pos + length - 1);
          update (UPD_TOC_DATA | UPD_TRACK_DATA, tocEditView_);
          signal_tocModified (UPD_TOC_DATA | UPD_TRACK_DATA);
        }
      }
      break;
    }
    guiUpdate();
  }
}

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
  active_ = 0;
  mode_ = M_APPEND;

  minutes_ = new Gtk::Entry;
  seconds_ = new Gtk::Entry;
  frames_ = new Gtk::Entry;
  samples_ = new Gtk::Entry;

  Gtk::Frame *frame = new Gtk::Frame(string("Length of Silence"));

  Gtk::Table *table = new Gtk::Table(4, 2, FALSE);
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  hbox = new Gtk::HBox;
  hbox->pack_start(*table, TRUE, TRUE, 5);
  table->show();
  vbox = new Gtk::VBox;
  vbox->pack_start(*hbox, FALSE, FALSE, 5);
  hbox->show();
  frame->add(*vbox);
  vbox->show();
  
  Gtk::Label *label = new Gtk::Label(string("Minutes:"));
  table->attach(*label, 0, 1, 0, 1, GTK_SHRINK);
  label->show();
  table->attach(*minutes_, 1, 2, 0, 1);
  minutes_->show();

  label = new Gtk::Label(string("Seconds:"));
  table->attach(*label, 0, 1, 1, 2, GTK_SHRINK);
  label->show();
  table->attach(*seconds_, 1, 2, 1, 2);
  seconds_->show();

  label = new Gtk::Label(string("Frames:"));
  table->attach(*label, 0, 1, 2, 3, GTK_SHRINK);
  label->show();
  table->attach(*frames_, 1, 2, 2, 3);
  frames_->show();

  label = new Gtk::Label(string("Samples:"));
  table->attach(*label, 0, 1, 3, 4, GTK_SHRINK);
  label->show();
  table->attach(*samples_, 1, 2, 3, 4);
  samples_->show();

  hbox = new Gtk::HBox;
  hbox->pack_start(*frame, TRUE, TRUE, 10);
  frame->show();

  get_vbox()->pack_start(*hbox, FALSE, FALSE, 10);
  hbox->show();

  get_vbox()->show();

  Gtk::HButtonBox *bbox = new Gtk::HButtonBox(GTK_BUTTONBOX_SPREAD);

  applyButton_ = new Gtk::Button(string(" Apply "));
  bbox->pack_start(*applyButton_);
  applyButton_->show();
  applyButton_->clicked.connect(slot(this, &AddSilenceDialog::applyAction));

  button = new Gtk::Button(string(" Clear "));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AddSilenceDialog::clearAction));

  button = new Gtk::Button(string(" Close "));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AddSilenceDialog::closeAction));

  get_action_area()->pack_start(*bbox);
  bbox->show();
  get_action_area()->show();

}

AddSilenceDialog::~AddSilenceDialog()
{
}

void AddSilenceDialog::mode(Mode m)
{
  mode_ = m;

  switch (mode_) {
  case M_APPEND:
    set_title(string("Append Silence"));
    break;
  case M_INSERT:
    set_title(string("Insert Silence"));
    break;
  }
}

void AddSilenceDialog::start(TocEditView *view)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, view);
  show();
}

void AddSilenceDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void AddSilenceDialog::update(unsigned long level, TocEditView *view)
{
  if (!active_)
    return;

  if (view == NULL) {
    applyButton_->set_sensitive(FALSE);
    tocEditView_ = NULL;
    return;
  }

  string s(view->tocEdit()->filename());
  s += " - ";
  s += APP_NAME;
  if (view->tocEdit()->tocDirty())
    s += "(*)";
  set_title(s);

  if ((level & UPD_EDITABLE_STATE) || tocEditView_ == NULL) {
    applyButton_->set_sensitive(view->tocEdit()->editable() ? TRUE : FALSE);
  }

  tocEditView_ = view;
}


gint AddSilenceDialog::delete_event_impl(GdkEventAny*)
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
  minutes_->set_text(string(""));
  seconds_->set_text(string(""));
  frames_->set_text(string(""));
  samples_->set_text(string(""));
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

  const char *s = minutes_->get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * 60 * 75 * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    minutes_->set_text(string(buf));
  }

  s = seconds_->get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * 75 * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    seconds_->set_text(string(buf));
  }

  s = frames_->get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val * SAMPLES_PER_BLOCK;
    sprintf(buf, "%ld", val);
    frames_->set_text(string(buf));
  }
  
  s = samples_->get_text().c_str();
  if (s != NULL && *s != 0) {
    val = atol(s);
    length += val;
    sprintf(buf, "%ld", val);
    samples_->set_text(string(buf));
  }
  
  if (length > 0) {
    unsigned long pos;

    switch (mode_) {
    case M_APPEND:
      tocEdit->appendSilence(length);
      break;
    case M_INSERT:
      if (tocEditView_->sampleMarker(&pos)) {
        if (tocEdit->insertSilence(length, pos) == 0) {
          tocEditView_->sampleSelection(pos, pos + length - 1);
        }
      }
      break;
    }
    guiUpdate();
  }
}

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: AddSilenceDialog.cc,v $
 * Revision 1.3  2000/04/23 09:07:08  andreasm
 * * Fixed most problems marked with '//llanero'.
 * * Added audio CD edit menus to MDIWindow.
 * * Moved central storage of TocEdit object to MDIWindow.
 * * AudioCdChild is now handled like an ordinary non modal dialog, i.e.
 *   it has a normal 'update' member function now.
 * * Added CdTextTable modal dialog.
 * * Old functionality of xcdrdao is now available again.
 *
 * Revision 1.2  2000/02/20 23:34:53  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:39:00  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.4  1999/08/07 16:27:28  mueller
 * Applied patch from Yves Bastide:
 * * prefixing member function names with their class name in connect_to_method
 * * explicitly `const_cast'ing a cast to const
 *
 * Revision 1.3  1999/03/06 13:55:18  mueller
 * Adapted to Gtk-- version 0.99.1
 *
 * Revision 1.2  1999/01/30 19:45:43  mueller
 * Fixes for compilation with Gtk-- 0.11.1.
 *
 */

static char rcsid[] = "$Id: AddSilenceDialog.cc,v 1.3 2000/04/23 09:07:08 andreasm Exp $";

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "AddSilenceDialog.h"

#include "TocEdit.h"
#include "guiUpdate.h"

#include "Sample.h"



AddSilenceDialog::AddSilenceDialog()
{
  Gtk::Button *button;
  Gtk::VBox *vbox;
  Gtk::HBox *hbox;

  tocEdit_ = NULL;
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

  button = new Gtk::Button(string(" Cancel "));
  bbox->pack_start(*button);
  button->show();
  button->clicked.connect(slot(this, &AddSilenceDialog::cancelAction));

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

void AddSilenceDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, tocEdit);
  show();
}

void AddSilenceDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void AddSilenceDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  if (tocEdit == NULL) {
    applyButton_->set_sensitive(FALSE);
    tocEdit_ = NULL;
    return;
  }

  if ((level & UPD_EDITABLE_STATE) || tocEdit_ == NULL) {
    applyButton_->set_sensitive(tocEdit->editable() ? TRUE : FALSE);
  }

  tocEdit_ = tocEdit;
}


gint AddSilenceDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void AddSilenceDialog::cancelAction()
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

  if (tocEdit_ == NULL || !tocEdit_->editable())
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
      tocEdit_->appendSilence(length);
      break;
    case M_INSERT:
      if (tocEdit_->sampleMarker(&pos)) {
	tocEdit_->insertSilence(length, pos);
      }
      break;
    }
    guiUpdate();
  }
}

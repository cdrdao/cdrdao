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

#include "AddFileDialog.h"

#include "TocEdit.h"
#include "TocEditView.h"
#include "guiUpdate.h"

#include "Sample.h"
#include "util.h"
#include "AudioCDProject.h"
#include "xcdrdao.h"

AddFileDialog::AddFileDialog(AudioCDProject *project) : Gtk::FileSelection(string(""))
{
  tocEditView_ = NULL;
  active_ = 0;
  project_ = project;
  
  mode(M_APPEND_TRACK);

  hide_fileop_buttons();
  ((Gtk::Label *)get_cancel_button()->get_child())->set_text("Close");

  get_ok_button()->clicked.connect(SigC::slot(this,&AddFileDialog::applyAction));
  get_cancel_button()->clicked.connect(SigC::slot(this,&AddFileDialog::closeAction));
}

AddFileDialog::~AddFileDialog()
{
}

void AddFileDialog::mode(Mode m)
{
  mode_ = m;

  switch (mode_) {
  case M_APPEND_TRACK:
    set_title(string("Append Track"));
    ((Gtk::Label *)get_ok_button()->get_child())->set_text("Append");
    break;
  case M_APPEND_FILE:
    set_title(string("Append File"));
    ((Gtk::Label *)get_ok_button()->get_child())->set_text("Append");
    break;
  case M_INSERT_FILE:
    set_title(string("Insert File"));
    ((Gtk::Label *)get_ok_button()->get_child())->set_text("Insert");
    break;
  }
}

void AddFileDialog::start(TocEditView *tocEditView)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, tocEditView);
  show();
}

void AddFileDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void AddFileDialog::update(unsigned long level, TocEditView *tocEditView)
{
  if (!active_)
    return;

  if (tocEditView == NULL) {
    get_ok_button()->set_sensitive(FALSE);
    tocEditView_ = NULL;
    return;
  }

  if ((level & UPD_EDITABLE_STATE) || tocEditView_ == NULL) {
    get_ok_button()->set_sensitive(tocEditView->tocEdit()->editable() ? TRUE : FALSE);
  }

  tocEditView_ = tocEditView;
}


gint AddFileDialog::delete_event_impl(GdkEventAny*)
{
  stop();
  return 1;
}

void AddFileDialog::closeAction()
{
  stop();
}

void AddFileDialog::applyAction()
{
  if (tocEditView_ == NULL || !tocEditView_->tocEdit()->editable())
    return;

  string str = get_filename();
  const char *s = stripCwd(str.c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
    unsigned long pos;

    switch (mode_) {
    case M_APPEND_TRACK:
      switch (tocEditView_->tocEdit()->appendTrack(s)) {
      case 0:
	guiUpdate();
	project_->statusMessage("Appended track with audio data from \"%s\".", s);
	break;
      case 1:
	project_->statusMessage("Cannot open audio file \"%s\".", s);
	break;
      case 2:
	project_->statusMessage("Audio file \"%s\" has wrong format.", s);
	break;
      }
      break;

    case M_APPEND_FILE:
      switch (tocEditView_->tocEdit()->appendFile(s)) {
      case 0:
	guiUpdate();
	project_->statusMessage("Appended audio data from \"%s\".", s);
      break;
      case 1:
	project_->statusMessage("Cannot open audio file \"%s\".", s);
	break;
      case 2:
	project_->statusMessage("Audio file \"%s\" has wrong format.", s);
	break;
      }
      break;
    case M_INSERT_FILE:
      if (tocEditView_->sampleMarker(&pos)) {
    unsigned long len;
	switch (tocEditView_->tocEdit()->insertFile(s, pos, &len)) {
	case 0:
	  tocEditView_->sampleSelection(pos, pos + len - 1);
	  guiUpdate();
	  project_->statusMessage("Inserted audio data from \"%s\".", s);
	  break;
	case 1:
	  project_->statusMessage("Cannot open audio file \"%s\".", s);
	  break;
	case 2:
	  project_->statusMessage("Audio file \"%s\" has wrong format.", s);
	  break;
	}
      }
      break;
    }

    guiUpdate();
  }
}

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
 * $Log: AddFileDialog.cc,v $
 * Revision 1.5  2000/10/01 16:39:09  llanero
 * applied Jason Lunz patch: "Close" instead of "Cancel" where appropiate.
 *
 * Revision 1.4  2000/09/22 00:21:13  llanero
 * Added Drag and Drop ability to the AudioCDView, but just
 * one file at a time, and when not playin, not reading, ...
 *
 * Revision 1.3  2000/09/21 02:07:06  llanero
 * MDI support:
 * Splitted AudioCDChild into same and AudioCDView
 * Move Selections from TocEdit to AudioCDView to allow
 *   multiple selections.
 * Cursor animation in all the views.
 * Can load more than one from from command line
 * Track info, Toc info, Append/Insert Silence, Append/Insert Track,
 *   they all are built for every child when needed.
 * ...
 *
 * Revision 1.2  2000/02/20 23:34:53  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:58  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/09/07 11:17:32  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: AddFileDialog.cc,v 1.5 2000/10/01 16:39:09 llanero Exp $";

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "AddFileDialog.h"

#include "TocEdit.h"
#include "guiUpdate.h"

#include "Sample.h"
#include "util.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "MDIWindow.h"
#include "xcdrdao.h"

AddFileDialog::AddFileDialog(AudioCDChild *child) : Gtk::FileSelection(string(""))
{
  tocEdit_ = NULL;
  active_ = 0;

  mode(M_APPEND_TRACK);

  cdchild = child;

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

void AddFileDialog::start(TocEdit *tocEdit)
{
  if (active_) {
    get_window().raise();
    return;
  }

  active_ = 1;

  update(UPD_ALL, tocEdit);
  show();
}

void AddFileDialog::stop()
{
  if (active_) {
    hide();
    active_ = 0;
  }
}

void AddFileDialog::update(unsigned long level, TocEdit *tocEdit)
{
  if (!active_)
    return;

  if (tocEdit == NULL) {
    get_ok_button()->set_sensitive(FALSE);
    tocEdit_ = NULL;
    return;
  }

  if ((level & UPD_EDITABLE_STATE) || tocEdit_ == NULL) {
    get_ok_button()->set_sensitive(tocEdit->editable() ? TRUE : FALSE);
  }

  tocEdit_ = tocEdit;
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
  if (tocEdit_ == NULL || !tocEdit_->editable())
    return;

  string str = get_filename();
  const char *s = stripCwd(str.c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
    unsigned long pos;

    switch (mode_) {
    case M_APPEND_TRACK:
      switch (tocEdit_->appendTrack(s)) {
      case 0:
	guiUpdate();
	MDI_WINDOW->statusMessage("Appended track with audio data from \"%s\".", s);
	break;
      case 1:
	MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", s);
	break;
      case 2:
	MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", s);
	break;
      }
      break;

    case M_APPEND_FILE:
      switch (tocEdit_->appendFile(s)) {
      case 0:
	guiUpdate();
	MDI_WINDOW->statusMessage("Appended audio data from \"%s\".", s);
      break;
      case 1:
	MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", s);
	break;
      case 2:
	MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", s);
	break;
      }
      break;
    case M_INSERT_FILE:
      AudioCDView *view = static_cast <AudioCDView *>(cdchild->get_active());

      if (view->sampleMarker(&pos)) {
    unsigned long len;
	switch (tocEdit_->insertFile(s, pos, &len)) {
	case 0:
      view->sampleSelection(pos, pos + len - 1);
	  guiUpdate();
	  MDI_WINDOW->statusMessage("Inserted audio data from \"%s\".", s);
	  break;
	case 1:
	  MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", s);
	  break;
	case 2:
	  MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", s);
	  break;
	}
      }
      break;
    }

    guiUpdate();
  }
}

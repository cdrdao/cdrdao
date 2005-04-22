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


#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include <gtkmm.h>
#include <gnome.h>

#include "AddFileDialog.h"
#include "guiUpdate.h"
#include "TocEdit.h"
#include "Sample.h"
#include "util.h"
#include "AudioCDProject.h"
#include "xcdrdao.h"

AddFileDialog::AddFileDialog(AudioCDProject *project)
    : Gtk::FileSelection("")
{
  active_ = false;
  project_ = project;

  set_filename("*.wav");
  show_fileop_buttons();
  set_select_multiple(true);
  set_transient_for(*project->getParentWindow ());
  mode(M_APPEND_TRACK);

  Gtk::Button* cancel = get_cancel_button();
  cancel->set_label(Gtk::Stock::CLOSE.id);
  cancel->set_use_stock(true);

  Gtk::Button* ok = get_ok_button();
  ok->set_label(Gtk::Stock::ADD.id);
  ok->set_use_stock(true);

  ok->signal_clicked().connect(sigc::mem_fun(*this,
                                             &AddFileDialog::applyAction));
  cancel->signal_clicked().connect(sigc::mem_fun(*this,
                                                 &AddFileDialog::closeAction));
}

AddFileDialog::~AddFileDialog()
{
}

void AddFileDialog::mode(Mode m)
{
  mode_ = m;

  switch (mode_) {
  case M_APPEND_TRACK:
    set_title(_("Append Track"));
    break;
  case M_APPEND_FILE:
    set_title(_("Append File"));
    break;
  case M_INSERT_FILE:
    set_title(_("Insert File"));
    break;
  }
}

void AddFileDialog::start()
{
  if (active_) {
    get_window()->raise();
    return;
  }

  active_ = true;
  set_filename("*.wav");
  show();
}

void AddFileDialog::stop()
{
  if (active_) {
    hide();
    active_ = false;
  }
}

bool AddFileDialog::on_delete_event(GdkEventAny*)
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
  Glib::ArrayHandle<std::string> sfiles = get_selections();
  std::list<std::string> files;

  for (Glib::ArrayHandle<std::string>::const_iterator i = sfiles.begin();
       i != sfiles.end(); i++) {

    const char *s = stripCwd((*i).c_str());

    if (s && *s != 0 && s[strlen(s) - 1] != '/') {

      if (fileExtension(s) == FE_M3U)
        parseM3u(s, files);
      else
        files.push_back(s);
    }
  }

  if (files.size() > 0) {
    switch (mode_) {
    case M_APPEND_TRACK:
      project_->appendTracks(files);
      break;
      
    case M_APPEND_FILE:
      project_->appendFiles(files);
      break;

    case M_INSERT_FILE:
      project_->insertFiles(files);
      break;
    }
    if (files.size() > 1)
      stop();
  }

  set_filename("*.wav");
}

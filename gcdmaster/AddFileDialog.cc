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

#include "config.h"
#include "AddFileDialog.h"
#include "guiUpdate.h"
#include "TocEdit.h"
#include "Sample.h"
#include "util.h"
#include "AudioCDProject.h"
#include "xcdrdao.h"

AddFileDialog::AddFileDialog(AudioCDProject *project)
    : Gtk::FileChooserDialog("")
{
  active_ = false;
  project_ = project;

  set_select_multiple(true);
  set_transient_for(*project->getParentWindow ());
  mode(M_APPEND_TRACK);

  Gtk::FileFilter* filter_tocs = new Gtk::FileFilter;
  manage(filter_tocs);
  std::string fname = "Audio Files (wav";
#ifdef HAVE_MP3_SUPPORT
  fname = fname + ", mp3, m3u";
#endif
#ifdef HAVE_OGG_SUPPORT
  fname = fname + ", ogg";
#endif
  fname = fname + ")";
  filter_tocs->set_name(fname);

  filter_tocs->add_pattern("*.wav");
#ifdef HAVE_OGG_SUPPORT
  filter_tocs->add_pattern("*.ogg");
#endif
#ifdef HAVE_MP3_SUPPORT
  filter_tocs->add_pattern("*.mp3");
  filter_tocs->add_pattern("*.m3u");
#endif
  add_filter(*filter_tocs);

  Gtk::FileFilter* filter_all = new Gtk::FileFilter;
  manage(filter_all);
  filter_all->set_name("Any files");
  filter_all->add_pattern("*");
  add_filter(*filter_all);

  add_button(Gtk::Stock::CLOSE, Gtk::RESPONSE_CANCEL);
  add_button(Gtk::Stock::ADD, Gtk::RESPONSE_OK);
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
  show();

  bool contFlag = true;

  while (contFlag) {

    int result = run();

    switch (result) {
    case Gtk::RESPONSE_CANCEL:
      contFlag = false;
      break;
    case Gtk::RESPONSE_OK:
      contFlag = applyAction();
      break;
    }
  }

  stop();
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

bool AddFileDialog::applyAction()
{
  std::list<Glib::ustring> sfiles = get_filenames();
  std::list<std::string> files;

  for (std::list<Glib::ustring>::const_iterator i = sfiles.begin();
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
      return false;
  }

  return true;
}

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __PROJECT_H__
#define __PROJECT_H__

#include <gtk--.h>
#include <gtk/gtk.h>
#include <gnome--.h>

class AudioCDChild;
class AudioCDView;
class TocEdit;
#include "ViewSwitcher.h"

class Project : public Gnome::App
{
protected:
  Gtk::HBox *hbox;
//  Gtk::Statusbar *statusbar_;  
  Gnome::AppBar *statusbar_;  
  Gtk::ProgressBar *progressbar_;  
  Gtk::Button *progressButton_;  
  ViewSwitcher *viewSwitcher_;
  
  int project_number;
  gint viewNumber;
  bool new_; // If it is a new project (not saved)

  TocEdit *tocEdit_;

  void createMenus();
  void createStatusbar();
  void updateWindowTitle();
  void saveProject();
  void saveAsProject();
  Gtk::FileSelection *saveFileSelector_;
  void saveFileSelectorOKCB();
  void saveFileSelectorCancelCB();

  void statusMessage(const char *fmt, ...);

  virtual int delete_event_impl(GdkEventAny *event);

public:
  Project();

  void readToc(char *name);
  virtual bool closeProject() = 0;
  void recordToc2CD();

  gint getViewNumber();
};
#endif


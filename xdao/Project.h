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
#include "ProjectChooser.h"

class Project : public Gnome::App
{
private:
  ProjectChooser *projectChooser_;
  AudioCDChild *audioCDChild_;
  Gtk::HBox *hbox;
//  Gtk::Statusbar *statusbar_;  
  Gnome::AppBar *statusbar_;  
  Gtk::ProgressBar *progressbar_;  
  Gtk::Button *progressButton_;  
  ViewSwitcher *viewSwitcher_;
  
  int project_number;
  gint viewNumber;
  bool new_; // If it is a new project (not saved)
  enum ProjectType { P_NONE, P_CHOOSER, P_AUDIOCD };
  ProjectType projectType;

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
  Project(int);

  void readToc(char *name);
  bool busy();
  void newChooserWindow();
  void newAudioCDProject(const char *name = 0);
  bool closeProject();
  void recordToc2CD();
  void configureDevices();

  gint getViewNumber();
};
#endif


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

#ifndef __GCDMASTER_H__
#define __GCDMASTER_H__

#include <gtk--.h>
#include <gtk/gtk.h>
#include <gnome--.h>

#include <list>

class ProjectChooser;
#include "Project.h"

class GCDMaster : public Gtk::Widget
{
private:
  list<Project *> projects;
  list<ProjectChooser *> choosers;

  gint project_number;

  Gnome::About *about_;  

  void add(Project *);
  void add(ProjectChooser *);

  Gtk::FileSelection *readFileSelector_;
  void readFileSelectorOKCB(ProjectChooser *projectChooser);
  void readFileSelectorCancelCB();

  int aboutDestroy();

public:
  GCDMaster();

  void appClose(Project *);
  void closeProject(Project *);
  void closeChooser(ProjectChooser *);
  bool openNewProject(const char*);
  void openProject(ProjectChooser *);
  void newChooserWindow();
  ProjectChooser* newChooserWindow2();
//FIXME: join this two: ?
  void newAudioCDProject2(ProjectChooser *);
  void newAudioCDProject(const char *name, TocEdit *tocEdit, ProjectChooser *);

  void update(unsigned long level);

  void recordCD2CD();
  void recordCD2HD();
  void configureDevices();

  void aboutDialog();
};
#endif


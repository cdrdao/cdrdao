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

#include <gtkmm.h>
#include <gnome.h>

#include "config.h"

#include "xcdrdao.h"
#include "DeviceConfDialog.h"
#include "ProjectChooser.h"
#include "gcdmaster.h"
#include "TocEdit.h"
#include "util.h"
#include "AudioCDProject.h"
#include "DuplicateCDProject.h"
#include "BlankCDDialog.h"
#include "DumpCDProject.h"
#include "Icons.h"

GCDMaster::GCDMaster()
{
  project_number = 0;
  blankCDDialog_ = 0;

  readFileSelector_.get_cancel_button()->signal_clicked().
    connect(slot(*this, &GCDMaster::readFileSelectorCancelCB));

  Icons::registerStockIcons();
}

void GCDMaster::add(Project *project)
{
  projects.push_back(project);
}

void GCDMaster::add(ProjectChooser *projectChooser)
{
  choosers.push_back(projectChooser);
}

bool GCDMaster::openNewProject(const char* s)
{
  TocEdit *tocEdit = new TocEdit(NULL, NULL);

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/')
  {
    if (tocEdit->readToc(stripCwd(s)) == 0)
    {
      //FIXME: We should test what type of project it is
      //       AudioCD, ISO. No problem now.

      //cout << "Read ok" << endl;

      newAudioCDProject(stripCwd(s), tocEdit, NULL);
    }
    else
      return false;
  }
  return true;
}

void GCDMaster::openProject(ProjectChooser *projectChooser)
{
  readFileSelector_.get_ok_button()->signal_clicked().
    connect(bind(slot(*this, &GCDMaster::readFileSelectorOKCB),
                 projectChooser));

  readFileSelector_.show();
  readFileSelector_.raise();
}

void GCDMaster::readFileSelectorCancelCB()
{
  readFileSelector_.hide();
}

void GCDMaster::readFileSelectorOKCB(ProjectChooser *projectChooser)
{
  TocEdit *tocEdit = new TocEdit(NULL, NULL);
  char *s = g_strdup(readFileSelector_.get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/')
  {
    if (tocEdit->readToc(stripCwd(s)) == 0)
    {
      //FIXME: We should test what type of project it is
      //       AudioCD, ISO. No problem now.

      // cout << "Read ok" << endl;

      newAudioCDProject(stripCwd(s), tocEdit, NULL);
      if (projectChooser)
        closeChooser(projectChooser);
    }
    else
    {
      Glib::ustring message(_("Error loading "));
      message += s;
      Gtk::MessageDialog(message, Gtk::MESSAGE_ERROR); 
    }
  }
  g_free(s);

  readFileSelectorCancelCB();
}

void GCDMaster::closeProject(Project *project)
{
  if (project->closeProject())
  {
    projects.remove(project);
    delete project;
  }

  //cout << "Number of projects = " << projects.size() << endl;
  //cout << "Number of choosers = " << choosers.size() << endl;

  if ((projects.size() == 0) && (choosers.size() == 0))
    Gnome::Main::quit(); // Quit if there are not remaining windows
}

void GCDMaster::closeChooser(ProjectChooser *projectChooser)
{
  choosers.remove(projectChooser);
  delete projectChooser;

  //cout << "Number of projects = " << projects.size() << endl;
  //cout << "Number of choosers = " << choosers.size() << endl;

  if ((projects.size() == 0) && (choosers.size() == 0))
    Gnome::Main::quit(); // Quit if there are not remaining windows
}

void GCDMaster::appClose(Project *project)
{
  if (project->closeProject())
  {
    Project *previous = 0;

    projects.remove(project);
    delete project;

    for (std::list<Project *>::iterator i = projects.begin();
         i != projects.end(); i++)
    {
	  if (previous != 0)
      {
        projects.remove(previous);
        delete (previous);
      }
      if (!((*i)->closeProject()))
        return;
      previous = *i;
    }
    Gnome::Main::quit();
  }
}

void GCDMaster::newChooserWindow()
{
  ProjectChooser *projectChooser = new ProjectChooser();
  projectChooser->show();
//  As it always can be closed, we don't add it.
  add(projectChooser);
}

ProjectChooser* GCDMaster::newChooserWindow2()
{
  ProjectChooser *projectChooser = new ProjectChooser();
  projectChooser->show();
//  As it always can be closed, we don't add it.
  add(projectChooser);
  return projectChooser;
}

void GCDMaster::newAudioCDProject(const char *name, TocEdit *tocEdit, ProjectChooser *projectChooser)
{
  AudioCDProject *project = new AudioCDProject(project_number++, name, tocEdit);
  add(project);
  project->show();
  if (projectChooser)
    closeChooser(projectChooser);
}

void GCDMaster::newAudioCDProject2(ProjectChooser *projectChooser)
{
  AudioCDProject *project = new AudioCDProject(project_number++, "", NULL);
  add(project);
  // NOTE: We can't show the Gnome::UI::App here, because it also
  // shows all the DockItems it contains, and the viewSwitcher will
  // take care of this.  project->show();
  if (projectChooser)
    closeChooser(projectChooser);
}

void GCDMaster::newDuplicateCDProject(ProjectChooser *projectChooser)
{
  DuplicateCDProject *project = new DuplicateCDProject();
  add(project);
  project->show();
  if (projectChooser)
    closeChooser(projectChooser);
}

void GCDMaster::newDumpCDProject(ProjectChooser *projectChooser)
{
  DumpCDProject *project = new DumpCDProject();
  add(project);
  project->show();
  if (projectChooser)
    closeChooser(projectChooser);
}

void GCDMaster::update(unsigned long level)
{
  for (std::list<Project *>::iterator i = projects.begin();
       i != projects.end(); i++)
  {
    (*i)->update(level);
  }

  if (blankCDDialog_ != 0)
    blankCDDialog_->update(level);
}

void GCDMaster::configureDevices()
{
  deviceConfDialog->start();
}

void GCDMaster::blankCDRW(Project* project)
{
  if (!blankCDDialog_)
    blankCDDialog_ = new BlankCDDialog;

  blankCDDialog_->start(*project);
}

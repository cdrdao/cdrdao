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

#include <gnome.h>

#include "xcdrdao.h"
#include "DeviceConfDialog.h"
#include "RecordGenericDialog.h"
#include "gcdmaster.h"

GCDMaster::GCDMaster()
{
  project_number = 0;
  about_ = 0;
  readFileSelector_ = 0;

}

void GCDMaster::add(Project *project)
{
  projects.push_back(project);
cout << "Number of projects = " << projects.size() << endl;
}

void GCDMaster::openProject(Project *project)
{
  if (readFileSelector_)
  {
    Gdk_Window selector_win = readFileSelector_->get_window();
    selector_win.show();
    selector_win.raise();
  }
  else
  {
    readFileSelector_ = new Gtk::FileSelection("Open project");
    readFileSelector_->get_ok_button()->clicked.connect(
				bind(slot(this, &GCDMaster::readFileSelectorOKCB), project));
    readFileSelector_->get_cancel_button()->clicked.connect(
				slot(this, &GCDMaster::readFileSelectorCancelCB));
  }

  readFileSelector_->show();
}

void GCDMaster::readFileSelectorCancelCB()
{
  readFileSelector_->hide();
  readFileSelector_->destroy();
  readFileSelector_ = 0;
}

void GCDMaster::readFileSelectorOKCB(Project *project)
{
  char *s = g_strdup(readFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/')
  {
    if (project->busy())
    {
      project = new Project(project_number);
      add(project);
    }
    project->readToc(s);
  }
  g_free(s);

  readFileSelectorCancelCB();
}

void GCDMaster::closeProject(Project *project)
{
  if (project->closeProject())
  {
    delete project;
    projects.remove(project);
  }
cout << "Number of projects = " << projects.size() << endl;
  if (projects.size() == 0)
    appClose();
}

void GCDMaster::appClose()
{
  for (list<Project *>::iterator i = projects.begin();
       i != projects.end(); i++)
  {
    if (!((*i)->closeProject()))
      return;
  }
  Gnome::Main::quit();
}

void GCDMaster::newChooserWindow()
{
  Project *project = new Project(project_number);
  project->newChooserWindow();
  add(project);
}

void GCDMaster::newAudioCDProject(const char *name)
{
  Project *project = new Project(project_number);
  project->newAudioCDProject(name);
  add(project);
}

void GCDMaster::recordCD2CD()
{
  RECORD_GENERIC_DIALOG->cd_to_cd();
}

void GCDMaster::recordCD2HD()
{
  RECORD_GENERIC_DIALOG->cd_to_hd();
}

void GCDMaster::configureDevices()
{
  DEVICE_CONF_DIALOG->start();
}

void GCDMaster::aboutDialog()
{

  if (about_) // "About" dialog hasn't been closed, so just raise it
  {
      Gdk_Window about_win = about_->get_window();
      about_win.show();
      about_win.raise();
  }
  else
  {
    gchar *logo_char;
    string logo;
    vector<string> authors;
    authors.push_back("Andreas Mueller <mueller@daneb.ping.de>");
    authors.push_back("Manuel Clos <llanero@jazzfree.com>");

//FIXME: not yet wrapped
    logo_char = gnome_pixmap_file("gcdmaster.png");

    if (logo_char != NULL)
      logo = logo_char;

    about_ = new Gnome::About(_("gcdmaster"), "1.1.4",
                               "(C) Andreas Mueller",
                               authors,
                               _("A CD Mastering app for Gnome."),
                               logo);

//    about_->set_parent(*this);
//    about_->destroy.connect(slot(this, &GCDMaster::aboutDestroy));
    // We attach to the close signal because default behaviour is
    // close_hides = true, as in a normal dialog
    about_->close.connect(slot(this, &GCDMaster::aboutDestroy));
    about_->show();
  }
}

int GCDMaster::aboutDestroy()
{
  cout << "about closed" << endl;
  delete about_;
  about_ = 0;
  return true; // Do not close, as we have already deleted it.
}

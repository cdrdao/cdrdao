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
#include "RecordGenericDialog.h"
#include "gcdmaster.h"

GCDMaster::GCDMaster()
{
  project_number = 0;
  about_ = 0;
}

void GCDMaster::add(Project *project)
{
  projects.push_back(project);
}

void GCDMaster::closeProject(Project *project)
{
  if (project->closeProject())
    projects.remove(project);
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

void GCDMaster::aboutDialog()
{

  if(about_) // "About" box hasn't been closed, so just raise it
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

//      about_->set_parent(*this);
      about_->destroy.connect(slot(this, &GCDMaster::aboutDestroy));
      about_->show();
    }
}

void GCDMaster::aboutDestroy()
{
  about_ = 0;
}

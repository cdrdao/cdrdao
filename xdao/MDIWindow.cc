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

#include <stdio.h>
#include <fstream.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <strstream.h>

#include <gnome.h>

#include "util.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "MessageBox.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "DeviceConfDialog.h"
#include "RecordGenericDialog.h"
#include "TocInfoDialog.h"
#include "TrackInfoDialog.h"

#include "MDIWindow.h"

void
MDIWindow::nothing_cb()
{
  cout << "nothing here" << endl;
}

MDIWindow::MDIWindow()
  : Gnome::MDI("GnomeCDMaster", "Gnome CD Master")
{
  // Toolbar
  vector<Gnome::UI::Info> toolbarTree;

  {
/*
    using namespace Gnome::UI;
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_NEW),
								N_("New"),
								slot(this, &MDIWindow::newAudioCDProject),
								N_("New Audio CD Project")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_OPEN),
								N_("Open"),
								slot(this, &MDIWindow::readProject),
								N_("Open a project")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_SAVE),
								N_("Save"),
								slot(this, &MDIWindow::saveProject),
								N_("Save current project")));
    toolbarTree.push_back(Separator());

    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_CDROM),
								N_("Record"),
								slot(this, &MDIWindow::recordToc2CD),
								N_("Record to CD")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_CDROM),
								N_("CD to CD"),
								slot(this, &MDIWindow::recordCD2CD),
								N_("CD duplication")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_CDROM),
								N_("Dump CD"),
								slot(this, &MDIWindow::recordCD2HD),
								N_("Dump CD to disk")));
    toolbarTree.push_back(Separator());

    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_PREFERENCES),
								N_("Devices"),
								slot(this, &MDIWindow::configureDevices),
								N_("Configure devices")));
*/
/*
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_PREFERENCES),
								N_("Prefs"),
								slot(this, &MDIWindow::nothing_cb),
								N_("Preferences")));

    toolbarTree.push_back(Separator());

    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_QUIT),
								N_("Quit"),
								slot(this, &MDIWindow::app_close),
								N_("Quit application")));
*/
  }

//FIXME  set_toolbar_template(toolbarTree);


//  delete_event.connect(slot(this, &MDIWindow::delete_event_cb));

//FIXME  child_changed.connect(slot(this, &MDIWindow::child_changed_cb));

  set_child_menu_path("_File");
  set_child_list_path("_Windows/");

  childs = g_list_alloc();

  child_number = 0;
  about_ = 0;

}

void 
MDIWindow:: app_created_impl(Gnome::App& app)
{
  Gnome::AppBar *appBar = new Gnome::AppBar(false, true, GNOME_PREFERENCES_NEVER);

  app.set_statusbar(*appBar);
  app.install_menu_hints();
  app.set_policy(false, true, false);
  app.set_default_size(600, 400);
//  app.set_usize(600, 400);
  app.set_wmclass("GCDMaster", "GCDMaster");
}


void MDIWindow::update(unsigned long level)
{
//FIXME: Here we should update the menus and the icons
//       this is, enabled/disabled. How to get menus in gnome--?
/*
  childs = g_list_first(childs);
  if (childs->data)
    //enable record
  else
    //disable record
*/

//FIXME:  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
//FIXME:    string s(tocEdit_->filename());

//FIXME:    if (tocEdit_->tocDirty())
//FIXME:      s += "(*)";

//NOTE: child_changed still not in gnome-- (for 1.1.13)

//FIXME: Do this on every "child_changed" signal, not here.    
//FIXME:    get_active_window()->set_title(s);
//    set_title(s);
//FIXME:  }

//FIXME: Update to all childs (and views) or just the current one???
  // send update to active child only
//  audioCdChild_->update(level, tocEdit_);
}



void MDIWindow::tocBlockedMsg(const char *op)
{
  MessageBox msg(this->get_active_window(), op, 0,
//  MessageBox msg(this, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();

}


void MDIWindow::configureDevices()
{
  DEVICE_CONF_DIALOG->start();
}

void MDIWindow::recordToc2CD()
{
//FIXME
/*
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->record_to_cd();
*/
}


void MDIWindow::closeProject()
{
/*
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    if (child->closeProject())
    {
      remove(*child);
      childs = g_list_remove(childs, child);
      guiUpdate();
    }
*/
}

void MDIWindow::readProject()
{
/*
  if (readFileSelector_)
  {
    Gdk_Window selector_win = readFileSelector_->get_window();
    selector_win.show();
    selector_win.raise();
  }
  else
  {
    readFileSelector_ = new Gtk::FileSelection("Read Project");
    readFileSelector_->get_ok_button()->clicked.connect(
				slot(this, &MDIWindow::readFileSelectorOKCB));
    readFileSelector_->get_cancel_button()->clicked.connect(
				slot(this, &MDIWindow::readFileSelectorCancelCB));
  }

  readFileSelector_->show();
*/
}

void MDIWindow::saveProject()
{
/*  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->saveProject();
*/
}

void MDIWindow::saveAsProject()
{
/*
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->saveAsProject();
*/
}

void MDIWindow::readFileSelectorCancelCB()
{
  readFileSelector_->hide();
  readFileSelector_->destroy();
  readFileSelector_ = 0;
}

void MDIWindow::readFileSelectorOKCB()
{
  char *s = g_strdup(readFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
//FIXME: We should test what type of project it is
//       AudioCD, ISO. No problem now.
//    MDI_WINDOW->openAudioCDProject(s);
  }
  g_free(s);

  readFileSelectorCancelCB();
}

void
MDIWindow::about_cb()
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

//FIXME: not yet wrapped - sorry
      logo_char = gnome_pixmap_file("gcdmaster.png");

      if (logo_char != NULL)
        logo = logo_char;

      about_ = new Gnome::About(_("gcdmaster"), "1.1.4",
                               "(C) Andreas Mueller",
                               authors,
                               _("A CD Mastering app for Gnome."),
                               logo);

      about_->set_parent(*this->get_active_window());
      about_->destroy.connect(slot(this, &MDIWindow::about_destroy_cb));
      about_->show();
    }
}

void MDIWindow::about_destroy_cb()
{
  about_ = 0;
}

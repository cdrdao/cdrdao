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

#include <glade/glade.h>

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
  vector<Gnome::UI::Info> menus, newMenuTree, fileMenuTree, actionsMenuTree;
  vector<Gnome::UI::Info> settingsMenuTree, helpMenuTree, windowsMenuTree;

  {
    using namespace Gnome::UI;
    // File->New menu
    newMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_NEW),
								N_("Audio CD"),
								slot(this, &MDIWindow::newAudioCDProject),
								N_("New Audio CD")));

    // File menu
    fileMenuTree.push_back(SubTree(Icon(GNOME_STOCK_MENU_NEW),
							    N_("New"),
							    newMenuTree,
							    "Create a new project"));
  }

  {
    using namespace Gnome::MenuItems;
    fileMenuTree.push_back(Open(slot(this, &MDIWindow::readProject)));
    fileMenuTree.push_back(Save(slot(this, &MDIWindow::saveProject)));
    fileMenuTree.push_back(SaveAs(slot(this, &MDIWindow::saveAsProject)));

    fileMenuTree.push_back(Gnome::UI::Separator());
/*
    fileMenuTree.push_back(PrintSetup(slot(this, &MDIWindow::nothing_cb)));

    fileMenuTree.push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_PRINT),
								 N_("Print Cover..."),
								 slot(this, &MDIWindow::nothing_cb),
								 N_("Print Cover")));

    fileMenuTree.push_back(Gnome::UI::Separator());
*/
    // Close the current child (project);
    fileMenuTree.push_back(Close(slot(this, &MDIWindow::closeProject)));
    fileMenuTree.push_back(Exit(slot(this, &MDIWindow::app_close)));
  }

  {
    using namespace Gnome::UI;
    // Actions menu
    actionsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_CDROM),
								N_("_Record"),
								slot(this, &MDIWindow::recordToc2CD),
								N_("Record")));
    actionsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_CDROM),
								N_("_CD to CD copy"),
								slot(this, &MDIWindow::recordCD2CD),
								N_("CD to CD copy")));
    actionsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_CDROM),
								N_("_Dump CD to disk"),
								slot(this, &MDIWindow::recordCD2HD),
								N_("Dump CD to disk")));
/*
    actionsMenuTree.push_back(Gnome::UI::Item(N_("Fixate CD"),
					    slot(this, &MDIWindow::nothing_cb)));
    actionsMenuTree.push_back(Gnome::UI::Item(N_("Blank CD-RW"),
					    slot(this, &MDIWindow::nothing_cb)));
    actionsMenuTree.push_back(Gnome::UI::Item(N_("Get Info"),
					    slot(this, &MDIWindow::nothing_cb)));
*/
    // Settings menu
    settingsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_PREF),
								N_("Configure Devices..."),
								slot(this, &MDIWindow::configureDevices)));
  }
/*
    settingsMenuTree.push_back(Gnome::MenuItems::Preferences
  				(slot(this, &MDIWindow::nothing_cb)));
*/

  // Help menu
  //helpMenuTree.push_back(Gnome::UI::Help("Quick Start"));

  helpMenuTree.push_back(Gnome::MenuItems::About
  				(slot(this, &MDIWindow::about_cb)));

  {
    using namespace Gnome::Menus;
    menus.push_back(File(fileMenuTree));
    menus.push_back(Gnome::UI::Menu(N_("_Actions"), actionsMenuTree));
    menus.push_back(Settings(settingsMenuTree));
    menus.push_back(Windows(windowsMenuTree));
    menus.push_back(Help(helpMenuTree));
  }

  set_menubar_template(menus);

  // Toolbar
  vector<Gnome::UI::Info> toolbarTree;

  {
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
/*
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_PREFERENCES),
								N_("Prefs"),
								slot(this, &MDIWindow::nothing_cb),
								N_("Preferences")));
*/
    toolbarTree.push_back(Separator());

    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_QUIT),
								N_("Quit"),
								slot(this, &MDIWindow::app_close),
								N_("Quit application")));
  }

  set_toolbar_template(toolbarTree);

  readFileSelector_ = 0;

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

void MDIWindow::app_close()
{
  childs = g_list_first(childs);
//FIXME: g_list_foreach in C++ ?
  while (childs->data)
  {
    GenericChild *child = static_cast <GenericChild *>(childs->data);

    if (child->closeProject())
    {
//FIXME: test type of child, and remove all childs that refer to the same
//       project.
      childs = g_list_remove(childs, child);
      remove(*child);
//      guiUpdate();
    }
    else
      return;
  }

/*
// Broken

  GList *children = gtkobj()->children;

  while (children)
  {
cout<< "called" <<endl;
    GenericChild *child = static_cast <GenericChild *>(children->data);
    if (child)
//    if (child->closeProject())
    {
      remove(*child);
//      guiUpdate();
    }
    children = children->next;
  }  
*/
/*
// Broken too :(

  for (MDIList::iterator i=children().begin();
       i!=children().end();
       ++i)
  {
    GenericChild *child = dynamic_cast <GenericChild *>(*i);
cout<< "called = " << *i <<endl;
    if (child)
//    if (child->closeProject())
    {
//      remove(*child);
//      guiUpdate();
    }
  }
*/

//  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

//  if (!child)
//    Gnome::Main::quit();

  childs = g_list_first(childs);
  if (!childs->data)
    Gnome::Main::quit();

}

gint 
MDIWindow::delete_event_impl(GdkEventAny* e)
{
  app_close();

  /* Prevent the window's destruction, since we destroyed it 
   * ourselves with app_close()
   */
  return false;
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

void MDIWindow::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  strstream str;

  str.vform(fmt, args);
  str << ends;

//FIXME  statusBar_->messages().clear();
//FIXME  statusBar_->push(1, string(str.str()));
//FIXME: Does this the trick?:
  Gnome::App *app = get_active_window();
  app->flash(str.str());

  str.freeze(0);

  va_end(args);
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
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->record_to_cd();
}

void MDIWindow::recordCD2CD()
{
  RECORD_GENERIC_DIALOG->cd_to_cd();
}

void MDIWindow::recordCD2HD()
{
  RECORD_GENERIC_DIALOG->cd_to_hd();
}

void MDIWindow::newAudioCDProject()
{
  AudioCDChild *child;
  bool new_window;
  
  child = manage(new AudioCDChild(++child_number));
    
  add(*child);
  
  new_window = !(childs->data == NULL);
  
  childs = g_list_prepend(childs, child);

  if (new_window)
    child->create_toplevel_view();
  else
    child->create_view();

  guiUpdate();
}

void MDIWindow::openAudioCDProject(char *name)
{
  AudioCDChild *child;
  bool new_window;

  child = manage(new AudioCDChild(++child_number));
  
  add(*child);

  new_window = !(childs->data == NULL);

  childs = g_list_prepend(childs, child);

  if (new_window)
    child->create_toplevel_view();
  else
    child->create_view();

  if (child->tocEdit()->readToc(stripCwd(name)) == 0)
  {
    AudioCDView *view;
    view = static_cast <AudioCDView *>(child->get_active());
    view->tocEditView()->sampleViewFull();
  }
  else
  {
    gchar *message;
    
    message = g_strdup_printf("Error loading %s", name);
    Gnome::Dialogs::error(message); 
    MDI_WINDOW->remove(*child);

    g_free(message);
  }

  //guiUpdate();
}

void MDIWindow::closeProject()
{
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    if (child->closeProject())
    {
      remove(*child);
      guiUpdate();
    }
}

void MDIWindow::readProject()
{
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
}

void MDIWindow::saveProject()
{
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->saveProject();
}

void MDIWindow::saveAsProject()
{
  GenericChild *child = static_cast <GenericChild *>(this->get_active_child());

  if (child)
    child->saveAsProject();
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
    MDI_WINDOW->openAudioCDProject(s);
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

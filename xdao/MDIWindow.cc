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

#include "util.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "TocEdit.h"
#include "MDIWindow.h"
#include "MessageBox.h"
#include "AudioCDChild.h"
#include "DeviceConfDialog.h"
#include "RecordDialog.h"
#include "ExtractDialog.h"
#include "TocInfoDialog.h"
#include "TrackInfoDialog.h"

void
MDIWindow::nothing_cb()
{
  cout << "nothing here" << endl;
}

void
MDIWindow::install_menus_and_toolbar()
{
  vector<Gnome::UI::Info> menus, newMenuTree, fileMenuTree, audioEditMenuTree;
  vector<Gnome::UI::Info> actionsMenuTree, settingsMenuTree, helpMenuTree;
  // File->New menu
  //

  newMenuTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_NEW),
					 N_("Audio CD"),
					 slot(this, &MDIWindow::newProject),
					 N_("New Audio CD")));
  newMenuTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_NEW),
					 N_("Data CD"),
					 slot(this, &MDIWindow::newProject),
					 N_("New Data CD")));
  newMenuTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_NEW),
					 N_("Mixed CD"),
					 slot(this, &MDIWindow::newProject),
					 N_("New Mixed CD")));

  // File menu
  //
  
  fileMenuTree.push_back(Gnome::UI::SubTree(Gnome::Icon(GNOME_STOCK_MENU_NEW),
					     N_("New"),
					     newMenuTree,
					    "Create a new compilation"));

  fileMenuTree.push_back(Gnome::MenuItems::Open
			 (slot(this, &MDIWindow::readProject)));

  fileMenuTree.push_back(Gnome::MenuItems::Save
  				(slot(this, &MDIWindow::saveProject)));

  fileMenuTree.push_back(Gnome::MenuItems::SaveAs
  				(slot(this, &MDIWindow::saveAsProject)));

  fileMenuTree.push_back(Gnome::UI::Separator());

  fileMenuTree.push_back(Gnome::MenuItems::PrintSetup
  				(slot(this, &MDIWindow::nothing_cb)));

  fileMenuTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_PRINT),
					 N_("Print Cover..."),
					 slot(this, &MDIWindow::nothing_cb),
					 N_("Print Cover")));

  fileMenuTree.push_back(Gnome::UI::Separator());

//This Close refers to close the current (selected) child
  fileMenuTree.push_back(Gnome::MenuItems::Close
  				(slot(this, &MDIWindow::nothing_cb)));

  fileMenuTree.push_back(Gnome::MenuItems::Exit
  				(slot(this, &MDIWindow::app_close)));

  menus.push_back(Gnome::Menus::File(fileMenuTree));

// The Edit Menu should be a per child menu, so every child knows how
// to do the copy, cut and paste operations. And also more operations,
// like in the Audio editing (Insert file, Insert Silence, ...)
// It is here for fast copy and paste ;)
  // Edit menu
  //
  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Project Info..."),
			      slot(this, &MDIWindow::projectInfo),
			      N_("Edit global project data")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Track Info..."),
			      slot(this, &MDIWindow::trackInfo),
			      N_("Edit track data")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_CUT),
			      N_("Cut"),
			      slot(audioCdChild_, &AudioCDChild::cutTrackData),
			      N_("Cut out selected samples")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_PASTE),
			      N_("Paste"),
			      slot(audioCdChild_,
				   &AudioCDChild::pasteTrackData),
			      N_("Paste previously cut samples")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Track Mark"),
			      slot(audioCdChild_, &AudioCDChild::addTrackMark),
			      N_("Add track marker at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Index Mark"),
			      slot(audioCdChild_, &AudioCDChild::addIndexMark),
			      N_("Add index marker at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Add Pre-Gap"),
			      slot(audioCdChild_, &AudioCDChild::addPregap),
			      N_("Add pre-gap at current marker position")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Remove Track Mark"),
			      slot(audioCdChild_, &AudioCDChild::removeTrackMark),
			      N_("Remove selected track/index marker or pre-gap")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append Track"),
			      slot(audioCdChild_, &AudioCDChild::appendTrack),
			      N_("Append track with data from audio file")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append File"),
			      slot(audioCdChild_, &AudioCDChild::appendFile),
			      N_("Append data from audio file to last track")));
  
  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Insert File"),
			      slot(audioCdChild_, &AudioCDChild::insertFile),
			      N_("Insert data from audio file at current marker position")));

  audioEditMenuTree.push_back(Gnome::UI::Separator());

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Append Silence"),
			      slot(audioCdChild_, &AudioCDChild::appendSilence),
			      N_("Append silence to last track")));

  audioEditMenuTree.
    push_back(Gnome::UI::Item(N_("Insert Silence"),
			      slot(audioCdChild_, &AudioCDChild::insertSilence),
			      N_("Insert silence at current marker position")));

  menus.push_back(Gnome::Menus::Edit(audioEditMenuTree));

  // Actions menu
  //
  actionsMenuTree.push_back(Gnome::UI::Item(N_("Duplicate CD"),
					    slot(this, &MDIWindow::extract)));
  actionsMenuTree.push_back(Gnome::UI::Item(N_("Record"),
					    slot(this, &MDIWindow::record)));
  actionsMenuTree.push_back(Gnome::UI::Item(N_("Fixate CD"),
					    slot(this, &MDIWindow::nothing_cb)));
  actionsMenuTree.push_back(Gnome::UI::Item(N_("Blank CD-RW"),
					    slot(this, &MDIWindow::nothing_cb)));
  actionsMenuTree.push_back(Gnome::UI::Item(N_("Get Info"),
					    slot(this, &MDIWindow::nothing_cb)));

  menus.push_back(Gnome::UI::Menu(N_("_Actions"), actionsMenuTree));

  // Settings menu
  //
  settingsMenuTree.
    push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_MENU_PREF),
			      N_("Configure Devices..."),
			      slot(this, &MDIWindow::configureDevices)));

  settingsMenuTree.push_back(Gnome::MenuItems::Preferences
  				(slot(this, &MDIWindow::nothing_cb)));

  menus.push_back(Gnome::Menus::Settings(settingsMenuTree));


  // Help menu
  //
  //helpMenuTree.push_back(Gnome::UI::Help("Quick Start"));

  helpMenuTree.push_back(Gnome::MenuItems::About
  				(slot(this, &MDIWindow::nothing_cb)));

  menus.push_back(Gnome::Menus::Help(helpMenuTree));


//FIXME: MDI STUFF  set_menubar_template(*menus);
  create_menus(menus);
  
  // Toolbar
  //
  vector<Gnome::UI::Info> toolbarTree;

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_NEW),
					N_("New"),
					slot(this, &MDIWindow::newProject),
					N_("Create a new project")));

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_OPEN),
					N_("Open"),
					slot(this, &MDIWindow::readProject),
					N_("Open a project")));

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_SAVE),
					N_("Save"),
					slot(this, &MDIWindow::saveProject),
					N_("Save current project")));

  toolbarTree.push_back(Gnome::UI::Separator());

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_PREFERENCES),
					N_("Devices"),
					slot(this, &MDIWindow::configureDevices),
					N_("Configure devices")));

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_PREFERENCES),
				      N_("Prefs"),
				      slot(this, &MDIWindow::nothing_cb),
				      N_("Preferences")));

  toolbarTree.push_back(Gnome::UI::Separator());

  toolbarTree.push_back(Gnome::UI::Item(Gnome::Icon(GNOME_STOCK_PIXMAP_QUIT),
					N_("Quit"),
					slot(this, &MDIWindow::app_close),
					N_("Quit application")));

//FIXME: MDI STUFF  set_toolbar_template(*toolbarTree);
  create_toolbar(toolbarTree);

  install_menu_hints();
}

MDIWindow::MDIWindow(TocEdit *tedit)
//FIXME: MDI STUFF  : Gnome::MDI("StillNoName", "StillNoTitle")
  : Gnome::App("StillNoName", "StillNoTitle"),
    readSaveFileSelector_("")
{
  tocEdit_ = tedit;

  readSaveOperation_ = 0;

//  set_policy(false, true, false);
  set_default_size(600, 400);
  set_usize(600, 400);

//  set_wmclass("StillNoClass", "StillNoClass");

  readSaveFileSelector_.get_ok_button()->clicked.connect(slot(this, &MDIWindow::readWriteFileSelectorOKCB));
  readSaveFileSelector_.get_cancel_button()->clicked.connect(slot(this, &MDIWindow::readWriteFileSelectorCancelCB));

  audioCdChild_ = new AudioCDChild();

  statusBar_ = new Gtk::Statusbar;
  set_statusbar(*statusBar_);
  
  install_menus_and_toolbar();

  //FIXME: MDI STUFF  MDI_WINDOW->add_child(*AUDIOCD_CHILD);
  //FIXME: MDI STUFF  MDI_WINDOW->add_view(*AUDIOCD_CHILD);
  set_contents(*audioCdChild_->vbox_);

  //delete_event.connect(slot(this, &MDIWindow::delete_event_cb));

}

void MDIWindow::app_close()
{
  if (tocEdit_->tocDirty()) {
    Ask2Box msg(this, "Quit", 0, 2, "Current work not saved.", "",
		"Really Quit?", NULL);
    if (msg.run() != 1)
      return;
  }

//  destroy();
  
//  hide();
//  MDIWindow::remove_all(0);
//  Gnome::Main::quit();
  Gtk::Main::quit();

}

gint 
MDIWindow::delete_event_impl(GdkEventAny* e)
{
  app_close();

  /* Prevent the window's destruction, since we destroyed it 
   * ourselves with app_close()
   */
  return true;
}


void MDIWindow::update(unsigned long level)
{
  if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
    string s(tocEdit_->filename());

    if (tocEdit_->tocDirty())
      s += "(*)";
    
    set_title(s);
  }

  // send update to active child only
  audioCdChild_->update(level, tocEdit_);
}

void MDIWindow::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  strstream str;

  str.vform(fmt, args);
  str << ends;

  statusBar_->messages().clear();

  statusBar_->push(1, string(str.str()));

  str.freeze(0);

  va_end(args);
}


void MDIWindow::tocBlockedMsg(const char *op)
{
  MessageBox msg(this, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();

}


/*
GtkWidget *
example_creator(GnomeMDIChild *child, gpointer data)
{
  GladeXML *xml;
  GtkWidget *new_view;       
//  GtkWidget *new_view = gtk_vbox_new(TRUE, TRUE);

  xml = glade_xml_new ("./glade/record.glade", "hbox1");
  new_view = glade_xml_get_widget (xml, "hbox1");
  glade_xml_signal_autoconnect(xml);

Gtk::Widget *view2 = Gtk::wrap(new_view);

        return new_view;
}


void
MDIWindow::example_child()
{
Gnome::MDIGenericChild *example = new Gnome::MDIGenericChild("example");
example->set_view_creator(example_creator, NULL);

MDIWindow::add_child(*example);
MDIWindow::add_view(*example);
}
*/


void MDIWindow::configureDevices()
{
  DEVICE_CONF_DIALOG->start(tocEdit_);
}

void MDIWindow::extract()
{
  EXTRACT_DIALOG->start(tocEdit_);
}

void MDIWindow::record()
{
  RECORD_DIALOG->start(tocEdit_);
}

void MDIWindow::trackInfo()
{
  TRACK_INFO_DIALOG->start(tocEdit_);
}

void MDIWindow::projectInfo()
{
  TOC_INFO_DIALOG->start(tocEdit_);
}

void MDIWindow::newProject()
{
  if (!tocEdit_->editable()) {
    tocBlockedMsg("New Project");
    return;
  }

  if (tocEdit_->tocDirty()) {
    Ask2Box msg(this, "New", 0, 2, "Current project not saved.", "",
		"Continue?", NULL);
    if (msg.run() != 1)
      return;
  }

  Toc *toc = new Toc;
  
  tocEdit_->toc(toc, "unnamed.toc");

	  guiUpdate();
}

void MDIWindow::readProject()
{
  readSaveFileSelector_.set_title("Read Project");
  readSaveOperation_ = 1;

  readSaveFileSelector_.show();
  
}

void MDIWindow::saveProject()
{
  if (tocEdit_->saveToc() == 0) {
    statusMessage("Project saved to \"%s\".", tocEdit_->filename());
    guiUpdate();
  }
  else {
    string s("Cannot save toc to \"");
    s += tocEdit_->filename();
    s+= "\":";
    
    MessageBox msg(this, "Save Project", 0, s.c_str(), strerror(errno), NULL);
    msg.run();
  }
}

void MDIWindow::saveAsProject()
{
  readSaveFileSelector_.set_title("Save Project");
  readSaveOperation_ = 2;

  readSaveFileSelector_.show();
}

void MDIWindow::readWriteFileSelectorCancelCB()
{
  readSaveFileSelector_.hide();
}

void MDIWindow::readWriteFileSelectorOKCB()
{
  if (readSaveOperation_ == 1) {
    if (!tocEdit_->editable()) {
      tocBlockedMsg("Read Project");
      return;
    }

    if (tocEdit_->tocDirty()) {
      Ask2Box msg(this, "Read Project", 0, 2, "Current work not saved.", "",
		  "Continue?", NULL);
      if (msg.run() != 1)
	return;
    }

    const char *s = readSaveFileSelector_.get_filename().c_str();

    if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
      if (tocEdit_->readToc(stripCwd(s)) == 0) {
	tocEdit_->sampleViewFull();
	guiUpdate();
      }
    }
  }
  else if (readSaveOperation_ == 2) {
    const char *s = readSaveFileSelector_.get_filename().c_str();

    if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
      if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
	statusMessage("Project saved to \"%s\".", tocEdit_->filename());
	guiUpdate();
      }
      else {
	string m("Cannot save toc to \"");
	m += tocEdit_->filename();
	m += "\":";
    
	MessageBox msg(this, "Save Project", 0, m.c_str(), strerror(errno), NULL);
	msg.run();
      }
    }
  }

  readSaveFileSelector_.hide();
}


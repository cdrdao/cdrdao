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

  using namespace Gnome::Menu_Helpers;
  
  // File->New menu
  //
  Gnome::UIInfoTree *newMenuTree = new Gnome::UIInfoTree();
  newMenuTree->push_item(Gnome::UIItem(N_("Audio CD"), N_("New Audio CD"),
  				GNOME_STOCK_MENU_NEW,
  				slot(this, &MDIWindow::newProject)));
  newMenuTree->push_item(Gnome::UIItem(N_("Data CD"), N_("New Data CD"),
  				GNOME_STOCK_MENU_NEW,
  				slot(this, &MDIWindow::newProject)));
  newMenuTree->push_item(Gnome::UIItem(N_("Mixed CD"), N_("New Mixed CD"),
  				GNOME_STOCK_MENU_NEW,
  				slot(this, &MDIWindow::newProject)));

  StockMenuItems::NewSubtree *newMenu = new 
  				StockMenuItems::NewSubtree(*newMenuTree);
//  Gnome::UIItem *newMenu = new Gnome::UISubtree(N_("New"),
//  				*newMenuTree, GNOME_STOCK_MENU_NEW);

  // File menu
  //
  Gnome::UIInfoTree *fileMenuTree = new Gnome::UIInfoTree();
  
/*  fileMenuTree->push_item(StockMenuItems::New(N_("New"),
  				("Create a new compilation"),
				slot(this, &MDIWindow::nothing_cb)));
*/
  fileMenuTree->push_item(*newMenu);

  fileMenuTree->push_item(StockMenuItems::Open
  				(slot(this, &MDIWindow::readProject)));

  fileMenuTree->push_item(StockMenuItems::Save
  				(slot(this, &MDIWindow::saveProject)));

  fileMenuTree->push_item(StockMenuItems::SaveAs
  				(slot(this, &MDIWindow::saveAsProject)));

  fileMenuTree->push_item(Gnome::UISeparator());

  fileMenuTree->push_item(StockMenuItems::PrintSetup
  				(slot(this, &MDIWindow::nothing_cb)));

  fileMenuTree->push_item(Gnome::UIItem(N_("Print Cover..."), N_("Print Cover"),
  				GNOME_STOCK_MENU_PRINT,
  				slot(this, &MDIWindow::nothing_cb)));

  fileMenuTree->push_item(Gnome::UISeparator());

//This Close refers to close the current (selected) child
  fileMenuTree->push_item(StockMenuItems::Close
  				(slot(this, &MDIWindow::nothing_cb)));

  fileMenuTree->push_item(StockMenuItems::Exit
  				(slot(this, &MDIWindow::app_close)));

  StockMenus::File *fileMenu = new StockMenus::File(*fileMenuTree);

// The Edit Menu should be a per child menu, so every child knows how
// to do the copy, cut and paste operations. And also more operations,
// like in the Audio editing (Insert file, Insert Silence, ...)
// It is here for fast copy and paste ;)
  // Edit menu
  //
  Gnome::UIInfoTree *audioEditMenuTree = new Gnome::UIInfoTree();
  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Project Info..."),
			    N_("Edit global project data"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(this, &MDIWindow::projectInfo)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Track Info..."),
			    N_("Edit track data"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(this, &MDIWindow::trackInfo)));

  audioEditMenuTree->push_item(Gnome::UISeparator());

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Cut"), N_("Cut out selected samples"),
			    GNOME_STOCK_MENU_CUT,
			    slot(audioCdChild_, &AudioCDChild::cutTrackData)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Paste"), N_("Paste previously cut samples"),
			    GNOME_STOCK_MENU_PASTE,
			    slot(audioCdChild_, &AudioCDChild::pasteTrackData)));

  audioEditMenuTree->push_item(Gnome::UISeparator());

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Add Track Mark"),
			    N_("Add track marker at current marker position"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::addTrackMark)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Add Index Mark"),
			    N_("Add index marker at current marker position"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::addIndexMark)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Add Pre-Gap"),
			    N_("Add pre-gap at current marker position"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::addPregap)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Remove Track Mark"),
			    N_("Remove selected track/index marker or pre-gap"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::removeTrackMark)));

  audioEditMenuTree->push_item(Gnome::UISeparator());

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Append Track"),
			    N_("Append track with data from audio file"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::appendTrack)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Append File"),
			    N_("Append data from audio file to last track"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::appendFile)));
  
  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Insert File"),
			    N_("Insert data from audio file at current marker position"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::insertFile)));

  audioEditMenuTree->push_item(Gnome::UISeparator());

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Append Silence"),
			    N_("Append silence to last track"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::appendSilence)));

  audioEditMenuTree->
    push_item(Gnome::UIItem(N_("Insert Silence"),
			    N_("Insert silence at current marker position"),
			    GNOME_STOCK_MENU_BLANK,
			    slot(audioCdChild_, &AudioCDChild::insertSilence)));

  StockMenus::Edit *audioEditMenu = new StockMenus::Edit(*audioEditMenuTree);

  // Actions menu
  //
  Gnome::UIInfoTree *actionsMenuTree = new Gnome::UIInfoTree();
  actionsMenuTree->push_item(Gnome::UIItem(N_("Duplicate CD"), N_(""),
  				GNOME_STOCK_MENU_BLANK,
  				slot(this, &MDIWindow::extract)));
  actionsMenuTree->push_item(Gnome::UIItem(N_("Record"), N_(""),
  				GNOME_STOCK_MENU_BLANK,
  				slot(this, &MDIWindow::record)));
  actionsMenuTree->push_item(Gnome::UIItem(N_("Fixate CD"), N_(""),
  				GNOME_STOCK_MENU_BLANK,
  				slot(this, &MDIWindow::nothing_cb)));
  actionsMenuTree->push_item(Gnome::UIItem(N_("Blank CD-RW"), N_(""),
  				GNOME_STOCK_MENU_BLANK,
  				slot(this, &MDIWindow::nothing_cb)));
  actionsMenuTree->push_item(Gnome::UIItem(N_("Get Info"), N_(""),
  				GNOME_STOCK_MENU_BLANK,
  				slot(this, &MDIWindow::nothing_cb)));

  Gnome::UIItem *actionsMenu = new Gnome::UISubtree(N_("_Actions"),
  				*actionsMenuTree);

  // Settings menu
  //
  Gnome::UIInfoTree *settingsMenuTree = new Gnome::UIInfoTree();
  settingsMenuTree->push_item(Gnome::UIItem(N_("Configure Devices..."), N_(""),
  				GNOME_STOCK_MENU_PROP,
  				slot(this, &MDIWindow::configureDevices)));
  settingsMenuTree->push_item(StockMenuItems::Preferences
  				(slot(this, &MDIWindow::nothing_cb)));

  StockMenus::Settings *settingsMenu = new StockMenus::Settings(*settingsMenuTree);


  // Help menu
  //
  Gnome::UIInfoTree *helpMenuTree = new Gnome::UIInfoTree();

  helpMenuTree->push_item(Gnome::UIHelp("Quick Start"));

  helpMenuTree->push_item(StockMenuItems::About
  				(slot(this, &MDIWindow::nothing_cb)));

  StockMenus::Help *helpMenu = new StockMenus::Help(*helpMenuTree);

  Gnome::UIInfoTree *menus = new Gnome::UIInfoTree();

  menus->push_item(*fileMenu);
  menus->push_item(*audioEditMenu);
  menus->push_item(*actionsMenu);
  menus->push_item(*settingsMenu);
  menus->push_item(*helpMenu);

//FIXME: MDI STUFF  set_menubar_template(*menus);
  create_menus(*menus);
  
  // Toolbar
  //
  Gnome::UIInfoTree *toolbarTree = new Gnome::UIInfoTree();

  toolbarTree->push_item(Gnome::UIItem(N_("New"),
                                       N_("Create a new compilation"),
                                       GNOME_STOCK_PIXMAP_NEW,
                                       slot(this, &MDIWindow::nothing_cb)));

  toolbarTree->push_item(Gnome::UISeparator());

  toolbarTree->push_item(Gnome::UIItem(N_("Quit"),
                                       N_("Quit application"),
                                       GNOME_STOCK_PIXMAP_QUIT,
                                       slot(this, &MDIWindow::app_close)));

//FIXME: MDI STUFF  set_toolbar_template(*toolbarTree);
  create_toolbar(*toolbarTree);

  install_menu_hints(*menus);
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


void
nada_cb(GtkWidget *widget, gpointer data)
{
  g_print("%s", "asdfsfdasadf");
  cout << "nothing here" << endl;
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
}


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

#include <errno.h>

#include "Project.h"
#include "util.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
#include "gcdmaster.h"
#include "MessageBox.h"
#include "AudioCDChild.h"
#include "AudioCDView.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "RecordTocDialog.h"

Project::Project() : Gnome::App("gcdmaster", APP_NAME)
{
  new_ = true;
  saveFileSelector_ = 0;  
  viewNumber = 0;
  recordTocDialog_ = 0;
  enable_layout_config(true);
//  set_usize(630, 440);
//  set_policy(false, true, false);
  set_wmclass("GCDMaster", "GCDMaster");

  createMenus();
  createToolbar();
  createStatusbar();
}

void Project::createMenus()
{
  std::vector<Gnome::UI::SubTree> menus;
  std::vector<Gnome::UI::Info> fileMenuTree, newMenuTree, editMenuTree, actionsMenuTree;
  std::vector<Gnome::UI::Info> settingsMenuTree, helpMenuTree, windowsMenuTree;

  {
    using namespace Gnome::UI;
    fileMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_NEW),
						N_("New..."),
						slot(gcdmaster, &GCDMaster::newChooserWindow),
						N_("New Project")));

    // File->New menu
    newMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_NEW),
						N_("_Audio CD"),
						bind(slot(gcdmaster, &GCDMaster::newAudioCDProject2), (ProjectChooser *)NULL),
						N_("New Audio CD")));

    newMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_NEW),
						N_("_Duplicate CD"),
						bind(slot(gcdmaster, &GCDMaster::newDuplicateCDProject), (ProjectChooser *)NULL),
						N_("Make a copy of a CD")));

    newMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_NEW),
						N_("_Copy CD to disk"),
						bind(slot(gcdmaster, &GCDMaster::newDumpCDProject), (ProjectChooser *)NULL),
						N_("Dump CD to disk")));

    // File menu
    fileMenuTree.push_back(SubTree(Icon(GNOME_STOCK_MENU_NEW),
					    N_("New"),
					    newMenuTree,
					    "Create a new project"));
  }

  guint posFileSave;
  guint posFileSaveAs;
  {
    using namespace Gnome::MenuItems;
    fileMenuTree.push_back(Open(bind(slot(gcdmaster, &GCDMaster::openProject), (ProjectChooser *)0)));
    fileMenuTree.push_back(Save(slot(this, &Project::saveProject)));
    posFileSave = fileMenuTree.size() - 1;
    fileMenuTree.push_back(SaveAs(slot(this, &Project::saveAsProject)));
    posFileSaveAs = fileMenuTree.size() - 1;

    fileMenuTree.push_back(Gnome::UI::Separator());

//    fileMenuTree.push_back(PrintSetup(slot(this, &Project::nothing_cb)));
//
//    fileMenuTree.push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_PRINT),
//								 N_("Print Cover..."),
//								 slot(this, &Project::nothing_cb),
//								 N_("Print Cover")));
//
//    fileMenuTree.push_back(Gnome::UI::Separator());

    // Close the current child (project);
    fileMenuTree.push_back(Close(bind(slot(gcdmaster, &GCDMaster::closeProject), this)));
    fileMenuTree.push_back(Exit(bind(slot(gcdmaster, &GCDMaster::appClose), this)));
  }

  guint posActionsRecord;
  {
    using namespace Gnome::UI;
    // Edit menu
    editMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_PROP),
    				 N_("Project Info..."),
			      slot(this, &Project::projectInfo),
			      N_("Edit global project data")));

    // Actions menu
    actionsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_CDROM),
								N_("_Record"),
								slot(this, &Project::recordToc2CD),
								N_("Record")));
    posActionsRecord = actionsMenuTree.size() - 1;

    actionsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_CDROM),
								N_("Blank CD-RW"),
								slot(gcdmaster, &GCDMaster::blankCDRW),
								N_("Erase a CD-RW")));

//    actionsMenuTree.push_back(Gnome::UI::Item(N_("Fixate CD"),
//					    slot(this, &Project::nothing_cb)));
//    actionsMenuTree.push_back(Gnome::UI::Item(N_("Get Info"),
//					    slot(this, &Project::nothing_cb)));

    // Settings menu
    settingsMenuTree.push_back(Item(Icon(GNOME_STOCK_MENU_PREF),
								N_("Configure Devices..."),
								slot(gcdmaster, &GCDMaster::configureDevices)));
  }

//    settingsMenuTree.push_back(Gnome::MenuItems::Preferences
//  				(slot(this, &Project::nothing_cb)));


  // Help menu
  //helpMenuTree.push_back(Gnome::UI::Help("Quick Start"));

  helpMenuTree.push_back(Gnome::MenuItems::About
  				(slot(gcdmaster, &GCDMaster::aboutDialog)));

  {
    using namespace Gnome::Menus;
    menus.push_back(File(fileMenuTree));
    menus.push_back(Gnome::Menus::Edit(editMenuTree));
    menus.push_back(Gnome::UI::Menu(N_("_Actions"), actionsMenuTree));
    menus.push_back(Settings(settingsMenuTree));
//    menus.push_back(Windows(windowsMenuTree));
    menus.push_back(Help(helpMenuTree));
  }

  Gnome::UI::Array<Gnome::UI::SubTree>& arrayInfo = create_menus(menus);
  Gnome::UI::SubTree& subtreeFile = arrayInfo[0];
  Gnome::UI::SubTree& subtreeEdit = arrayInfo[1];
  Gnome::UI::SubTree& subtreeAction = arrayInfo[2];
  Gnome::UI::Array<Gnome::UI::Info>& arrayInfoFile = subtreeFile.get_uitree();
  Gnome::UI::Array<Gnome::UI::Info>& arrayInfoEdit = subtreeEdit.get_uitree();
  Gnome::UI::Array<Gnome::UI::Info>& arrayInfoAction = subtreeAction.get_uitree();
  
  // Get widget of created menuitems
  miSave_ = arrayInfoFile[posFileSave].get_widget();
  miSaveAs_ = arrayInfoFile[posFileSaveAs].get_widget();
  miEditTree_ = subtreeEdit.get_widget();
  miRecord_ = arrayInfoAction[posActionsRecord].get_widget();
}

void Project::createToolbar()
{
  toolbar = new Gtk::Toolbar;
  Gtk::AccelGroup *accel_group = Gtk::AccelGroup::create();
  std::vector<Gnome::UI::Info> toolbarTree;

  {
    using namespace Gnome::UI;
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_NEW),
								N_("New"),
								slot(gcdmaster, &GCDMaster::newChooserWindow),
								N_("New Project")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_OPEN),
								N_("Open"),
								bind(slot(gcdmaster, &GCDMaster::openProject), (ProjectChooser *)0),
								N_("Open a project")));
    toolbarTree.push_back(Item(Icon(GNOME_STOCK_PIXMAP_CLOSE),
								N_("Close"),
								bind(slot(gcdmaster, &GCDMaster::closeProject), this),
								N_("Close current project")));
  }

  fill(*toolbar, toolbarTree, *accel_group);

  toolbar->show();
  toolbar->set_border_width(2);

  Gnome::StockPixmap *pixmap =
  	manage(new Gnome::StockPixmap(GNOME_STOCK_PIXMAP_SAVE));
  toolbar->tools().push_back(Gtk::Toolbar_Helpers::ButtonElem(N_("Save"), *pixmap,
  		slot(this, &Project::saveProject), N_("Save current project"), ""));
  tiSave_ = toolbar->tools().back()->get_widget();

  pixmap = manage(new Gnome::StockPixmap(GNOME_STOCK_PIXMAP_CDROM));
  toolbar->tools().push_back(Gtk::Toolbar_Helpers::ButtonElem(N_("Record"), *pixmap,
  		slot(this, &Project::recordToc2CD), N_("Record to CD"), ""));
  tiRecord_ = toolbar->tools().back()->get_widget();

  add_docked(*toolbar, "main_toolbar", GNOME_DOCK_ITEM_BEH_NORMAL, GNOME_DOCK_TOP, 1, 1, 0);
}

void Project::createStatusbar()
{
  Gtk::HBox *container = new Gtk::HBox;
//  statusbar_ = new Gtk::Statusbar;
  statusbar_ = new Gnome::AppBar(FALSE, TRUE, GNOME_PREFERENCES_NEVER);
  progressbar_ = new Gtk::ProgressBar;
  progressButton_ = new Gtk::Button("Cancel");
  progressButton_->set_sensitive(false);

  progressbar_->set_usize(150, 0);
  container->pack_start(*statusbar_, TRUE, TRUE); 
  container->pack_start(*progressbar_, FALSE, FALSE); 
  container->pack_start(*progressButton_, FALSE, FALSE); 
  set_statusbar_custom(*container, *statusbar_);
  container->set_spacing(2);
  container->set_border_width(2);
  container->show_all();
}

gint Project::delete_event_impl(GdkEventAny* e)
{
  gcdmaster->closeProject(this);
  return true;  // Do not close window, we will delete it if necessary
}

void Project::updateWindowTitle()
{
  std::string s(tocEdit_->filename());
  s += " - ";
  s += APP_NAME;
  if (tocEdit_->tocDirty())
    s += "(*)";
  set_title(s);
}

void Project::saveProject()
{
  if (new_)
  {
    saveAsProject();
    return;
  }
  if (tocEdit_->saveToc() == 0)
  {
    statusMessage("Project saved to ", tocEdit_->filename());
//FIXME    guiUpdate();
  }
  else {
    std::string s("Cannot save toc to \"");
    s += tocEdit_->filename();
    s+= "\":";
    
    MessageBox msg(this, "Save Project", 0, s.c_str(), strerror(errno), NULL);
    msg.run();
  }
}

void Project::saveAsProject()
{
  if (saveFileSelector_)
  {
    Gdk_Window selector_win = saveFileSelector_->get_window();
    selector_win.show();
    selector_win.raise();
  }
  else
  {
    saveFileSelector_ = new Gtk::FileSelection("Save Project");
    saveFileSelector_->get_ok_button()->clicked.connect(
				slot(this, &Project::saveFileSelectorOKCB));
    saveFileSelector_->get_cancel_button()->clicked.connect(
				slot(this, &Project::saveFileSelectorCancelCB));
    saveFileSelector_->set_transient_for(*this);
  }
  saveFileSelector_->show();
}

void Project::saveFileSelectorCancelCB()
{
  saveFileSelector_->hide();
  saveFileSelector_->destroy();
  saveFileSelector_ = 0;
}

void Project::saveFileSelectorOKCB()
{
  char *s = g_strdup(saveFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {
    if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
      statusMessage("Project saved to \"%s\".", tocEdit_->filename());
//FIXME  	guiUpdate();

      new_ = false; // The project is now saved
      //cout << tocEdit_->filename() << endl;
      updateWindowTitle();
      saveFileSelectorCancelCB();
    }
    else {
  	std::string m("Cannot save toc to \"");
  	m += tocEdit_->filename();
  	m += "\":";
    
  	MessageBox msg(saveFileSelector_, "Save Project", 0, m.c_str(), strerror(errno), NULL);
  	msg.run();
    }
    g_free(s);
  }
}

gint Project::getViewNumber()
{
  return(viewNumber++);
}

void Project::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  char *s = g_strdup_vprintf(fmt, args);

  flash(s);

  free(s);

  va_end(args);
}

int Project::projectNumber()
{
  return projectNumber_;
}

TocEdit *Project::tocEdit()
{
  return tocEdit_;
}

void Project::tocBlockedMsg(const char *op)
{
  MessageBox msg(this, op, 0,
		 "Cannot perform requested operation because", 
		 "project is in read-only state.", NULL);
  msg.run();
}


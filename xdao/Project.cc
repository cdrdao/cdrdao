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
#include <gtkmm.h>
#include <gnome.h>

#include "config.h"

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

Project::Project() : Gnome::UI::App("gcdmaster", APP_NAME)
{
  new_ = true;
  saveFileSelector_ = 0;  
  viewNumber = 0;
  about_ = NULL;
  recordTocDialog_ = 0;
  enable_layout_config(true);

  set_resizable();
  set_wmclass("gcdmaster", "GCDMaster");
  frame_.set_shadow_type(Gtk::SHADOW_IN);
  set_contents(frame_);

  createMenus();
  createStatusbar();
}

void Project::createMenus()
{
  std::vector<Gnome::UI::Items::SubTree> menus;
  std::vector<Gnome::UI::Items::Info> fileMenuTree, newMenuTree;
  std::vector<Gnome::UI::Items::Info> editMenuTree, actionsMenuTree;
  std::vector<Gnome::UI::Items::Info> settingsMenuTree, helpMenuTree;
  std::vector<Gnome::UI::Items::Info> windowsMenuTree;

  {
    using namespace Gnome::UI::Items;
    using namespace Gnome::UI::MenuItems;
    fileMenuTree.push_back(New(_("New..."), _("Create a new project"),
                               slot(*gcdmaster,
                                    &GCDMaster::newChooserWindow)));

    // File->New menu
    newMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::NEW)),
                               _("_Audio CD"),
                               bind(slot(*gcdmaster,
                                         &GCDMaster::newAudioCDProject2),
                                    (ProjectChooser *)NULL),
                               _("New Audio CD")));

    newMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::NEW)),
                               _("_Duplicate CD"),
                               bind(slot(*gcdmaster,
                                         &GCDMaster::newDuplicateCDProject),
                                    (ProjectChooser *)NULL),
                               _("Make a copy of a CD")));

    newMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::NEW)),
                               _("_Copy CD to disk"),
                               bind(slot(*gcdmaster,
                                         &GCDMaster::newDumpCDProject),
                                    (ProjectChooser *)NULL),
                               _("Dump CD to disk")));

    // File menu
    fileMenuTree.push_back(SubTree(Icon(Gtk::StockID(Gtk::Stock::NEW)),
                                   _("New"),
                                   newMenuTree,
                                   _("Create a new project")));
  }

  guint posFileSave;
  guint posFileSaveAs;
  {
    using namespace Gnome::UI::MenuItems;
    fileMenuTree.push_back(Open(bind(slot(*gcdmaster,
                                          &GCDMaster::openProject),
                                     (ProjectChooser *)0)));
    fileMenuTree.push_back(Save(slot(*this, &Project::saveProject)));
    posFileSave = fileMenuTree.size() - 1;
    fileMenuTree.push_back(SaveAs(slot(*this, &Project::saveAsProject)));
    posFileSaveAs = fileMenuTree.size() - 1;

    fileMenuTree.push_back(Gnome::UI::Items::Separator());

//    fileMenuTree.push_back(PrintSetup(slot(*this, &Project::nothing_cb)));
//
//    fileMenuTree.push_back(Gnome::UI::Item(Gnome::UI::Icon(GNOME_STOCK_MENU_PRINT),
//								 "Print Cover...",
//								 slot(*this, &Project::nothing_cb),
//								 "Print Cover"));
//
//    fileMenuTree.push_back(Gnome::UI::Items::Separator());

    // Close the current child (project);
    fileMenuTree.push_back(Close(bind(slot(*gcdmaster, &GCDMaster::closeProject), this)));
    fileMenuTree.push_back(Exit(bind(slot(*gcdmaster, &GCDMaster::appClose), this)));
  }

  guint posActionsRecord;
  {
      using namespace Gnome::UI::Items;
    // Edit menu
    editMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::PROPERTIES)),
                                _("Project Info..."),

                                slot(*this, &Project::projectInfo),
                                _("Edit global project data")));

    // Actions menu
    actionsMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::CDROM)),
                                   _("_Record"),
                                   slot(*this, &Project::recordToc2CD),
                                   _("Record")));
    posActionsRecord = actionsMenuTree.size() - 1;

    actionsMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::CDROM)),
                                   _("Blank CD-RW"),
                                   bind(slot(*gcdmaster,
                                             &GCDMaster::blankCDRW),
                                        this),
                                   _("Erase a CD-RW")));

//    actionsMenuTree.push_back(Gnome::UI::Item("Fixate CD",
//					    slot(*this, &Project::nothing_cb)));
//    actionsMenuTree.push_back(Gnome::UI::Item("Get Info",
//					    slot(*this, &Project::nothing_cb)));

    // Settings menu
    settingsMenuTree.push_back(Item(Icon(Gtk::StockID(Gtk::Stock::PREFERENCES)),
                                    _("Configure Devices..."),
                                    slot(*gcdmaster, &GCDMaster::configureDevices)));
  }

//    settingsMenuTree.push_back(Gnome::UI::MenuItems::Preferences
//  				(slot(*this, &Project::nothing_cb)));


  // Help menu
  //helpMenuTree.push_back(Gnome::UI::Help("Quick Start"));

  helpMenuTree.push_back(Gnome::UI::MenuItems::About
  				(slot(*this, &Project::aboutDialog)));

  {
    using namespace Gnome::UI::Menus;
    menus.push_back(File(fileMenuTree));
    menus.push_back(Gnome::UI::Menus::Edit(editMenuTree));
    menus.push_back(Gnome::UI::Items::Menu(_("_Actions"), actionsMenuTree));
    menus.push_back(Settings(settingsMenuTree));
//    menus.push_back(Windows(windowsMenuTree));
    menus.push_back(Help(helpMenuTree));
  }

  Gnome::UI::Items::Array<Gnome::UI::Items::SubTree>& arrayInfo =
      create_menus(menus);
  Gnome::UI::Items::SubTree& subtreeFile = arrayInfo[0];
  Gnome::UI::Items::SubTree& subtreeEdit = arrayInfo[1];
  Gnome::UI::Items::SubTree& subtreeAction = arrayInfo[2];
  Gnome::UI::Items::Array<Gnome::UI::Items::Info>& arrayInfoFile =
      subtreeFile.get_uitree();
  Gnome::UI::Items::Array<Gnome::UI::Items::Info>& arrayInfoEdit =
      subtreeEdit.get_uitree();
  Gnome::UI::Items::Array<Gnome::UI::Items::Info>& arrayInfoAction =
      subtreeAction.get_uitree();
  
  // Get widget of created menuitems
  miSave_ = arrayInfoFile[posFileSave].get_widget();
  miSaveAs_ = arrayInfoFile[posFileSaveAs].get_widget();
  miEditTree_ = subtreeEdit.get_widget();
  miRecord_ = arrayInfoAction[posActionsRecord].get_widget();
}

void Project::createStatusbar()
{
  Gtk::HBox *container = new Gtk::HBox;
  statusbar_ = new Gnome::UI::AppBar(false, true,
                                     Gnome::UI::PREFERENCES_NEVER);
  progressbar_ = new Gtk::ProgressBar;
  progressButton_ = new Gtk::Button(_("Cancel"));
  progressButton_->set_sensitive(false);

  progressbar_->set_size_request(150, -1);
  container->pack_start(*statusbar_, true, true); 
  container->pack_start(*progressbar_, false, false); 
  container->pack_start(*progressButton_, false, false); 
  set_statusbar_custom(*container, *statusbar_);
  container->set_spacing(2);
  container->set_border_width(2);
  container->show_all();
}

bool Project::on_delete_event(GdkEventAny* e)
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
  if (new_) {
    saveAsProject();
    return;
  }

  if (tocEdit_->saveToc() == 0) {
    statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename());
    guiUpdate(UPD_TOC_DIRTY);

  } else {
    std::string s(_("Cannot save toc to \""));
    s += tocEdit_->filename();
    s+= "\":";
    
    MessageBox msg(this, _("Save Project"), 0, s.c_str(), strerror(errno),
                   NULL);
    msg.run();
  }
}

void Project::saveAsProject()
{
  if (!saveFileSelector_) {
    saveFileSelector_ = new Gtk::FileSelection(_("Save Project"));
    saveFileSelector_->get_ok_button()->signal_clicked().
      connect(slot(*this, &Project::saveFileSelectorOKCB));
    saveFileSelector_->get_cancel_button()->signal_clicked().
      connect(slot(*this, &Project::saveFileSelectorCancelCB));
    saveFileSelector_->set_transient_for(*this);
  }

  saveFileSelector_->present();
}

void Project::saveFileSelectorCancelCB()
{
  saveFileSelector_->hide();
}

void Project::saveFileSelectorOKCB()
{
  char *s = g_strdup(saveFileSelector_->get_filename().c_str());

  if (s != NULL && *s != 0 && s[strlen(s) - 1] != '/') {

    if (tocEdit_->saveAsToc(stripCwd(s)) == 0) {
      statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename());
      new_ = false; // The project is now saved
      updateWindowTitle();
      saveFileSelectorCancelCB();
      guiUpdate(UPD_TOC_DIRTY);

    } else {

      std::string m(_("Cannot save toc to \""));
      m += tocEdit_->filename();
      m += "\":";
      MessageBox msg(saveFileSelector_, _("Save Project"), 0, m.c_str(),
                     strerror(errno), NULL);
      msg.run();
    }
  }

  if (s) g_free(s);
}

gint Project::getViewNumber()
{
  return viewNumber++;
}

void Project::statusMessage(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  char *s = g_strdup_vprintf(fmt, args);

  statusbar_->push(s);

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
		 _("Cannot perform requested operation because " 
                   "project is in read-only state."), NULL);
  msg.run();
}

void Project::aboutDialog()
{
  if (about_) {
      // "About" dialog hasn't been closed, so just raise it
      about_->present();

  } else {

    std::vector<std::string> authors;
    authors.push_back("Andreas Mueller <mueller@daneb.ping.de>");
    authors.push_back("Manuel Clos <llanero@jazzfree.com>");
    authors.push_back("Denis Leroy <denis@poolshark.org>");
    std::vector<std::string> comments;

    about_ = new Gnome::UI::About("gcdmaster", VERSION,
                                  "(C) Andreas Mueller",
                                  authors, comments);

    about_->set_transient_for(*this);
    about_->show();
  }
}

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
#include "guiUpdate.h"
#include "DeviceConfDialog.h"
#include "PreferencesDialog.h"
#include "ProjectChooser.h"
#include "gcdmaster.h"
#include "TocEdit.h"
#include "util.h"
#include "AudioCDProject.h"
#include "DuplicateCDProject.h"
#include "BlankCDDialog.h"
#include "DumpCDProject.h"
#include "Icons.h"

// Static class members
std::list<GCDMaster *> GCDMaster::apps;

GCDMaster::GCDMaster() : Gnome::UI::App("gcdmaster", APP_NAME)
{
  set_title(APP_NAME);

  project_number = 0;
  about_ = 0;
  project_ = 0;
  chooser_ = 0;

  set_resizable();
  set_wmclass("gcdmaster", "GCDMaster");

  readFileSelector_ =
    new Gtk::FileChooserDialog(_("Please select a project"),
                               Gtk::FILE_CHOOSER_ACTION_OPEN);
  readFileSelector_->set_transient_for(*this);
  readFileSelector_->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  readFileSelector_->add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

  Gtk::FileFilter* filter_tocs = new Gtk::FileFilter;
  manage(filter_tocs);
#ifdef HAVE_MP3_SUPPORT
  filter_tocs->set_name("Content Files (*.toc, *.cue, *.m3u)");
#else
  filter_tocs->set_name("Content Files (*.toc, *.cue)");
#endif
  filter_tocs->add_pattern("*.toc");
  filter_tocs->add_pattern("*.cue");
#ifdef HAVE_MP3_SUPPORT
  filter_tocs->add_pattern("*.m3u");
#endif
  readFileSelector_->add_filter(*filter_tocs);

  Gtk::FileFilter* filter_all = new Gtk::FileFilter;
  manage(filter_all);
  filter_all->set_name("Any files");
  filter_all->add_pattern("*");
  readFileSelector_->add_filter(*filter_all);

  Icons::registerStockIcons();
  notebook_.set_show_border(false);
  set_contents(notebook_);

  createMenus();
  createStatusbar();

  apps.push_back(this);
}

void GCDMaster::createMenus()
{
  //Define the actions:
  m_refActionGroup = Gtk::ActionGroup::create("Actions");

  // File
  m_refActionGroup->add( Gtk::Action::create("FileMenu", _("_File")) );
  m_refActionGroup->add( Gtk::Action::create("New", Gtk::Stock::NEW),
                         sigc::mem_fun(*this, &GCDMaster::newChooserWindow) );

  // File->New
  m_refActionGroup->add( Gtk::Action::create("FileNewMenu", _("N_ew")) );
  m_refActionGroup->add( Gtk::Action::create("NewAudioCD", Gtk::Stock::NEW,
                         _("_Audio CD"),
                         _("New Audio CD")),
                         sigc::mem_fun(*this, &GCDMaster::newAudioCDProject2) );

  m_refActionGroup->add( Gtk::Action::create("NewDuplicateCD", Gtk::Stock::NEW,
                         _("_Duplicate CD"),
                         _("Make a copy of a CD")),
                         sigc::mem_fun(*this, &GCDMaster::newDuplicateCDProject) );

  m_refActionGroup->add( Gtk::Action::create("NewDumpCD", Gtk::Stock::NEW,
                         _("_Copy CD to disk"),
                         _("Dump CD to disk")),
                         sigc::mem_fun(*this, &GCDMaster::newDumpCDProject) );

  m_refActionGroup->add( Gtk::Action::create("Open", Gtk::Stock::OPEN),
                         sigc::mem_fun(*this, &GCDMaster::openProject) );

  m_refActionGroup->add( Gtk::Action::create("Close", Gtk::Stock::CLOSE),
                         sigc::hide_return(sigc::mem_fun(*this, &GCDMaster::closeProject)));

  m_refActionGroup->add( Gtk::Action::create("Quit", Gtk::Stock::QUIT),
                         &GCDMaster::appClose);

  // Edit
  m_refActionGroup->add( Gtk::Action::create("EditMenu", _("_Edit")) );


  // Actions menu
  m_refActionGroup->add( Gtk::Action::create("ActionsMenu", _("_Actions")) );
  m_refActionGroup->add( Gtk::Action::create("BlankCD", Gtk::Stock::CDROM,
                         _("Blank CD-RW"),
                         _("Erase a CD-RW")),
                         sigc::mem_fun(*this, &GCDMaster::blankCDRW) );

  // Settings
  m_refActionGroup->add( Gtk::Action::create("SettingsMenu", _("_Settings")) );
  m_refActionGroup->add( Gtk::Action::create("ConfigureDevices", Gtk::Stock::PREFERENCES,
                         _("Configure Devices..."),
                         _("Configure the read and recording devices")),
                         sigc::mem_fun(*this, &GCDMaster::configureDevices) );
  m_refActionGroup->add( Gtk::Action::create("Preferences", Gtk::Stock::PREFERENCES,
					     _("_Preferences..."),
					     _("Set various preferences and parameters")),
			 sigc::mem_fun(*this, &GCDMaster::configurePreferences));

  // Help
  m_refActionGroup->add( Gtk::Action::create("HelpMenu", _("_Help")) );
//FIXME: llanero Gtk::Stock::ABOUT ???
  m_refActionGroup->add( Gtk::Action::create("About", _("About")),
                         sigc::mem_fun(*this, &GCDMaster::aboutDialog) );

  m_refUIManager = Gtk::UIManager::create();
  m_refUIManager->insert_action_group(m_refActionGroup);
  add_accel_group(m_refUIManager->get_accel_group());

  //Layout the actions in a menubar and toolbar:
  try
  {
    Glib::ustring ui_info =
        "<ui>"
        "  <menubar name='MenuBar'>"
        "    <menu action='FileMenu'>"
        "      <menuitem action='New'/>"
        "      <menu action='FileNewMenu'>"
        "        <menuitem action='NewAudioCD'/>"
        "        <menuitem action='NewDuplicateCD'/>"
        "        <menuitem action='NewDumpCD'/>"
        "      </menu>"
        "      <menuitem action='Open'/>"
        "      <placeholder name='FileSaveHolder'/>"
        "      <separator/>"
        "      <menuitem action='Close'/>"
        "      <menuitem action='Quit'/>"
        "    </menu>"
        "    <menu action='EditMenu'/>"
        "    <menu action='ActionsMenu'>"
        "      <placeholder name='ActionsRecordHolder'/>"
        "      <menuitem action='BlankCD'/>"
        "      <separator/>"
        "    </menu>"
        "    <menu action='SettingsMenu'>"
        "      <menuitem action='ConfigureDevices'/>"
	"      <separator/>"
        "      <menuitem action='Preferences'/>"
        "    </menu>"
        "    <menu action='HelpMenu'>"
        "      <menuitem action='About'/>"
        "    </menu>"
        "  </menubar>"
        "  <toolbar name='ToolBar'>"
        "    <toolitem action='New'/>"
        "    <toolitem action='Open'/>"
        "  </toolbar>"
        "</ui>";

    m_refUIManager->add_ui_from_string(ui_info);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << "building menus failed: " <<  ex.what() << "\n";
    exit(1);
  }

  Gtk::Widget* pMenuBar = m_refUIManager->get_widget("/MenuBar");
  set_menus(dynamic_cast<Gtk::MenuBar&>(*pMenuBar));
  Gtk::Widget* pToolbar = m_refUIManager->get_widget("/ToolBar");
  set_toolbar(dynamic_cast<Gtk::Toolbar&>(*pToolbar));
}

bool GCDMaster::openNewProject(const char* s)
{
  TocEdit* tocEdit;

  if (s == NULL || *s == 0 || s[strlen(s) - 1] == '/')
    return false;

  FileExtension type = fileExtension(s);
  switch (type) {

  case FE_M3U:
    newAudioCDProject("", NULL, s);
    break;

  case FE_TOC:
    tocEdit = new TocEdit(NULL, NULL);
    if (tocEdit->readToc(stripCwd(s)) == 0)
      newAudioCDProject(stripCwd(s), tocEdit);
    else
      return false;
    break;

  case FE_CUE:
    tocEdit = new TocEdit(NULL, NULL);
    if (tocEdit->readToc(stripCwd(s)) == 0)
      newAudioCDProject("", tocEdit);
    else
      return false;
    break;

  default:
    printf("Could not open \"%s\": format not supported.\n", s);
    return false;
    break;
  }

  return true;
}

void GCDMaster::openProject()
{
  readFileSelector_->present();
  int result = readFileSelector_->run();
  readFileSelector_->hide();

  if (result == Gtk::RESPONSE_OK) {
    std::string s = readFileSelector_->get_filename();
    openNewProject(s.c_str());
  }
}

bool GCDMaster::closeProject()
{
  if (chooser_)
    closeChooser();

  if (project_) {
    if (project_->closeProject()) {
      delete project_;
      project_ = NULL;
    } else {
      return false; // User clicked on cancel
    }
  }

  if (readFileSelector_)
    delete readFileSelector_;

  apps.remove(this);
  delete this;

  if (apps.size() == 0)
    Gnome::Main::quit(); // Quit if there are not remaining windows

  return true;
}

void GCDMaster::closeChooser()
{
  delete chooser_;
  chooser_ = NULL;
}

bool GCDMaster::on_delete_event(GdkEventAny* e)
{
  closeProject();
  return true;
}

// Application Close. Called when the user selects 'Quit' from the
// menu. Try to close all project windows and quit.

void GCDMaster::appClose()
{
  // Can't just iterate, as closeProject will remove its object from
  // the list.
  while (apps.size() > 0) {
    
    if (!(*(GCDMaster::apps.begin()))->closeProject())
      return;
  }

  return;
}

void GCDMaster::newChooserWindow()
{
  if (project_ || chooser_) {

    GCDMaster *gcdmaster = new GCDMaster;
    gcdmaster->newChooserWindow();
    gcdmaster->show();

  } else {

    chooser_ = new ProjectChooser();
    chooser_->newAudioCDProject.connect(sigc::mem_fun(*this,
                                        &GCDMaster::newAudioCDProject2));
    chooser_->newDuplicateCDProject.connect(sigc::mem_fun(*this,
                                        &GCDMaster::newDuplicateCDProject));
    chooser_->newDumpCDProject.connect(sigc::mem_fun(*this,
                                        &GCDMaster::newDumpCDProject));
    chooser_->show();
    notebook_.set_show_tabs(false);
    notebook_.append_page(*chooser_);
  }
}

void GCDMaster::newAudioCDProject(const char *name, TocEdit *tocEdit,
                                  const char* tracks)
{
  if (!project_) {

    AudioCDProject* p = new AudioCDProject(project_number++, name, tocEdit,
                                           this);
    p->add_menus(m_refUIManager);
    p->configureAppBar(statusbar_, progressbar_, progressButton_);

    project_ = p;
    project_->show();
    if (chooser_)
      closeChooser();
    notebook_.remove_page();
    notebook_.set_show_tabs(false);
    notebook_.append_page(*project_);
    if (tracks)
      p->appendTrack(tracks);

  } else {

    GCDMaster *gcdmaster = new GCDMaster;
    gcdmaster->newAudioCDProject(name, tocEdit, tracks);
    gcdmaster->show();
  }
}

void GCDMaster::newAudioCDProject2()
{
  newAudioCDProject("", NULL);
}

void GCDMaster::newDuplicateCDProject()
{
  if (!project_) {

    project_ = new DuplicateCDProject(this);
    project_->show();
    if (chooser_)
      closeChooser();
    notebook_.remove_page();
    notebook_.set_show_tabs(false);
    notebook_.append_page(*project_);
    set_title(_("Duplicate CD"));

  } else {

    GCDMaster *gcdmaster = new GCDMaster;
    gcdmaster->newDuplicateCDProject();
    gcdmaster->show();
  }
}

void GCDMaster::newDumpCDProject()
{
  if (!project_) {

    project_ = new DumpCDProject(this);
    project_->show();
    if (chooser_)
      closeChooser();
    notebook_.remove_page();
    notebook_.set_show_tabs(false);
    notebook_.append_page(*project_);
    set_title(_("Dump CD to disk"));

  } else {

    GCDMaster *gcdmaster = new GCDMaster;
    gcdmaster->newDumpCDProject();
    gcdmaster->show();
  }
}

void GCDMaster::update(unsigned long level)
{
  if (project_)
    project_->update(level);

  blankCDDialog_.update(level);
}

void GCDMaster::configureDevices()
{
  deviceConfDialog->start();
}

void GCDMaster::configurePreferences()
{
    preferencesDialog->show_all();
}

void GCDMaster::blankCDRW()
{
  blankCDDialog_.start(*this);
}

void GCDMaster::createStatusbar()
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
  install_menu_hints();
}

void GCDMaster::aboutDialog()
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

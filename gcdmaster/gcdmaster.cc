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

#include "config.h"

#include "gcdmaster.h"
#include "xcdrdao.h"
#include "guiUpdate.h"
//#include "DeviceConfDialog.h"
#include "ProjectChooser.h"
#include "TocEdit.h"
#include "util.h"
#include "AudioCDProject.h"
#include "DuplicateCDProject.h"
#include "BlankCDWindow.h"
#include "DumpCDProject.h"

GCDWindow* GCDWindow::create(Glib::RefPtr<Gtk::Builder> builder, GCDWindow::What what,
                             const char* proj_name = NULL, TocEdit* toc = NULL)
{
    GCDWindow* window = NULL;
    Project* project = NULL;

    builder->add_from_resource("/org/gnome/gcdmaster/window.ui");
    builder->get_widget_derived("app_window", window);
    if (!window)
        throw std::runtime_error("No \"app_window\" object");

    switch (what) {
    case What::CHOOSER:
        project = ProjectChooser::create(builder, window);
        window->gears_->hide();
        break;
    case What::DUPLICATE:
        project = DuplicateCDProject::create(builder, window);
        break;
    case What::DUMP:
        project = DumpCDProject::create(builder, window);
        break;
    case What::AUDIOCD:
        project = AudioCDProject::create(builder, 0, proj_name, toc, window);
        break;
    default:
        throw std::runtime_error("create arg");
    }

    window->set_project(project);

    return window;
}

GCDWindow::GCDWindow(BaseObjectType* cobject,
                     const Glib::RefPtr<Gtk::Builder>& builder) :
    Gtk::ApplicationWindow(cobject),
    notebook_(NULL)
{
    builder->get_widget("notebook", notebook_);
    builder->get_widget("gears", gears_);

    if (!notebook_ || !gears_)
        throw std::runtime_error("Missing resource");

    notebook_->set_show_border(false);
    notebook_->set_show_tabs(false);

    builder->add_from_resource("/org/gnome/gcdmaster/gears_menu.ui");
    auto object = builder->get_object("gears-menu");
    auto menu = Glib::RefPtr<Gio::MenuModel>::cast_dynamic(object);
    if (!menu)
        throw std::runtime_error("No \"gears_menu\" in gears_menu.ui");

    gears_->set_menu_model(menu);

    auto gtk_settings = Gtk::Settings::get_default();
    if (gtk_settings)
        gtk_settings->property_gtk_shell_shows_app_menu() = false;
    set_show_menubar(true);

    set_icon(Gdk::Pixbuf::create_from_resource("/org/gnome/gcdmaster/gcdmaster.png"));
}

void GCDWindow::set_project(Project* project)
{
    project_ = project;
    while (notebook_->get_n_pages() > 0)
        notebook_->remove_page();
    notebook_->set_show_tabs(false);
    notebook_->append_page(*project);
}

void GCDWindow::update(unsigned long level)
{
    project_->update(level);
}

GCDMaster::GCDMaster() : Gtk::Application("org.gnome.gcdmaster"),
                         aboutDialog_(), blankCDWindow_()
{
    builder_ = Gtk::Builder::create();

    project_number_ = 0;
//  project_ = 0;

//   readFileSelector_ =
//     new Gtk::FileChooserDialog("Please select a project",
//                                Gtk::FILE_CHOOSER_ACTION_OPEN);
//   readFileSelector_->set_transient_for(*this);
//   readFileSelector_->add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
//   readFileSelector_->add_button(Gtk::Stock::OPEN, Gtk::RESPONSE_OK);

//   Gtk::FileFilter* filter_tocs = new Gtk::FileFilter;
//   manage(filter_tocs);
// #ifdef HAVE_MP3_SUPPORT
//   filter_tocs->set_name("Content Files (*.toc, *.cue, *.m3u)");
// #else
//   filter_tocs->set_name("Content Files (*.toc, *.cue)");
// #endif
//   filter_tocs->add_pattern("*.toc");
//   filter_tocs->add_pattern("*.cue");
// #ifdef HAVE_MP3_SUPPORT
//   filter_tocs->add_pattern("*.m3u");
// #endif
//   readFileSelector_->add_filter(*filter_tocs);

//   Gtk::FileFilter* filter_all = new Gtk::FileFilter;
//   manage(filter_all);
//   filter_all->set_name("Any files");
//   filter_all->add_pattern("*");
//   readFileSelector_->add_filter(*filter_all);

//    Icons::registerStockIcons();
//  notebook_.set_show_border(false);
//  set_contents(notebook_);

//  createMenus();
//  createStatusbar();

}

// void GCDMaster::createMenus()
// {
//   //Define the actions:
//   m_refActionGroup = Gtk::ActionGroup::create("Actions");

//   // File
//   m_refActionGroup->add( Gtk::Action::create("FileMenu", "_File") );
//   m_refActionGroup->add( Gtk::Action::create("New", Gtk::Stock::NEW),
//                          sigc::mem_fun(*this, &GCDMaster::newChooserWindow) );

//   // File->New
//   m_refActionGroup->add( Gtk::Action::create("FileNewMenu", "N_ew") );
// //  m_refActionGroup->add( Gtk::Action::create("NewAudioCD", Gtk::Stock::NEW,
// //                         _("_Audio CD"),
// //                         _("New Audio CD")),
// //                         sigc::mem_fun(*this, &GCDMaster::newAudioCDProject2) );

//   m_refActionGroup->add( Gtk::Action::create("NewDuplicateCD", Gtk::Stock::NEW,
//                          "_Duplicate CD",
//                          "Make a copy of a CD"),
//                          sigc::mem_fun(*this, &GCDMaster::newDuplicateCDProject) );

//   m_refActionGroup->add( Gtk::Action::create("NewDumpCD", Gtk::Stock::NEW,
//                          "_Copy CD to disk",
//                          "Dump CD to disk"),
//                          sigc::mem_fun(*this, &GCDMaster::newDumpCDProject) );

//   // m_refActionGroup->add( Gtk::Action::create("Open", Gtk::Stock::OPEN),
//   //                        sigc::mem_fun(*this, &GCDMaster::openProject) );

//   // m_refActionGroup->add( Gtk::Action::create("Close", Gtk::Stock::CLOSE),
//   //                        sigc::hide_return(sigc::mem_fun(*this, &GCDMaster::closeProject)));

//   m_refActionGroup->add( Gtk::Action::create("Quit", Gtk::Stock::QUIT),
//                          &GCDMaster::appClose);

//   // Edit
//   m_refActionGroup->add( Gtk::Action::create("EditMenu", "_Edit") );


//   // Actions menu
//   m_refActionGroup->add( Gtk::Action::create("ActionsMenu", "_Actions") );
//   m_refActionGroup->add( Gtk::Action::create("BlankCD", Gtk::Stock::CDROM,
//                          "Blank CD-RW",
//                          "Erase a CD-RW"),
//                          sigc::mem_fun(*this, &GCDMaster::blankCDRW) );

//   // Help
//   m_refActionGroup->add( Gtk::Action::create("HelpMenu", "_Help") );

//   m_refUIManager = Gtk::UIManager::create();
//   m_refUIManager->insert_action_group(m_refActionGroup);
//   add_accel_group(m_refUIManager->get_accel_group());

//   //Layout the actions in a menubar and toolbar:
//   try
//   {
//     Glib::ustring ui_info =
//         "<ui>"
//         "  <menubar name='MenuBar'>"
//         "    <menu action='FileMenu'>"
//         "      <menuitem action='New'/>"
//         "      <menu action='FileNewMenu'>"
//         "        <menuitem action='NewAudioCD'/>"
//         "        <menuitem action='NewDuplicateCD'/>"
//         "        <menuitem action='NewDumpCD'/>"
//         "      </menu>"
//         "      <menuitem action='Open'/>"
//         "      <placeholder name='FileSaveHolder'/>"
//         "      <separator/>"
//         "      <menuitem action='Close'/>"
//         "      <menuitem action='Quit'/>"
//         "    </menu>"
//         "    <menu action='EditMenu'/>"
//         "    <menu action='ActionsMenu'>"
//         "      <placeholder name='ActionsRecordHolder'/>"
//         "      <menuitem action='BlankCD'/>"
//         "      <separator/>"
//         "    </menu>"
//         "    <menu action='SettingsMenu'>"
//         "      <menuitem action='ConfigureDevices'/>"
// 	"      <separator/>"
//         "      <menuitem action='Preferences'/>"
//         "    </menu>"
//         "    <menu action='HelpMenu'>"
//         "      <menuitem action='About'/>"
//         "    </menu>"
//         "  </menubar>"
//         "  <toolbar name='ToolBar'>"
//         "    <toolitem action='New'/>"
//         "    <toolitem action='Open'/>"
//         "  </toolbar>"
//         "</ui>";

//     m_refUIManager->add_ui_from_string(ui_info);
//   }
//   catch(const Glib::Error& ex)
//   {
//     std::cerr << "building menus failed: " <<  ex.what() << "\n";
//     exit(1);
//   }

//   Gtk::Widget* pMenuBar = m_refUIManager->get_widget("/MenuBar");
//   set_menus(dynamic_cast<Gtk::MenuBar&>(*pMenuBar));
//   Gtk::Widget* pToolbar = m_refUIManager->get_widget("/ToolBar");
//   set_toolbar(dynamic_cast<Gtk::Toolbar&>(*pToolbar));
// }

bool GCDMaster::openNewProject(const char* s)
{
    TocEdit* tocEdit;

    if (s == NULL || *s == 0 || s[strlen(s) - 1] == '/')
        return false;

    FileExtension type = fileExtension(s);
    switch (type) {

//    case FE_M3U:
//        newAudioCDProject("", NULL, s);
//        break;

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

// void GCDMaster::openProject()
// {
//   readFileSelector_->present();
//   int result = readFileSelector_->run();
//   readFileSelector_->hide();

//   if (result == Gtk::RESPONSE_OK) {
//     std::string s = readFileSelector_->get_filename();
//     openNewProject(s.c_str());
//   }
// }

// bool GCDMaster::closeProject()
// {
//   if (chooser_)
//     closeChooser();

//   if (project_) {
//     if (project_->closeProject()) {
//       delete project_;
//       project_ = NULL;
//     } else {
//       return false; // User clicked on cancel
//     }
//   }

//   if (readFileSelector_)
//     delete readFileSelector_;

//   apps.remove(this);
//   delete this;

//   if (apps.size() == 0)
//     Gnome::Main::quit(); // Quit if there are not remaining windows

//   return true;
// }

// void GCDMaster::closeChooser()
// {
//   delete chooser_;
//   chooser_ = NULL;
// }

// bool GCDMaster::on_delete_event(GdkEventAny* e)
// {
//   closeProject();
//   return true;
// }

// // Application Close. Called when the user selects 'Quit' from the
// // menu. Try to close all project windows and quit.

// void GCDMaster::appClose()
// {
//   // Can't just iterate, as closeProject will remove its object from
//   // the list.
//   while (apps.size() > 0) {
    
//     if (!(*(GCDMaster::apps.begin()))->closeProject())
//       return;
//   }

//   return;
// }

void GCDMaster::newChooserWindow()
{
    auto window = GCDWindow::create(builder_, GCDWindow::What::CHOOSER);
    add_window(*window);

    window->present();

    auto chooser = dynamic_cast<ProjectChooser*>(window->project());
}

void GCDMaster::newEmptyAudioCDProject()
{
    newAudioCDProject(NULL, NULL);
}

void GCDMaster::newAudioCDProject(const char* name, TocEdit* tocEdit)
{
    auto window = GCDWindow::create(builder_, GCDWindow::What::AUDIOCD,
                                    name, tocEdit);
    add_window(*window);

    window->show_all_children();
    window->present();
}

// void GCDMaster::newAudioCDProject2()
// {
//   newAudioCDProject("", NULL);
// }

void GCDMaster::newDuplicateCDProject()
{
    auto window = GCDWindow::create(builder_, GCDWindow::What::DUPLICATE);
    add_window(*window);

    window->show_all_children();
    window->present();
}
void GCDMaster::newDumpCDProject()
{
    auto window = GCDWindow::create(builder_, GCDWindow::What::DUMP);
    add_window(*window);

    window->show_all_children();
    window->present();
}

void GCDMaster::update(unsigned long level)
 {
     for (auto window : get_windows()) {
         GCDWindow* gw = dynamic_cast<GCDWindow*>(window);
         if (gw)
             gw->update(level);
     }

     if (blankCDWindow_)
         blankCDWindow_->update(level);

     if (preferencesDialog_)
         preferencesDialog_->update(level);
 }

// void GCDMaster::configureDevices()
// {
// //  deviceConfDialog->start();
// }

// void GCDMaster::createStatusbar()
// {
//   Gtk::HBox *container = new Gtk::HBox;
//   statusbar_ = new Gnome::UI::AppBar(false, true,
//                                      Gnome::UI::PREFERENCES_NEVER);
//   progressbar_ = new Gtk::ProgressBar;
//   progressButton_ = new Gtk::Button("Cancel");
//   progressButton_->set_sensitive(false);

//   progressbar_->set_size_request(150, -1);
//   container->pack_start(*statusbar_, true, true); 
//   container->pack_start(*progressbar_, false, false); 
//   container->pack_start(*progressButton_, false, false); 
//   set_statusbar_custom(*container, *statusbar_);
//   container->set_spacing(2);
//   container->set_border_width(2);
//   container->show_all();
//   install_menu_hints();
// }

void GCDMaster::on_startup()
{
    printf("on_startup()\n");

    // Call the base class implementation
    Gtk::Application::on_startup();

    // Configure Menu
    try {
        builder_->add_from_resource("/org/gnome/gcdmaster/app_menu.ui");
    } catch (const Glib::Error& ex) {
        std::cerr << "on_startup() menu: " << ex.what() << std::endl;
        return;
    }

    // Add actions
    add_action("preferences",
               sigc::mem_fun(*this, &GCDMaster::on_action_preferences));
    add_action("quit", sigc::mem_fun(*this, &GCDMaster::on_action_quit));
    set_accel_for_action("app.quit", "<Ctrl>Q");
    add_action("new-audio-cd",
               sigc::mem_fun(this, &GCDMaster::newEmptyAudioCDProject));
    add_action("new-duplicate-cd",
               sigc::mem_fun(this, &GCDMaster::newDuplicateCDProject));
    add_action("new-dump-cd",
               sigc::mem_fun(this, &GCDMaster::newDumpCDProject));
    add_action("about",
               sigc::mem_fun(this, &GCDMaster::on_action_about));
    add_action("blank-cdrw",
               sigc::mem_fun(this, &GCDMaster::on_action_blank_cdrw));

    auto object = builder_->get_object("app-menu");
    auto app_menu = Glib::RefPtr<Gio::MenuModel>::cast_dynamic(object);
    if (app_menu) {
        set_app_menu(app_menu);
    }  else {
        std::cerr << "on_startup() no app menu" << std::endl;
        return;
    }
}

void GCDMaster::on_action_quit()
{
    printf("on_action_quit()\n");
    auto windows = get_windows();
    for (auto window : windows)
        window->hide();

    quit();
}

void GCDMaster::on_action_preferences()
{
    printf("on_action_preferences()\n");
    if (!preferencesDialog_) {
        preferencesDialog_ = PreferencesDialog::create(builder_);

    }
    auto windows = get_windows();
    preferencesDialog_->set_transient_for(*windows[0]);
    preferencesDialog_->run();
    preferencesDialog_->hide();
}

void GCDMaster::on_activate()
{
    printf("on_activate()\n");
//    newChooserWindow();
    openNewProject("/home/denis/tmp/foo.toc");
}

void GCDMaster::on_action_about()
{
    if (aboutDialog_) {
        // "About" dialog hasn't been closed, so just raise it
        aboutDialog_->run();
        aboutDialog_->hide();

    } else {

        aboutDialog_ = Glib::RefPtr<Gtk::AboutDialog>(new Gtk::AboutDialog());

        std::vector<Glib::ustring> authors;
        authors.push_back("Andreas Mueller <mueller@daneb.ping.de>");
        authors.push_back("Manuel Clos <llanero@jazzfree.com>");
        authors.push_back("Denis Leroy <denis@poolshark.org> (maintainer)");
        aboutDialog_->set_authors(authors);

        aboutDialog_->set_program_name("gcdmaster");
        aboutDialog_->set_version(VERSION);

        aboutDialog_->set_website("hhttps://github.com/cdrdao/cdrdao/wiki");
        aboutDialog_->set_comments("A Gnome Audio CD Mastering Tool");
        aboutDialog_->set_copyright("Copyright \xc2\xa9 2000-2018 Andreas Mueller, Manuel Clos, Denis Leroy");
        aboutDialog_->set_logo(Gdk::Pixbuf::create_from_resource("/org/gnome/gcdmaster/gcdmaster.png"));
        aboutDialog_->set_wrap_license(true);
        aboutDialog_->set_license_type(Gtk::LICENSE_GPL_2_0);

        auto windows = get_windows();
        aboutDialog_->set_transient_for(*windows[0]);

        aboutDialog_->run();
        aboutDialog_->hide();
    }
}

void GCDMaster::on_action_blank_cdrw()
{
    printf("on_action_blank_cdrw()\n");
    if (!blankCDWindow_) {
        blankCDWindow_ = new BlankCDWindow();
        add_window(*blankCDWindow_);
        blankCDWindow_->start();
    }
    blankCDWindow_->present();
}

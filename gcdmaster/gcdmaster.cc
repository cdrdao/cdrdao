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

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "config.h"

#include "xcdrdao.h"
#include "AudioCDProject.h"
#include "BlankCDDialog.h"
#include "DeviceConfDialog.h"
#include "Icons.h"
#include "PreferencesDialog.h"
#include "TocEdit.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "util.h"

GCDMaster::GCDMaster()
    : Gtk::Application("org.gnome.gcdmaster", Gio::Application::HANDLES_OPEN)
{
    builder_ = Gtk::Builder::create();
}

void GCDMaster::on_open(const type_vec_file& files, const Glib::ustring& hint)
{
    for (auto file : files)
        openNewProject(i->get_path());
}

void GCDMaster::openNewProject(const Glib::ustring path = "")
{
   try {
        auto new_win = new AudioCDProject(path);
        add_window(*window);
        window->present();
    } catch (std::exception& e) {
        std::cerr << "Failed\n";
    }
}

void GCDMaster::update(unsigned long level)
{
    for (auto window : get_windows()) {
        auto w = dynamic_cast<AudioCDProject*>(window);
        if (w)
            w->update(level);
    }
    if (blankCDWindow_)
        blankCDWindow->update(level);

    if (preferences_)
        preferences_->update(level);
}

void GCDMaster::on_startup()
{
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
    add_action("new",
               sigc::mem_fun(this, &GCDMaster::newChooserWindow));
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
    add_action("open",
               sigc::mem_fun(this, &GCDMaster::on_action_open));

    set_accel_for_action("app.new", "<Primary>n");
    set_accel_for_action("app.open", "<Primary>o");

    // Configure file chooser
    all_filter_ = Gtk::FileFilter::create();
    all_filter_->set_name("Any files");
    all_filter_->add_pattern("*");
    open_filter_ = Gtk::FileFilter::create();
#ifdef HAVE_MP3_SUPPORT
    open_filter_->set_name("Content Files (*.toc, *.cue, *.m3u)");
#else
    open_filter_->set_name("Content Files (*.toc, *.cue)");
#endif
    open_filter_->add_pattern("*.toc");
    open_filter_->add_pattern("*.cue");
#ifdef HAVE_MP3_SUPPORT
    open_filter_->add_pattern("*.m3u");
#endif
    openFileChooser_ = Gtk::FileDialog::create();
    openFileChooser_->add_filter(open_filter_);
    openFileChooser_->add_filter(all_filter_);

    auto object = builder_->get_object("app-menu");
    auto app_menu = Glib::RefPtr<Gio::MenuModel>::cast_dynamic(object);
    if (app_menu) {
        set_app_menu(app_menu);
    }  else {
        std::cerr << "on_startup() no app menu" << std::endl;
        return;
    }

    about_ = Glib::RefPtr<Gtk::AboutDialog>(new Gtk::AboutDialog());
    std::vector<Glib::ustring> authors;
    authors.push_back("Andreas Mueller <mueller@daneb.ping.de>");
    authors.push_back("Manuel Clos <llanero@jazzfree.com>");
    authors.push_back("Denis Leroy <denis@poolshark.org> (maintainer)");
    about_->set_authors(authors);
    about_->set_program_name("gcdmaster");
    about_->set_version(VERSION);

    about_->set_website("hhttps://github.com/cdrdao/cdrdao/wiki");
    about_->set_comments("A Gnome Audio CD Mastering Tool");
    about_->set_copyright("Copyright \xc2\xa9 2000-2025 The Cdrdao Team");
    about_->set_logo(Gdk::Pixbuf::create_from_resource("/org/gnome/gcdmaster/gcdmaster.png"));
    about_->set_wrap_license(true);
    about_->set_license_type(Gtsk::LICENSE_GPL_2_0);
}

void GCDMaster::on_action_open()
{
    auto windows = get_windows();
    openFileChooser_.set_transient_for(*windows[0]);
    openFileChooser_.present();
    int result = openFileChooser_.run();
    openFileChooser_.hide();

    if (result == Gtk::RESPONSE_OK) {
        auto s = openFileChooser_.get_filename();
        openNewProject(s.c_str());
    }
}

void GCDMaster::on_action_quit()
{
    auto windows = get_windows();
    for (auto window : windows)
        window->hide();

    quit();
}

void GCDMaster::on_action_preferences()
{
    if (!preferencesDialog_) {
        preferencesDialog_ = PreferencesDialog::create(builder_);

    }
    auto windows = get_windows();
    preferencesDialog_->set_transient_for(*windows[0]);
    preferencesDialog_->run();
    preferencesDialog_->hide();
}

// Called only when gcdmaster is called without open arguments.
//
void GCDMaster::on_activate()
{
    openNewProject();
}

void GCDMaster::on_action_about()
{
    about_->present();
}

void GCDMaster::on_action_blank_cdrw()
{
    if (!blankCDWindow_) {
        blankCDWindow_ = new BlankCDWindow();
        add_window(*blankCDWindow_);
        blankCDWindow_->start();
    }
    blankCDWindow_->present();
}

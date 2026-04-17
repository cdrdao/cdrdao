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
#include "PreferencesDialog.h"
#include "TocEdit.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "util.h"
#include "log.h" 

GCDWindow::GCDWindow()
{
    set_icon_name("gcdmaster");
}

GCDWindow::GCDWindow(BaseObjectType* cobject)
    : Gtk::ApplicationWindow(cobject)
{
    set_icon_name("gcdmaster");
}

Glib::RefPtr<GCDMaster> GCDMaster::create()
{
    return Glib::make_refptr_for_instance<GCDMaster>(new GCDMaster());
}

GCDMaster::GCDMaster()
    : Adw::Application("org.gnome.gcdmaster", Gio::Application::Flags::HANDLES_OPEN)
{
    builder_ = Gtk::Builder::create();
}

void GCDMaster::on_open(const type_vec_files& files, const Glib::ustring& hint)
{
    for (auto file : files)
        openNewProject(file->get_path());
}

void GCDMaster::openNewProject(const Glib::ustring path = "")
{
   try {
       auto project = dynamic_cast<AudioCDProject*>(get_active_window());
       if (path.empty() || !project || project->tocEdit()->tocDirty()) {
	   auto new_win = AudioCDProject::create(builder_, path);
	   add_window(*new_win);
	   new_win->present();
	   guiUpdate(UPD_ALL);
       } else {
	   if (!path.empty())
	       project->loadNewToc(path);
       }
    } catch (std::exception& e) {
       std::cerr << "Failed: " << e.what() << "\n";
    }
}

void GCDMaster::newEmptyProject()
{
    openNewProject("");
}

void GCDMaster::update(unsigned long level)
{
    log_message(1, "[update %x] GCDMaster", level);
    for (auto window : get_windows()) {
        auto w = dynamic_cast<GCDWindow*>(window);
        if (w)
            w->update(level);
    }
    if (preferences_)
	preferences_->update(level);

    if (blanker_)
	blanker_->update(level);
}

void GCDMaster::on_startup()
{
    // Call the base class implementation
    Gtk::Application::on_startup();

    // Add actions
    add_action("preferences",
               sigc::mem_fun(*this, &GCDMaster::on_action_preferences));
    add_action("quit", sigc::mem_fun(*this, &GCDMaster::on_action_quit));
    add_action("new",
               sigc::mem_fun(*this, &GCDMaster::newEmptyProject));
    add_action("about",
               sigc::mem_fun(*this, &GCDMaster::on_action_about));
    add_action("blank-cdrw",
               sigc::mem_fun(*this, &GCDMaster::on_action_blank_cdrw));
    add_action("open",
               sigc::mem_fun(*this, &GCDMaster::on_action_open));
    add_action("close",
               sigc::mem_fun(*this, &GCDMaster::on_action_close));

    // You'd think the whole point of using <Primary> would be that it
    // matches to the right main modifier for each OS, i.e. Command on MacOS. But
    // noooooooooooo, that would be too simple.
#ifdef OS_DARWIN
    set_accel_for_action("app.quit", "<Meta>q");
    set_accel_for_action("app.new", "<Meta>n");
    set_accel_for_action("app.open", "<Meta>o");
    set_accel_for_action("app.close", "<Meta>w");
#else
    set_accel_for_action("app.quit", "<Primary>q");
    set_accel_for_action("app.new", "<Primary>n");
    set_accel_for_action("app.open", "<Primary>o");
    set_accel_for_action("app.close", "<Primary>w");
#endif

    // Configure file chooser
    allFilter_ = Gtk::FileFilter::create();
    allFilter_->set_name("Any files");
    allFilter_->add_pattern("*");
    openFilter_ = Gtk::FileFilter::create();
#ifdef HAVE_MP3_SUPPORT
    openFilter_->set_name("Content Files (*.toc, *.cue, *.m3u)");
#else
    openFilter_->set_name("Content Files (*.toc, *.cue)");
#endif
    openFilter_->add_pattern("*.toc");
    openFilter_->add_pattern("*.cue");
#ifdef HAVE_MP3_SUPPORT
    openFilter_->add_pattern("*.m3u");
#endif
    auto filter_list = Gio::ListStore<Gtk::FileFilter>::create();
    filter_list->append(openFilter_);
    filter_list->append(allFilter_);

    openFileChooser_ = Gtk::FileDialog::create();
    openFileChooser_->set_filters(filter_list);

    std::vector<Glib::ustring> authors;
    authors.push_back("Denis Leroy <denis@poolshark.org>");
    authors.push_back("Manuel Clos <llanero@jazzfree.com>");
    about_.set_authors(authors);
    about_.set_program_name("gcdmaster");
    about_.set_version(VERSION);
    about_.set_hide_on_close();

    about_.set_website("hhttps://github.com/cdrdao/cdrdao/wiki");
    about_.set_comments("A Gnome Audio CD Mastering Tool");
    about_.set_copyright("Copyright \xc2\xa9 2000-2026 The Cdrdao Team");
    about_.set_logo_icon_name("gcdmaster");
    about_.set_wrap_license(true);
    about_.set_license_type(Gtk::License::GPL_2_0);

    // Configure update periodic signal.
    updateSignal_ = Glib::signal_timeout().connect(sigc::ptr_fun(&guiUpdatePeriodic), 2000);
}

void GCDMaster::on_action_close()
{
    log_message(0, "GCDMaster: on action close");
    get_active_window()->close();
}

void GCDMaster::on_action_open()
{
    auto windows = get_windows();
    if (windows.empty())
	return;

    openFileChooser_->open(sigc::mem_fun(
			       *this, &GCDMaster::on_action_open_finish));
}

void GCDMaster::on_action_open_finish(const Glib::RefPtr<Gio::AsyncResult>& result)
{
    try {
	auto file = openFileChooser_->open_finish(result);
	openNewProject(file->get_path());
    }
    catch (...)
    {
    }
}

void GCDMaster::on_action_quit()
{
    updateSignal_.disconnect();
    log_message(0, "GCDMaster::on_action_quit, closing %d window(s)", get_windows().size());
    for (auto window : get_windows()) {
	log_message(0, "Closing window %s", window->get_title().c_str());
	remove_window(*window);
    }
    log_message(0, "All windows closed.");
    quit();
}

void GCDMaster::on_action_preferences()
{
    if (preferences_) {
        preferences_->present();
        return;
    }
    try {
        preferences_ = PreferencesDialog::create(builder_,
                                                 *get_active_window());
        add_window(*preferences_);
        preferences_->present();
        preferences_->signal_hide().connect([this](){
            this->remove_window(*preferences_);
            delete(preferences_);
            this->preferences_ = nullptr; });
    } catch (const std::exception& ex)
    {
        std::cerr << "Preferences: " << ex.what() << "\n";
    }
}

// Called only when gcdmaster is called without open arguments.
//
void GCDMaster::on_activate()
{
    openNewProject();
}

void GCDMaster::on_action_about()
{
    about_.set_visible();
}

void GCDMaster::on_action_blank_cdrw()
{
    if (blanker_) {
	blanker_->present();
	return;
    }

    try {
        blanker_ = BlankCDDialog::create(builder_, *get_active_window());
        blanker_->start();
        blanker_->present();
        blanker_->signal_hide().connect([this](){
	    delete(blanker_);
	    this->blanker_ = nullptr; });
    } catch (const std::exception& ex)
    {
        std::cerr << "Blank CD/RW: " << ex.what() << "\n";
    }
}

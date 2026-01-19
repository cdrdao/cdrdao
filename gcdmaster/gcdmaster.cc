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

#include "AudioCDProject.h"
#include "BlankCDDialog.h"
#include "DeviceConfDialog.h"
#include "DumpCDProject.h"
#include "DuplicateCDProject.h"
#include "Icons.h"
#include "PreferencesDialog.h"
#include "ProjectChooser.h"
#include "TocEdit.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "util.h"
#include "xcdrdao.h"

// Static class members
std::list<GCDMaster *> GCDMaster::apps;

GCDMaster::GCDMaster()
{
    set_title(APP_NAME);
    set_resizable();

    readFileSelector_ =
        new Gtk::FileChooserDialog(_("Please select a project"), Gtk::FileChooser::Action::OPEN);
    readFileSelector_->set_transient_for(*this);
    readFileSelector_->add_button(_("_Cancel"), Gtk::ResponseType::CANCEL);
    readFileSelector_->add_button(_("_Open"), Gtk::ResponseType::OK);

    auto filter_tocs = Gtk::FileFilter::create();
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
    readFileSelector_->add_filter(filter_tocs);

    auto filter_all = Gtk::FileFilter::create();
    filter_all->set_name("Any files");
    filter_all->add_pattern("*");
    readFileSelector_->add_filter(filter_all);

    Icons::registerIcons();
    notebook_.set_show_border(false);
    notebook_.show();

    set_child(box_);
    box_.set_orientation(Gtk::Orientation::VERTICAL);

    createMenus();
    createStatusbar();

    box_.append(notebook_);
    box_.append(*container_);

    if (!apps.empty())
        set_application(app);

    apps.push_back(this);
}

void GCDMaster::createMenus()
{
    // Create menu using Gio::Menu instead of deprecated UIManager
    auto menu = Gio::Menu::create();
    
    // File menu
    auto file_menu = Gio::Menu::create();
    auto file_new_menu = Gio::Menu::create();
    file_new_menu->append(_("_Audio CD"), "win.new-audio-cd");
    file_new_menu->append(_("_Duplicate CD"), "win.new-duplicate-cd");
    file_new_menu->append(_("_Copy CD to disk"), "win.new-dump-cd");
    
    file_menu->append_submenu(_("N_ew"), file_new_menu);
    file_menu->append(_("_Open"), "win.open");
    file_menu->append(_("_Close"), "win.close");
    file_menu->append(_("_Quit"), "app.quit");
    menu->append_submenu(_("_File"), file_menu);
    
    // Edit menu
    auto edit_menu = Gio::Menu::create();
    menu->append_submenu(_("_Edit"), edit_menu);
    
    // Actions menu
    auto actions_menu = Gio::Menu::create();
    actions_menu->append(_("Blank CD-RW"), "win.blank-cd");
    menu->append_submenu(_("_Actions"), actions_menu);
    
    // Settings menu
    auto settings_menu = Gio::Menu::create();
    settings_menu->append(_("Configure Devices..."), "win.configure-devices");
    settings_menu->append(_("_Preferences..."), "win.preferences");
    menu->append_submenu(_("_Settings"), settings_menu);
    
    // Help menu
    auto help_menu = Gio::Menu::create();
    help_menu->append(_("About"), "win.about");
    menu->append_submenu(_("_Help"), help_menu);
    
    // Create actions
    add_action("new-audio-cd", sigc::mem_fun(*this, &GCDMaster::newAudioCDProject2));
    add_action("new-duplicate-cd", sigc::mem_fun(*this, &GCDMaster::newDuplicateCDProject));
    add_action("new-dump-cd", sigc::mem_fun(*this, &GCDMaster::newDumpCDProject));
    add_action("open", sigc::mem_fun(*this, &GCDMaster::openProject));
    add_action("close", sigc::mem_fun(*this, &GCDMaster::closeProjectAction));
    add_action("blank-cd", sigc::mem_fun(*this, &GCDMaster::blankCDRW));
    add_action("configure-devices", sigc::mem_fun(*this, &GCDMaster::configureDevices));
    add_action("preferences", sigc::mem_fun(*this, &GCDMaster::configurePreferences));
    add_action("about", sigc::mem_fun(*this, &GCDMaster::aboutDialog));

    // Create About dialog
    std::vector<Glib::ustring> authors;
    authors.push_back("Manuel Clos <llanero@jazzfree.com>");
    authors.push_back("Denis Leroy <denis@poolshark.org>");
    authors.push_back("Andreas Mueller <mueller@daneb.ping.de>");
    authors.push_back("Stefan Roellin <stefan.roellin@gmx.ch>");
    about_.set_program_name("gcdmaster");
    about_.set_version(VERSION);
    about_.set_copyright("(C) The Cdrdao Team");
    about_.set_authors(authors);

        // Create menubar
    auto menubar = Gtk::make_managed<Gtk::PopoverMenuBar>(menu);
    box_.prepend(*Gtk::make_managed<Gtk::MenuButton>());
    
    // Create toolbar
    auto toolbar = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 2);
    
    auto new_button = Gtk::make_managed<Gtk::Button>(_("New"));
    new_button->signal_clicked().connect(sigc::mem_fun(*this, &GCDMaster::newChooserWindow));
    toolbar->append(*new_button);
    
    auto open_button = Gtk::make_managed<Gtk::Button>(_("Open"));
    open_button->signal_clicked().connect(sigc::mem_fun(*this, &GCDMaster::openProject));
    toolbar->append(*open_button);
    
    box_.prepend(*toolbar);
    box_.prepend(*menubar);
}

bool GCDMaster::openNewProject(const std::string &str)
{
    TocEdit *tocEdit;
    const char *s = str.c_str();

    if (s == NULL || *s == 0 || s[strlen(s) - 1] == '/')
        return false;

    auto type = Util::fileExtension(s);
    switch (type) {

    case Util::FileExtension::M3U:
        newAudioCDProject("", NULL, s);
        break;

    case Util::FileExtension::TOC:
        tocEdit = new TocEdit(NULL, NULL);
        if (tocEdit->readToc(stripCwd(s)) == 0)
            newAudioCDProject(stripCwd(s), tocEdit);
        else
            return false;
        break;

    case Util::FileExtension::CUE:
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
    readFileSelector_->signal_response().connect(
        sigc::bind(sigc::mem_fun(*this, &GCDMaster::on_file_dialog_response),
                   readFileSelector_));
}

void GCDMaster::on_file_dialog_response(int response, Gtk::FileChooserDialog* dialog)
{
    dialog->hide();
    
    if (response == Gtk::ResponseType::OK) {
        auto file = dialog->get_file();
        if (file) {
            std::string s = file->get_path();
            openNewProject(s);
        }
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

    GCDMaster::apps.remove(this);
    delete this;

    return true;
}

void GCDMaster::closeProjectAction()
{
    closeProject();
}

void GCDMaster::closeChooser()
{
    delete chooser_;
    chooser_ = NULL;
}

bool GCDMaster::on_close_request()
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
    while (!GCDMaster::apps.empty()) {
        if (!(GCDMaster::apps.front())->closeProject())
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
        chooser_->newAudioCDProject.connect(sigc::mem_fun(*this, &GCDMaster::newAudioCDProject2));
        chooser_->newDuplicateCDProject.connect(
            sigc::mem_fun(*this, &GCDMaster::newDuplicateCDProject));
        chooser_->newDumpCDProject.connect(sigc::mem_fun(*this, &GCDMaster::newDumpCDProject));
        chooser_->show();
        notebook_.set_show_tabs(false);
        notebook_.append_page(*chooser_);
        container_->hide();
    }
}

void GCDMaster::newAudioCDProject(const char *name, TocEdit *tocEdit, const char *tracks)
{
    if (!project_) {

        AudioCDProject *p = new AudioCDProject(project_number++, name, tocEdit, this);
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
        container_->show();
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
        container_->show();
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
        container_->show();
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
    preferencesDialog->show();
}

void GCDMaster::blankCDRW()
{
    blankCDDialog_.start(*this);
}

void GCDMaster::createStatusbar()
{
    container_ = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    statusbar_ = new Gtk::Statusbar();
    statusbar_->set_hexpand();
    progressbar_ = new Gtk::ProgressBar();
    progressButton_ = new Gtk::Button(_("Cancel"));
    progressButton_->set_sensitive(false);

    progressbar_->set_size_request(150, -1);

    container_->append(*statusbar_);
    container_->append(*progressbar_);
    container_->append(*progressButton_);
    container_->set_spacing(2);
    container_->set_margin(2);
}

void GCDMaster::aboutDialog()
{
    about_.set_visible(true);
    about_.present();
}

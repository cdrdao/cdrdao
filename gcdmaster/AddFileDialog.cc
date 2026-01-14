/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include "config.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "AddFileDialog.h"
#include "AudioCDProject.h"
#include "Sample.h"
#include "TocEdit.h"
#include "config.h"
#include "guiUpdate.h"
#include "util.h"
#include "xcdrdao.h"

AddFileDialog::AddFileDialog(AudioCDProject *project) :
    project_(project)
{
    file_dialog_ = Gtk::FileDialog::create();
    file_dialog_->set_modal(true);
    mode(M_APPEND_TRACK);

    // Set up filters
    filters_ = Gio::ListStore<Gtk::FileFilter>::create();

    auto filter_audio = Gtk::FileFilter::create();
    filter_audio->set_name(_("Audio Files"));
    filter_audio->add_pattern("*.wav");
#ifdef HAVE_OGG_SUPPORT
    filter_audio->add_pattern("*.ogg");
#endif
#ifdef HAVE_MP3_SUPPORT
    filter_audio->add_pattern("*.mp3");
    filter_audio->add_pattern("*.m3u");
#endif
#ifdef HAVE_FLAC_SUPPORT
    filter_audio->add_pattern("*.flac");
#endif
    filters_->append(filter_audio);

    auto filter_all = Gtk::FileFilter::create();
    filter_all->set_name(_("Any files"));
    filter_all->add_pattern("*");
    filters_->append(filter_all);

    file_dialog_->set_filters(filters_);
}

void AddFileDialog::mode(Mode m)
{
    mode_ = m;

    switch (mode_) {
    case M_APPEND_TRACK:
        file_dialog_->set_title(_("Append Track"));
        break;
    case M_APPEND_FILE:
        file_dialog_->set_title(_("Append File"));
        break;
    case M_INSERT_FILE:
        file_dialog_->set_title(_("Insert File"));
        break;
    }
}

void AddFileDialog::start()
{
    file_dialog_->open_multiple(*project_->getParentWindow(),
				sigc::mem_fun(*this,
					      &AddFileDialog::on_file_dialog_finish));
}

void AddFileDialog::on_file_dialog_finish(const Glib::RefPtr<Gio::AsyncResult>& result)
{
    try {
        auto files_list = file_dialog_->open_multiple_finish(result);
	applyAction(files_list);
    }
    catch (const Gtk::DialogError& e) {
        // User cancelled or error occurred
    }
    catch (const Glib::Error& e) {
        // Handle other errors
    }
}

bool AddFileDialog::applyAction(const std::vector<Glib::RefPtr<Gio::File>>& selected_files)
{
    std::list<std::string> files_to_process;

    for (const auto& file : selected_files) {
        std::string path = file->get_path();
        const char *s = stripCwd(path.c_str());

        if (s && *s != 0) {
            if (Util::fileExtension(s) == Util::FileExtension::M3U)
                parseM3u(s, files_to_process);
            else
                files_to_process.push_back(s);
        }
    }

    if (!files_to_process.empty()) {
        switch (mode_) {
        case M_APPEND_TRACK:
            project_->appendTracks(files_to_process);
            break;
        case M_APPEND_FILE:
            project_->appendFiles(files_to_process);
            break;
        case M_INSERT_FILE:
            project_->insertFiles(files_to_process);
            break;
        }
    }

    return true;
}

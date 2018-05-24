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

#include "xcdrdao.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "ProjectChooser.h"

#define ICON_PADDING 10
#define LABEL_PADDING 10
#define BUTTONS_RELIEF Gtk::RELIEF_NORMAL


ProjectChooser::ProjectChooser(BaseObjectType* cobject,
                               const Glib::RefPtr<Gtk::Builder>& builder) :
    Gtk::VBox(cobject)
{
    Gtk::Button* button;

#define MAP_BUTTON_TO_ACTION(id, method) \
    builder->get_widget(#id, button);    \
    if (button)                          \
        button->signal_clicked()         \
            .connect(sigc::mem_fun(*this, &ProjectChooser::method))

    MAP_BUTTON_TO_ACTION(AudioButton, on_new_audio_cd);
    MAP_BUTTON_TO_ACTION(CopyButton, on_new_duplicate_cd);
    MAP_BUTTON_TO_ACTION(DumpButton, on_new_dump_cd);
}

ProjectChooser::~ProjectChooser()
{
    printf("Chooser deleted.\n");
}

ProjectChooser* ProjectChooser::create(Glib::RefPtr<Gtk::Builder>& builder,
                                       GCDWindow* window)
{
    builder->add_from_resource("/org/gnome/gcdmaster/chooser.ui");

    ProjectChooser* chooser;
    builder->get_widget_derived("chooser_box", chooser);
    if (!chooser)
        throw std::runtime_error("No \"chooser_box\" object in chooser.ui");

    printf("Chooser created.\n");
    chooser->window_ = window;
    return chooser;
}

void ProjectChooser::on_new_audio_cd(void)
{
    window_->get_application()->activate_action("new-audio-cd");
    window_->hide();
}

void ProjectChooser::on_new_duplicate_cd(void)
{
    window_->get_application()->activate_action("new-duplicate-cd");
    window_->hide();
}

void ProjectChooser::on_new_dump_cd(void)
{
    window_->get_application()->activate_action("new-dump-cd");
    window_->hide();
}

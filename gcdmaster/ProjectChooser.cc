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
#include "Icons.h"

#define ICON_PADDING 10
#define LABEL_PADDING 10
#define BUTTONS_RELIEF Gtk::RELIEF_NORMAL


ProjectChooser::ProjectChooser(BaseObjectType* cobject,
                               const Glib::RefPtr<Gtk::Builder>& rb) :
    Gtk::VBox(cobject)
{
}

ProjectChooser* ProjectChooser::create()
{
    auto builder =
        Gtk::Builder::create_from_resource("/org/gnome/gcdmaster/chooser.ui");

    ProjectChooser* chooser = NULL;
    builder->get_widget_derived("chooser_box", chooser);
    if (!chooser)
        throw std::runtime_error("No \"chooser_box\" object in chooser.ui");

    return chooser;
}

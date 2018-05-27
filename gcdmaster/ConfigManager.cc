/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2007  Denis Leroy
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

#include "ConfigManager.h"

ConfigManager::ConfigManager()
{
    client_ = Gio::Settings::create("org.gnome.gcdmaster");
}

void ConfigManager::set(const Glib::ustring key, const Glib::ustring value)
{
    client_->set_string(key, value);
}

Glib::ustring ConfigManager::get_string(const Glib::ustring key)
{
    return client_->get_string(key);
}

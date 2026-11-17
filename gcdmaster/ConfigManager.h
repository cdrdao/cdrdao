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

#ifndef __CONFIG_MANAGER_H
#define __CONFIG_MANAGER_H

#include <glibmm/keyfile.h>
#include <glibmm/ustring.h>
#include <string>
#include <vector>

// Persistent application settings, stored as a GKeyFile (INI-style)
// in the per-user configuration directory returned by
// Glib::get_user_config_dir():
//
//   Linux/FreeBSD/macOS: $XDG_CONFIG_HOME/gcdmaster/gcdmaster.conf
//                        (defaults to ~/.config/gcdmaster/gcdmaster.conf)
//   Windows:             %LOCALAPPDATA%\gcdmaster\gcdmaster.conf
//
// Every setter writes the file immediately.

class ConfigManager
{
  public:
    ConfigManager();
    virtual ~ConfigManager();

    Glib::ustring getCdrdaoPath() const;
    void setCdrdaoPath(const Glib::ustring &path);

    Glib::ustring getTempDir() const;
    void setTempDir(const Glib::ustring &dir);

    bool getEjectWarning() const;
    void setEjectWarning(bool value);

    bool getDuplicateOnTheFlyWarning() const;
    void setDuplicateOnTheFlyWarning(bool value);

    bool getReloadWarning() const;
    void setReloadWarning(bool value);

    void setConfiguredDevices(const std::vector<Glib::ustring> &strings);
    std::vector<Glib::ustring> getConfiguredDevices() const;

    const std::string &filename() const { return filename_; }

  protected:
    Glib::ustring getString(const char *key, const Glib::ustring &def) const;
    bool getBoolean(const char *key, bool def) const;
    void save();

    Glib::RefPtr<Glib::KeyFile> keyFile_;
    std::string filename_;
};

#endif

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

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "log.h"

static const char *GROUP = "gcdmaster";
static const char *KEY_CDRDAO_PATH = "cdrdao-path";
static const char *KEY_TEMP_DIR = "temp-dir";
static const char *KEY_RECORD_EJECT_WARNING = "record-eject-warning";
static const char *KEY_RELOAD_EJECT_WARNING = "record-reload-warning";
static const char *KEY_DUPLICATE_ON_THE_FLY_WARNING = "duplicate-on-the-fly-warning";
static const char *KEY_CONFIGURED_DEVICES = "configured-devices";

static const char *DEFAULT_CDRDAO_PATH = "cdrdao";

ConfigManager::ConfigManager()
{
    // Glib::get_user_config_dir() and Glib::build_filename() pick the
    // right location and path separator on every platform.
    keyFile_ = Glib::KeyFile::create();
    filename_ = Glib::build_filename(Glib::get_user_config_dir(), "gcdmaster", "gcdmaster.conf");

    // A missing or unreadable file just means default values.
    try {
        keyFile_->load_from_file(filename_, Glib::KeyFile::Flags::KEEP_COMMENTS);
    } catch (const Glib::FileError &e) {
        if (e.code() != Glib::FileError::NO_SUCH_ENTITY)
            log_message(-1, "Cannot read settings file %s: %s", filename_.c_str(), e.what());
    } catch (const Glib::KeyFileError &e) {
        log_message(-1, "Ignoring malformed settings file %s: %s", filename_.c_str(), e.what());
    }
}

ConfigManager::~ConfigManager() {}

Glib::ustring ConfigManager::getString(const char *key, const Glib::ustring &def) const
{
    try {
        return keyFile_->get_string(GROUP, key);
    } catch (const Glib::KeyFileError &) {
        return def;
    }
}

bool ConfigManager::getBoolean(const char *key, bool def) const
{
    try {
        return keyFile_->get_boolean(GROUP, key);
    } catch (const Glib::KeyFileError &) {
        return def;
    }
}

void ConfigManager::save()
{
    std::string dir = Glib::path_get_dirname(filename_);
    if (g_mkdir_with_parents(dir.c_str(), 0700) != 0) {
        log_message(-1, "Cannot create settings directory %s", dir.c_str());
        return;
    }

    // save_to_file() goes through g_file_set_contents(), which writes
    // to a temporary file and renames it, so the file is never left
    // half-written.
    try {
        keyFile_->save_to_file(filename_);
    } catch (const Glib::Error &e) {
        log_message(-1, "Cannot save settings file %s: %s", filename_.c_str(), e.what());
    }
}

Glib::ustring ConfigManager::getCdrdaoPath() const
{
    auto path = getString(KEY_CDRDAO_PATH, DEFAULT_CDRDAO_PATH);
    return path.empty() ? Glib::ustring(DEFAULT_CDRDAO_PATH) : path;
}

void ConfigManager::setCdrdaoPath(const Glib::ustring &path)
{
    keyFile_->set_string(GROUP, KEY_CDRDAO_PATH, path);
    save();
}

Glib::ustring ConfigManager::getTempDir() const
{
    return getString(KEY_TEMP_DIR, "");
}

void ConfigManager::setTempDir(const Glib::ustring &dir)
{
    keyFile_->set_string(GROUP, KEY_TEMP_DIR, dir);
    save();
}

bool ConfigManager::getEjectWarning() const
{
    return getBoolean(KEY_RECORD_EJECT_WARNING, true);
}

void ConfigManager::setEjectWarning(bool value)
{
    keyFile_->set_boolean(GROUP, KEY_RECORD_EJECT_WARNING, value);
    save();
}

bool ConfigManager::getReloadWarning() const
{
    return getBoolean(KEY_RELOAD_EJECT_WARNING, true);
}

void ConfigManager::setReloadWarning(bool value)
{
    keyFile_->set_boolean(GROUP, KEY_RELOAD_EJECT_WARNING, value);
    save();
}

bool ConfigManager::getDuplicateOnTheFlyWarning() const
{
    return getBoolean(KEY_DUPLICATE_ON_THE_FLY_WARNING, true);
}

void ConfigManager::setDuplicateOnTheFlyWarning(bool value)
{
    keyFile_->set_boolean(GROUP, KEY_DUPLICATE_ON_THE_FLY_WARNING, value);
    save();
}

void ConfigManager::setConfiguredDevices(const std::vector<Glib::ustring> &strings)
{
    keyFile_->set_string_list(GROUP, KEY_CONFIGURED_DEVICES, strings);
    save();
}

std::vector<Glib::ustring> ConfigManager::getConfiguredDevices() const
{
    try {
        return keyFile_->get_string_list(GROUP, KEY_CONFIGURED_DEVICES);
    } catch (const Glib::KeyFileError &) {
        return {};
    }
}

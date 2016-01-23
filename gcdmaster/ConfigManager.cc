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

static const char* NAME_SCHEMA = "org.gnome.gcdmaster";
static const char* KEY_CDRDAO_PATH = "cdrdao-path";
static const char* KEY_TEMP_DIR = "temp-dir";
static const char* KEY_RECORD_EJECT_WARNING = "record-eject-warning";
static const char* KEY_RELOAD_EJECT_WARNING = "record-reload-warning";
static const char* KEY_DUPLICATE_ON_THE_FLY_WARNING = "duplicate-on-the-fly-warning";
static const char* KEY_CONFIGURED_DEVICES = "configured-devices";

ConfigManager::ConfigManager()
{
  settings_ = Gio::Settings::create(NAME_SCHEMA);
}

ConfigManager::~ConfigManager()
{
  if (settings_->get_has_unapplied())
  {
    settings_->apply();
  }
}

Glib::ustring ConfigManager::getCdrdaoPath() const
{
  return settings_->get_string(KEY_CDRDAO_PATH);
}

void ConfigManager::setTempDir(const Glib::ustring& dir) const
{
  settings_->set_string(KEY_TEMP_DIR, dir);
  settings_->apply();
}

Glib::ustring ConfigManager::getTempDir() const
{
  return settings_->get_string(KEY_TEMP_DIR);
}

bool ConfigManager::getEjectWarning() const
{
  return settings_->get_boolean(KEY_RECORD_EJECT_WARNING);
}

void ConfigManager::setEjectWarning(bool value)
{
  settings_->set_boolean(KEY_RECORD_EJECT_WARNING, value);
  settings_->apply();
}

bool ConfigManager::getReloadWarning() const
{
  return settings_->get_boolean(KEY_RELOAD_EJECT_WARNING);
}

void ConfigManager::setReloadWarning(bool value)
{
  settings_->set_boolean(KEY_RELOAD_EJECT_WARNING, value);
  settings_->apply();
}

bool ConfigManager::getDuplicateOnTheFlyWarning() const
{
  return settings_->get_boolean(KEY_DUPLICATE_ON_THE_FLY_WARNING);
}

void ConfigManager::setDuplicateOnTheFlyWarning(bool value)
{
  settings_->set_boolean(KEY_DUPLICATE_ON_THE_FLY_WARNING, value);
  settings_->apply();
}

void ConfigManager::setConfiguredDevices(const std::vector<Glib::ustring>& strings)
{
  settings_->set_string_array(KEY_CONFIGURED_DEVICES, strings);
  settings_->apply();
}

std::vector<Glib::ustring> ConfigManager::getConfiguredDevices(void) const
{
  return settings_->get_string_array(KEY_CONFIGURED_DEVICES);
}

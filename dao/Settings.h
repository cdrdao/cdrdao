/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <string>
#include <vector>

class Settings {
public:
  Settings();
  ~Settings();

  int read(const char *);
  int write(const char *) const; 

  const int *getInteger(const char *) const;
  const char *getString(const char *) const;
  bool getStrings(const char *, std::vector<std::string>& strings) const;

  void set(const char *, int);
  void set(const char *, const char *);
  void set(const char *, const std::vector<std::string>& strings);

  // Key name definitions
  static const char* setWriteSpeed;
  static const char* setWriteDriver;
  static const char* setWriteDevice;
  static const char* setWriteBuffers;
  static const char* setUserCapacity;
  static const char* setFullBurn;
  static const char* setReadSpeed;
  static const char* setReadDriver;
  static const char* setReadDevice;
  static const char* setReadParanoiaMode;
  static const char* setCddbServerList;
  static const char* setCddbTimeout;
  static const char* setCddbDbDir;
  static const char* setTmpFileDir;

private:
  class SettingsImpl *impl_;
};

#endif

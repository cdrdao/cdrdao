/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: Settings.h,v $
 * Revision 1.1  2000/02/05 01:38:51  llanero
 * Initial revision
 *
 */

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

extern const char *SET_CDRDAO_PATH;
extern const char *SET_RECORD_EJECT_WARNING;
extern const char *SET_RECORD_RELOAD_WARNING;

class Settings {
public:
  Settings();
  ~Settings();

  int read(const char *);
  int write(const char *) const; 

  int getInteger(const char *) const;
  const char *getString(const char *) const;

  void set(const char *, int);
  void set(const char *, const char *);

private:
  class SettingsImpl *impl_;
};

#endif

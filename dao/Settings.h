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
/*
 * $Log: Settings.h,v $
 * Revision 1.4  2000/08/06 13:13:09  andreasm
 * Added option --cddb-directory and corresponding setting to specify where
 * fetched CDDB record should be stored.
 *
 * Revision 1.3  2000/06/22 12:19:28  andreasm
 * Added switch for reading CDs written in TAO mode.
 * The fifo buffer size is now also saved to $HOME/.cdrdao.
 *
 * Revision 1.2  2000/06/19 20:17:37  andreasm
 * Added CDDB reading to add CD-TEXT information to toc-files.
 * Fixed bug in reading ATIP data in 'GenericMMC::diskInfo()'.
 * Attention: CdrDriver.cc is currently configured to read TAO disks.
 *
 * Revision 1.1  2000/06/06 22:26:13  andreasm
 * Updated list of supported drives.
 * Added saving of some command line settings to $HOME/.cdrdao.
 * Added test for multi session support in raw writing mode to GenericMMC.cc.
 * Updated manual page.
 *
 */

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

extern const char *SET_WRITE_SPEED;
extern const char *SET_WRITE_DRIVER;
extern const char *SET_WRITE_DEVICE;
extern const char *SET_WRITE_BUFFERS;
extern const char *SET_READ_DRIVER;
extern const char *SET_READ_DEVICE;
extern const char *SET_READ_PARANOIA_MODE;
extern const char *SET_CDDB_SERVER_LIST;
extern const char *SET_CDDB_TIMEOUT;
extern const char *SET_CDDB_DB_DIR;

class Settings {
public:
  Settings();
  ~Settings();

  int read(const char *);
  int write(const char *) const; 

  const int *getInteger(const char *) const;
  const char *getString(const char *) const;

  void set(const char *, int);
  void set(const char *, const char *);

private:
  class SettingsImpl *impl_;
};

#endif

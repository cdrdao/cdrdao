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
 * Revision 1.3  2001/08/18 19:15:43  llanero
 * added SET_DUPLICATE_ONTHEFLY_WARNING
 *
 * Revision 1.2  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
 *
 * Revision 1.1.1.1  2000/02/05 01:38:51  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

extern const char *SET_CDRDAO_PATH;
extern const char *SET_RECORD_EJECT_WARNING;
extern const char *SET_RECORD_RELOAD_WARNING;
extern const char *SET_DUPLICATE_ONTHEFLY_WARNING;
extern const char *SET_SECTION_DEVICES;
extern const char *SET_DEVICES_NUM;

#endif

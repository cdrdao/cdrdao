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
 * $Log: TextEdit.h,v $
 * Revision 1.3  2004/02/12 01:13:32  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.2.6.1  2004/01/05 00:34:03  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.2  2003/12/29 09:31:48  denis
 * fixed all dialogs
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:51  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/19 20:28:12  mueller
 * Initial revision
 *
 */

#ifndef __TEXT_EDIT_H__
#define __TEXT_EDIT_H__

#include <gtkmm.h>
#include <gtk/gtk.h>

class TextEdit : public Gtk::Entry
{
public:
  TextEdit(const char *sample);
  ~TextEdit();

  void upperCase(int);
  void lowerCase(int);
  void digits(int);
  void space(int);

protected:
  virtual void insert_text_impl(const gchar *p1,gint p2,gint *p3);

private:
  unsigned int upper_ : 1;
  unsigned int lower_ : 1;
  unsigned int digits_ : 1;
  unsigned int space_ : 1;

  void setSize(const char *sample);
};

#endif

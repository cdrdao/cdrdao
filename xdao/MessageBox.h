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
 * $Log: MessageBox.h,v $
 * Revision 1.4  2004/02/12 01:13:31  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.3.6.1  2004/01/05 00:34:02  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.3  2004/01/01 21:21:26  denis
 * Dialog modality fixes
 *
 * Revision 1.2  2003/12/09 20:08:04  denis
 * Now using on_delete_event
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.3  2000/05/01 18:15:00  andreasm
 * Switch to gnome-config settings.
 * Adapted Message Box to Gnome look, unfortunately the Gnome::MessageBox is
 * not implemented in gnome--, yet.
 *
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:46  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

#ifndef __MESSAGE_BOX_H__
#define __MESSAGE_BOX_H__

#include <gtkmm.h>
#include <gtk/gtk.h>
#include <stdarg.h>

class MessageBoxBase : public Gtk::Dialog
{
public:
  MessageBoxBase(Gtk::Window *);
  virtual ~MessageBoxBase();

  void init(const char *type, const char *titel, int askDontShow, int nButtons,
	    int defaultButton, Gtk::BuiltinStockID buttons[], va_list);

  int run();

  int dontShowAgain() const;

protected:
  int done_;
  int doneDefault_;

  Gtk::CheckButton *dontShowAgain_;

  Gtk::Button *createButton(const Gtk::BuiltinStockID);
  bool on_delete_event(GdkEventAny*);
  void buttonAction(int);
};

class MessageBox : public MessageBoxBase
{
public:
  MessageBox(Gtk::Window *, const char *title, int askDontShow, ...);
  ~MessageBox();
};

class Ask2Box : public MessageBoxBase
{
public:
  Ask2Box(Gtk::Window *, const char *title, int askDontShow,
	  int defaultButton, ...);
  ~Ask2Box();
};

class Ask3Box : public MessageBoxBase
{
public:
  Ask3Box(Gtk::Window *, const char *title, int askDontShow,
	  int defaultButton, ...);
  ~Ask3Box();
};

#endif

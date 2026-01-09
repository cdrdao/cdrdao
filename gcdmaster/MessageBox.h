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

class ErrorBox : public Gtk::MessageDialog
{
 public:
    ErrorBox(const char* msg);
};

#endif

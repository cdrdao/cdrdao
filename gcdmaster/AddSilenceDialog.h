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

#ifndef __ADD_SILENCE_DIALOG_H__
#define __ADD_SILENCE_DIALOG_H__

#include <gtkmm.h>
#include <gtk/gtk.h>

class TocEditView;

class AddSilenceDialog : public Gtk::Dialog
{
public:
  enum Mode { M_APPEND, M_INSERT };

  AddSilenceDialog();
  ~AddSilenceDialog();

  void start(TocEditView *);
  void stop();

  void mode(Mode);
  void update(unsigned long level, TocEditView *);
  sigc::signal1<void, unsigned long> signal_tocModified;
  sigc::signal0<void> signal_fullView;

  bool on_delete_event(GdkEventAny*);

private:
  TocEditView *tocEditView_;
  bool active_;
  Mode mode_;

  Gtk::Button *applyButton_;

  Gtk::Entry minutes_;
  Gtk::Entry seconds_;
  Gtk::Entry frames_;
  Gtk::Entry samples_;

  void clearAction();
  void closeAction();
  void applyAction();
};

#endif

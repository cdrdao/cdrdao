/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: AddSilenceDialog.h,v $
 * Revision 1.2  2000/02/20 23:34:53  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:46  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/01/30 15:11:13  mueller
 * Initial revision
 *
 */

#ifndef __ADD_SILENCE_DIALOG_H__
#define __ADD_SILENCE_DIALOG_H__

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;

class AddSilenceDialog : public Gtk::Dialog {
public:
  enum Mode { M_APPEND, M_INSERT };

  AddSilenceDialog();
  ~AddSilenceDialog();

  void start(TocEdit *);
  void stop();

  void mode(Mode);
  void update(unsigned long level, TocEdit *);

  gint delete_event_impl(GdkEventAny*);

private:
  TocEdit *tocEdit_;
  int active_;
  Mode mode_;

  Gtk::Button *applyButton_;

  Gtk::Entry *minutes_;
  Gtk::Entry *seconds_;
  Gtk::Entry *frames_;
  Gtk::Entry *samples_;

  void clearAction();
  void cancelAction();
  void applyAction();
    
};

#endif

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
 * $Log: AddFileDialog.h,v $
 * Revision 1.1  2000/02/05 01:38:46  llanero
 * Initial revision
 *
 * Revision 1.1  1999/09/07 11:17:32  mueller
 * Initial revision
 *
 */

#ifndef __ADD_FILE_DIALOG_H__
#define __ADD_FILE_DIALOG_H__

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;

class AddFileDialog : public Gtk_FileSelection {
public:
  enum Mode { M_APPEND_TRACK, M_APPEND_FILE, M_INSERT_FILE };

  AddFileDialog();
  ~AddFileDialog();

  void start(TocEdit *);
  void stop();

  void mode(Mode);
  void update(unsigned long level, TocEdit *);

  gint delete_event_impl(GdkEventAny*);

private:
  TocEdit *tocEdit_;
  int active_;
  Mode mode_;

  void cancelAction();
  void applyAction();
    
};

#endif

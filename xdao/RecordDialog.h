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
 * $Log: RecordDialog.h,v $
 * Revision 1.1  2000/02/05 01:38:46  llanero
 * Initial revision
 *
 * Revision 1.1  1999/09/07 11:16:16  mueller
 * Initial revision
 *
 */

#ifndef __RECORD_DIALOG_H
#define __RECORD_DIALOG_H

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;

class RecordDialog : public Gtk_Dialog {
public:
  RecordDialog();
  ~RecordDialog();

  void start(TocEdit *);
  void stop();

  void update(unsigned long level, TocEdit *);

  gint delete_event_impl(GdkEventAny*);

private:
  TocEdit *tocEdit_;
  int active_;

  int speed_;

  struct SpeedTable {
    int speed;
    const char *name;
  };

  struct DeviceData {
    int bus, id, lun;
  };

  Gtk_CList *list_;
  Gtk_Button *startButton_;

  Gtk_RadioButton *writeButton_;
  Gtk_RadioButton *simulateButton_;
  
  Gtk_CheckButton *closeSessionButton_;
  Gtk_CheckButton *ejectButton_;
  Gtk_CheckButton *reloadButton_;

  Gtk_ItemFactory_Menu *speedMenuFactory_;
  Gtk_OptionMenu *speedMenu_;

  void cancelAction();
  void startAction();

  void setSpeed(int);

  void appendTableEntry(CdDevice *);
  void import();
  void importStatus();

};

#endif

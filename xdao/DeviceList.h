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

#ifndef __DEVICE_LIST_H
#define __DEVICE_LIST_H

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;

class DeviceList : public Gtk::Frame {
public:
  DeviceList();
  ~DeviceList();

  Gtk::CList_Helpers::SelectionList selection();

//  Gtk::Window *parent; // the dialog where the vbox is placed

  struct DeviceData {
    int bus, id, lun;
  };

private:
  TocEdit *tocEdit_;

  int speed_;

  Gtk::CList *list_;

public:

  void appendTableEntry(CdDevice *);
  void import();
  void importStatus();

};

#endif

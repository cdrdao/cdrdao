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
 * $Log: DeviceConfDialog.h,v $
 * Revision 1.1.1.1  2000/02/05 01:38:46  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/09/06 09:09:37  mueller
 * Initial revision
 *
 */

#ifndef __DEVICE_CONF_DIALOG_H
#define __DEVICE_CONF_DIALOG_H

#include <gtk--.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;

class DeviceConfDialog : public Gtk_Dialog {
public:
  DeviceConfDialog();
  ~DeviceConfDialog();

  void start(TocEdit *);
  void stop();

  void update(unsigned long level, TocEdit *);

  gint delete_event_impl(GdkEventAny*);

private:
  int active_;

  int selectedRow_;

  struct DeviceData {
    int bus, id, lun;
    int driverId;
    int deviceType;
    unsigned long options;
    string specialDevice;
  };

  Gtk_CList *list_;
  Gtk_Button *applyButton_;

  Gtk_ItemFactory_Menu *driverMenuFactory_;
  Gtk_OptionMenu *driverMenu_;

  Gtk_ItemFactory_Menu *devtypeMenuFactory_;
  Gtk_OptionMenu *devtypeMenu_;

  Gtk_SpinButton *busEntry_, *idEntry_, *lunEntry_;
  Gtk_Entry *vendorEntry_, *productEntry_;
  Gtk_Entry *specialDeviceEntry_;
  Gtk_Entry *driverOptionsEntry_;

  const char *checkString(const string &str);

  void setDriverId(int);
  void setDeviceType(int);

  void selectRow(gint, gint, GdkEvent *);
  void unselectRow(gint, gint, GdkEvent *);

  void cancelAction();
  void resetAction();
  void applyAction();
  void addDeviceAction();
  void deleteDeviceAction();
  void rescanAction();

  void appendTableEntry(CdDevice *);
  void import();
  void importConfiguration(int);
  void importStatus();

  void exportData();
  void exportConfiguration(int);
};

#endif

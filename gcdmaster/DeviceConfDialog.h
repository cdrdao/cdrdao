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

#ifndef __DEVICE_CONF_DIALOG_H
#define __DEVICE_CONF_DIALOG_H

#include <gtkmm.h>

class TocEdit;
class CdDevice;

class DeviceConfDialog : public Gtk::Window
{
public:
  DeviceConfDialog();
  ~DeviceConfDialog();

  void start();
  void stop();

  void update(unsigned long level);

private:
  bool active_;

  Gtk::TreeIter selectedRow_;

  struct DeviceData {
    std::string dev;
    int driverId;
    int deviceType;
    unsigned long options;
  };

  // ------------------------------------- Device TreeView

  class ListColumns : public Gtk::TreeModel::ColumnRecord {
  public:
    ListColumns() {
      add(dev); add(vendor); add(model); add(status); add(data);
    };

    Gtk::TreeModelColumn<std::string> dev;
    Gtk::TreeModelColumn<std::string> vendor;
    Gtk::TreeModelColumn<std::string> model;
    Gtk::TreeModelColumn<std::string> status;
    Gtk::TreeModelColumn<DeviceData*> data;
  };
  Gtk::TreeView list_;
  Glib::RefPtr<Gtk::ListStore> listModel_;
  ListColumns listColumns_;

  Gtk::Frame listFrame_;
  Gtk::Frame settingFrame_;
  Gtk::Frame addDeviceFrame_;

  Gtk::OptionMenu *driverMenu_;
  Gtk::OptionMenu *devtypeMenu_;

  Gtk::Entry devEntry_;
  Gtk::Entry vendorEntry_;
  Gtk::Entry productEntry_;
  Gtk::Entry driverOptionsEntry_;

  const char *checkString(const std::string &str);

  void setDriverId(int);
  void setDeviceType(int);

  void selectionChanged();

  void closeAction();
  void resetAction();
  void applyAction();
  void addDeviceAction();
  void deleteDeviceAction();
  void rescanAction();

  Gtk::TreeIter appendTableEntry(CdDevice *);
  void import();
  void importConfiguration(Gtk::TreeIter);
  void importStatus();

  void exportData();
  void exportConfiguration(Gtk::TreeIter);
};

#endif

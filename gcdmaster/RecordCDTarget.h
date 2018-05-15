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

#ifndef __RECORD_CD_TARGET_H
#define __RECORD_CD_TARGET_H

#include <gtkmm.h>
#include <gtk/gtk.h>

class TocEdit;
class CdDevice;
class DeviceList;

class RecordCDTarget : public Gtk::VBox {
public:
  RecordCDTarget(Gtk::Window *);
  ~RecordCDTarget();
  
  void start();
  void stop();

  void update(unsigned long level);

  DeviceList *getDeviceList() { return DEVICES;}
  int getMultisession();
  int getCopies();
  int getSpeed();
  bool getEject();
  bool getOverburn();
  int checkEjectWarning(Gtk::Window *);
  bool getReload();
  int checkReloadWarning(Gtk::Window *);
  int getBuffer();

  void cancelAction();
private:
  int active_;

  DeviceList *DEVICES;

  int speed_;

  Gtk::Window *parent_;
  Gtk::MessageDialog *moreOptionsDialog_;

  Gtk::CheckButton *closeSessionButton_;
  Gtk::CheckButton *ejectButton_;
  Gtk::CheckButton *reloadButton_;
  Gtk::CheckButton *overburnButton_;

  Gtk::SpinButton *copiesSpinButton_;
  Gtk::SpinButton *speedSpinButton_;
  Gtk::CheckButton *speedButton_;

  Gtk::SpinButton *bufferSpinButton_;
  Gtk::Label *bufferRAMLabel_;

  void updateBufferRAMLabel();

  void moreOptions();

  void speedButtonChanged();
  void speedChanged();
};
#endif

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

#ifndef __BLANK_CD_DIALOG_H
#define __BLANK_CD_DIALOG_H

#include <libgnomeuimm.h>

class Project;

class DeviceList;

class BlankCDDialog : public Gtk::Window {
public:
  BlankCDDialog();

  void start(Gtk::Window& parent);
  void update(unsigned long level);

private:
  DeviceList *Devices;
  Gtk::Window* parent_;

  bool active_;
  int speed_;

  Gtk::RadioButton *fastBlank_rb;
  Gtk::RadioButton *fullBlank_rb;
  Gtk::MessageDialog *moreOptionsDialog_;
  Gtk::CheckButton *ejectButton_;
  Gtk::CheckButton *reloadButton_;

  Gtk::SpinButton *speedSpinButton_;
  Gtk::CheckButton *speedButton_;

  void stop();
  void startAction();
  void moreOptions();
  void speedButtonChanged();
  void speedChanged();
  bool getEject();
  int checkEjectWarning(Gtk::Window *);
  bool getReload();
  int checkReloadWarning(Gtk::Window *);
  int getSpeed();

  bool on_delete_event(GdkEventAny*);
};

#endif

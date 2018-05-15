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

#ifndef __RECORD_CD_SOURCE_H
#define __RECORD_CD_SOURCE_H

#include <libgnomeuimm.h>

class CdDevice;
class DeviceList;

class RecordCDSource : public Gtk::VBox {
public:
  RecordCDSource(Gtk::Window *);
  ~RecordCDSource();

  void start();
  void stop();

  void update(unsigned long level);

  bool getOnTheFly();
  void setOnTheFly(bool);
  int getCorrection();
  int getSubChanReadMode();
  DeviceList *getDeviceList() { return DEVICES;}
  void onTheFlyOption(bool);

  struct CorrectionTable {
    int correction;
    const char *name;
  };

  struct SubChanReadModeTable {
    int mode;
    const char *name;
  };

private:
  DeviceList *DEVICES;

  int active_;
  int correction_;
  int speed_;
  int subChanReadMode_;

//  bool onTheFly_;

  Gtk::Window *parent_;
  Gtk::MessageDialog *moreOptionsDialog_;

  Gtk::SpinButton *speedSpinButton_;
  Gtk::CheckButton *speedButton_;

  Gtk::OptionMenu *correctionMenu_;
  Gtk::OptionMenu *subChanReadModeMenu_;
  Gtk::CheckButton *onTheFlyButton_;
  Gtk::CheckButton *continueOnErrorButton_;
  Gtk::CheckButton *ignoreIncorrectTOCButton_;

  void moreOptions();

  void setSpeed(int);
  void setCorrection(int);
  void setSubChanReadMode(int);
  void speedButtonChanged();
  void speedChanged();
};

#endif

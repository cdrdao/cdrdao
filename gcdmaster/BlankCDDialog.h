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

#include <gtkmm.h>
#include "MessageBox.h"
#include "gcdmaster.h"

class DeviceList;

class BlankCDDialog : public GCDWindow
{
  public:
    virtual ~BlankCDDialog() {}

    static BlankCDDialog* create(Glib::RefPtr<Gtk::Builder> builder,
				 Gtk::Window& parent);

    void start();
    virtual void update(unsigned long level) override;

  private:
    BlankCDDialog();

    // UI Components
    DeviceList *Devices;
    Gtk::Box vbox_{Gtk::Orientation::VERTICAL, 10};

    bool active_ = false;
    int speed_ = 1;

    // Radio buttons are now CheckButtons in gtkmm4
    Gtk::CheckButton *fastBlank_rb;
    Gtk::CheckButton *fullBlank_rb;
    
    Gtk::MessageDialog *moreOptionsDialog_ = nullptr;
    Gtk::CheckButton *ejectButton_ = nullptr;
    Gtk::CheckButton *reloadButton_ = nullptr;

    Gtk::SpinButton *speedSpinButton_ = nullptr;
    Gtk::CheckButton *speedButton_ = nullptr;

    Glib::RefPtr<MessageBox> ejectWarningDialog;
    Glib::RefPtr<MessageBox> reloadWarningDialog;

    // Internal Logic Methods
    void stop();
    void startAction();
    void moreOptions(Gtk::Box* box, Gtk::Button* button);
    void speedButtonChanged();
    void speedChanged();
    
    // New Asynchronous Warning Chain
    void checkEjectWarningAsync(int result);
    void checkReloadWarningAsync(int result);
    void proceedWithBlanking();

    // Helper getters
    bool getEject();
    bool getReload();
    int getSpeed();

    bool on_close_request() override;
};

#endif

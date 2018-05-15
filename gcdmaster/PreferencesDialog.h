/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2007  Denis Leroy
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

#ifndef __PREFERENCES_DIALOG_H
#define __PREFERENCES_DIALOG_H

#include <gtkmm.h>
#include <libglademm.h>
#include <gconfmm.h>

class PreferencesDialog : public Gtk::Dialog
{
 public:
    PreferencesDialog(BaseObjectType* cobject,
		      const Glib::RefPtr<Gnome::Glade::Xml>&);
    virtual ~PreferencesDialog();

    void show();

 protected:
    void readFromGConf();
    bool saveToGConf();
    void on_button_apply();
    void on_button_cancel();
    void on_button_ok();
    void on_button_browse();
    void on_button_browse_cancel();
    void on_button_browse_open();

    Glib::RefPtr<Gnome::Glade::Xml> m_refGlade;
    Glib::RefPtr<Gnome::Conf::Client> m_refClient;

    Gtk::Button* _applyButton;
    Gtk::Button* _cancelButton;
    Gtk::Button* _okButton;
    Gtk::Entry*  _tempDirEntry;
    Gtk::FileChooserDialog* _tempDirDialog;
    Gtk::Button* _browseButton;
    Gtk::Button* _browseCancel;
    Gtk::Button* _browseOpen;
};

#endif

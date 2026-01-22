/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __GCDMASTER_H__
#define __GCDMASTER_H__

#include <gtk/gtk.h>
#include <gtkmm.h>

#include <list>

class ProjectChooser;
class BlankCDDialog;
class BlankCDDialog;
class TocEdit;
class PreferencesDialog;

// This is the "top-level" application, acting as a singleton

class GCDWindow : public Gtk::ApplicationWindow
{
public:
    GCDWindow();

    void update(unsigned long level);
};

class GCDMaster : public Gtk::Application
{
  public:
    GCDMaster();

    void openNewProject(const Glib::ustring path);
    void newEmptyProject();
    void update(unsigned long level);

  private:
    Glib::RefPtr<Gtk::Builder> builder_;

    void on_startup() override;
    void on_activate() override;
    void on_action_quit();
    void on_action_preferences();
    void on_action_about();
    void on_action_blank_cdrw();
    void on_action_open();
    void on_action_open_finish(const Glib::RefPtr<Gio::AsyncResult>&);
    void on_open(const type_vec_files& files, const Glib::ustring& hint) override;

    // Windows
    Glib::RefPtr<PreferencesDialog> preferences_;
    Glib::RefPtr<Gtk::AboutDialog> about_;
    Glib::RefPtr<BlankCDDialog> blankCDDialog_;
    Glib::RefPtr<Gtk::FileDialog> openFileChooser_;
    Glib::RefPtr<Gtk::FileFilter> openFilter_;
    Glib::RefPtr<Gtk::FileFilter> allFilter_;

    int argc;
    char* argv[];
};

#endif

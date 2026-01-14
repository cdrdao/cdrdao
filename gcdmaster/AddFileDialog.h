/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __ADD_FILE_DIALOG_H__
#define __ADD_FILE_DIALOG_H__

#include <gtkmm.h>

class AudioCDProject;

class AddFileDialog
{
  public:
    enum Mode {
        M_APPEND_TRACK,
        M_APPEND_FILE,
        M_INSERT_FILE
    };

    AddFileDialog(AudioCDProject *);

    void start();
    void mode(Mode);

  private:
    void on_file_dialog_finish(const Glib::RefPtr<Gio::AsyncResult>& result);
    bool applyAction(const std::vector<Glib::RefPtr<Gio::File>>& files);

    AudioCDProject *project_;
    Mode mode_;
    Glib::RefPtr<Gtk::FileDialog> file_dialog_;
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> filters_;
};

#endif

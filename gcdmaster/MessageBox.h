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

#ifndef __MESSAGE_BOX_H__
#define __MESSAGE_BOX_H__

#include <gtkmm.h>
#include <iostream>

class MessageBox
{
  public:
    virtual ~MessageBox() {}

    static Glib::RefPtr<MessageBox>
    create(Gtk::Window& p, const Glib::ustring title,
                        const std::vector<Glib::ustring> buttons,
                        int defaultButton, int cancelButton,
                        const std::vector<Glib::ustring> extra_lines = {});

    static void message(Gtk::Window& p, const Glib::ustring title,
                        const std::vector<Glib::ustring> extra_lines = {});

    virtual void choose();
    sigc::signal<void(int)> dialogDone;

  protected:
    Glib::RefPtr<Gtk::AlertDialog> dialog;
    Gtk::Window& parentWindow;

    struct PtrHolder {
        PtrHolder(Glib::RefPtr<MessageBox> b) : box(b) {}
        Glib::RefPtr<MessageBox> box;
    }* holder = NULL;

private:
    MessageBox(Gtk::Window&);

    void init(const Glib::ustring title,
              const std::vector<Glib::ustring> buttons,
              int defaultButton, int cancelButton,
              const std::vector<Glib::ustring> extra_lines = {});

};

class Ask2Box
{
  public:
    static Glib::RefPtr<MessageBox>
    create(Gtk::Window& p, const Glib::ustring title, int defaultButton,
           const std::vector<Glib::ustring> extra_lines = {});
};

class Ask3Box
{
  public:
    static Glib::RefPtr<MessageBox>
    create(Gtk::Window& p, const Glib::ustring title, int defaultButton,
           const std::vector<Glib::ustring> extra_lines = {});

    static void message(Gtk::Window& p, const Glib::ustring title,
                        int defaultButton,
                        const std::vector<Glib::ustring> extra_lines = {});
};

class ErrorBox
{
  public:
    static Glib::RefPtr<MessageBox>
    create(Gtk::Window& p, const Glib::ustring title,
           const std::vector<Glib::ustring> extra_lines = {});

    static void message(Gtk::Window& p, const Glib::ustring msg,
                        const std::vector<Glib::ustring> extra_lines = {});
};

#endif

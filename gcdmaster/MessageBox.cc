/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <stdarg.h>
#include <stddef.h>
#include <gtkmm.h>

#include "MessageBox.h"

MessageBox::MessageBox(Gtk::Window& p)
    : parentWindow(p)
{
    dialog = Gtk::AlertDialog::create();
}

void MessageBox::init(const Glib::ustring title,
                          const std::vector<Glib::ustring> buttons,
                          int defaultButton, int cancelButton,
                          const std::vector<Glib::ustring> extra_lines)
{
    dialog->set_message(title);
    dialog->set_modal();
    dialog->set_buttons(buttons);
    if (defaultButton >= 0)
        dialog->set_default_button(defaultButton);
    if (cancelButton >= 0)
        dialog->set_cancel_button(cancelButton);
    if (extra_lines.size() > 0) {
        Glib::ustring details;
        for (auto el : extra_lines) {
            details += el;
            details += "\n";
        }
        dialog->set_detail(details);
    }
}

void MessageBox::choose()
{
    dialog->choose(parentWindow,
                   sigc::mem_fun(*this, &MessageBox::dialog_finish));
}

void MessageBox::dialog_finish(const Glib::RefPtr<Gio::AsyncResult>& result)
{
    dialogDone.emit(dialog->choose_finish(result));
}

Glib::RefPtr<MessageBox>
MessageBox::create(Gtk::Window& p,const Glib::ustring title,
                   const std::vector<Glib::ustring> extra_lines)
{
    Glib::RefPtr<MessageBox> mb(new MessageBox(p));
    mb->init(title, { "Ok" }, 1, -1, extra_lines);
    return mb;
}

Glib::RefPtr<MessageBox>
Ask2Box::create(Gtk::Window& p, const Glib::ustring title, int defaultButton,
                const std::vector<Glib::ustring> extra_lines)
{
    Glib::RefPtr<MessageBox> mb(new MessageBox(p));
    mb->init(title, { "Yes", "No" }, defaultButton, -1, extra_lines);
    return mb;
}

Glib::RefPtr<MessageBox>
Ask3Box::create(Gtk::Window& p, const Glib::ustring title, int defaultButton,
                const std::vector<Glib::ustring> extra_lines)
{
    Glib::RefPtr<MessageBox> mb(new MessageBox(p));
    mb->init(title, { "Yes", "No", "Cancel" }, defaultButton, 2, extra_lines);
    return mb;
}

Glib::RefPtr<MessageBox>
ErrorBox::create(Gtk::Window& p, const Glib::ustring message,
                 const std::vector<Glib::ustring> extra_lines)
{
    Glib::RefPtr<MessageBox> mb(new MessageBox(p));
    mb->init("Error: " + message, { "Ok" }, 0, -1, extra_lines);
    return mb;
}

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

#ifndef __VIEW_SWITCHER_H__
#define __VIEW_SWITCHER_H__

#include <gtk--.h>
#include <gtk/gtk.h>
#include <gnome--.h>

#include <list>

class ViewSwitcher : public Gtk::Toolbar
{
private:
  Gtk::HBox *hbox_;

  std::list<Gtk::Widget *> *visible;

  void setView(std::list<Gtk::Widget *> *);
  Gtk::RadioButton::Group group_;

public:
  ViewSwitcher(Gtk::HBox *);

  void addView(std::list<Gtk::Widget *> *, Gnome::Pixmap *, Gtk::Label *);
};
#endif


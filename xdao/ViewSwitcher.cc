/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
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

#include <gnome.h>

#include "ViewSwitcher.h"

ViewSwitcher::ViewSwitcher(Gtk::HBox *hbox)
{
  set_button_relief(GTK_RELIEF_NONE);
  set_style(GTK_TOOLBAR_ICONS);
  set_border_width(2);
  hbox_ = hbox;
  visible = NULL;
}

void ViewSwitcher::addView(std::list<Gtk::Widget *> *widgets,
			   Gnome::Pixmap *pixmap, Gtk::Label *label)
{
  Gtk::HBox *hbox = new Gtk::HBox;

  hbox->pack_start(*pixmap, FALSE, FALSE);
  hbox->pack_start(*label, FALSE, FALSE);
  hbox->set_spacing(4);
  hbox->show_all();

  Gtk::Toolbar_Helpers::RadioElem *switchButton = new Gtk::Toolbar_Helpers::RadioElem
    (group_, *hbox, bind(slot(this, &ViewSwitcher::setView), widgets),
    "click to select this view");

  tools().push_back(*switchButton);

  if (!visible)
  {
    for (std::list<Gtk::Widget *>::iterator i = widgets->begin(); i != widgets->end(); i++)
      { (*i)->show(); }
    visible = widgets;
  }
}

void ViewSwitcher::setView(std::list<Gtk::Widget *> *widgets)
{
  if (!(widgets->front()->is_visible()) && (visible != widgets))
  {
    for (std::list<Gtk::Widget *>::iterator i = visible->begin(); i != visible->end(); i++)
      { (*i)->hide(); }
    for (std::list<Gtk::Widget *>::iterator i = widgets->begin(); i != widgets->end(); i++)
      { (*i)->show(); }
    visible = widgets;
  }
}

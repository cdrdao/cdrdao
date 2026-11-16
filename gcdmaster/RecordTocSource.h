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

#ifndef __RECORD_TOC_SOURCE_H
#define __RECORD_TOC_SOURCE_H

#include <gtk/gtk.h>
#include <gtkmm.h>
#include <glibmm/i18n.h>

class TocEdit;

class RecordTocSource : public Gtk::Box
{
  public:
    RecordTocSource(Glib::RefPtr<TocEdit>);

    void update(unsigned long level);
    void set(Glib::RefPtr<TocEdit>);
    
  private:
    Glib::RefPtr<TocEdit> tocEdit_;
    bool active_ = false;

    Gtk::Label label1{_("Project name: "), Gtk::Align::END};
    Gtk::Label projectLabel_{"", Gtk::Align::START};
    Gtk::Label label2{_("Toc Type: "), Gtk::Align::END};
    Gtk::Label tocTypeLabel_{"", Gtk::Align::START};
    Gtk::Label label3{_("Tracks: "), Gtk::Align::END};
    Gtk::Label nofTracksLabel_{"", Gtk::Align::START};
    Gtk::Label label4{_("Length: "), Gtk::Align::END};
    Gtk::Label tocLengthLabel_{"", Gtk::Align::START};
};

#endif

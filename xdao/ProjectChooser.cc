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

#include <gtkmm.h>
#include <gnome.h>

#include "xcdrdao.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "ProjectChooser.h"
#include "Icons.h"

#define ICON_PADDING 10
#define LABEL_PADDING 10
#define BUTTONS_RELIEF Gtk::RELIEF_NORMAL


ProjectChooser::ProjectChooser()
{
  set_title(APP_NAME);

  vbox.set_border_width(40);

  Gtk::HBox *hbox;
  Gtk::Label *label;

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*(manage(new Gtk::Image(Icons::OPEN,
                                           Gtk::ICON_SIZE_DIALOG))),
                   false, false, ICON_PADDING);
  label = manage(new Gtk::Label(_("Open existing project")));
  hbox->pack_start(*label, false, false, LABEL_PADDING);
  openButton.add(*hbox);
  vbox.pack_start(openButton);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*(manage(new Gtk::Image(Icons::AUDIOCD,
                                           Gtk::ICON_SIZE_DIALOG))),
                   false, false, ICON_PADDING);
  label = manage(new Gtk::Label(_("New Audio CD project")));
  hbox->pack_start(*label, false, false, LABEL_PADDING);
  audioCDButton.add(*hbox);
  vbox.pack_start(audioCDButton);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*(manage(new Gtk::Image(Icons::COPYCD,
                                           Gtk::ICON_SIZE_DIALOG))),
                   false, false, ICON_PADDING);
  label = manage(new Gtk::Label(_("Duplicate CD")));
  hbox->pack_start(*label, false, false, LABEL_PADDING);
  copyCDButton.add(*hbox);
  vbox.pack_start(copyCDButton);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*(manage(new Gtk::Image(Icons::DUMPCD,
                                           Gtk::ICON_SIZE_DIALOG))),
                   false, false, ICON_PADDING);
  label = manage(new Gtk::Label(_("Copy CD to disk")));
  hbox->pack_start(*label, false, false, LABEL_PADDING);
  dumpCDButton.add(*hbox);
  vbox.pack_start(dumpCDButton);
  add(vbox);

  // Connect button signals
  openButton.signal_clicked().
    connect(bind(slot(*gcdmaster, &GCDMaster::openProject), this));
  audioCDButton.signal_clicked().
    connect(bind(slot(*gcdmaster, &GCDMaster::newAudioCDProject2), this));
  copyCDButton.signal_clicked().
    connect(bind(slot(*gcdmaster, &GCDMaster::newDuplicateCDProject), this));
  dumpCDButton.signal_clicked().
    connect(bind(slot(*gcdmaster, &GCDMaster::newDumpCDProject), this));

  vbox.show_all();
}


bool ProjectChooser::on_delete_event(GdkEventAny* e)
{
  gcdmaster->closeChooser(this);
  return true;
}


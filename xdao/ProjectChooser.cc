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

#include "xcdrdao.h"
#include "gcdmaster.h"
#include "Project.h"
#include "ProjectChooser.h"

#define ICON_PADDING 10
#define LABEL_PADDING 10
#define BUTTONS_RELIEF GTK_RELIEF_NORMAL


ProjectChooser::ProjectChooser(Project *project) 
{
//  static const GtkTargetEntry drop_types [] =
//  {
//    { "text/uri-list", 0, TARGET_URI_LIST }
//  };

//  static gint n_drop_types = sizeof (drop_types) / sizeof(drop_types[0]);

//  drag_dest_set(static_cast <GtkDestDefaults> (GTK_DEST_DEFAULT_MOTION
//    | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP),
//    &drop_types[0], n_drop_types, GDK_ACTION_COPY);
  
//  drag_data_received.connect(slot(this, &AudioCDView::drag_data_received_cb));

//  Gtk::Table *table = manage(new Gtk::Table(7, 3, FALSE));
  Gtk::Table *table = new Gtk::Table(7, 3, FALSE);
  Gtk::HBox *hbox;
  Gnome::Pixmap *pixmap;
  Gtk::Label *label;
  Gdk_Font font;

  project_ = project;
  
//  table->set_col_spacings(20);
  table->set_border_width(40);
  
  Gtk::Button *openButton = manage(new Gtk::Button());
  openButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_open.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  // NOTE: Extra spaces are just to make it nicer.
  label = manage(new Gtk::Label("Open existing project                      "));
//FIXME  font = label->get_font();
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  openButton->add(*hbox);
  openButton->show();
  openButton->clicked.connect(bind(slot(gcdmaster, &GCDMaster::openProject), project));
  table->attach(*openButton, 1, 2, 0, 1);
//  pack_start(*openButton, FALSE, TRUE);

  Gtk::Button *audioCDButton = manage(new Gtk::Button());
  audioCDButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_audiocd.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("New Audio CD project"));
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  audioCDButton->add(*hbox);
  audioCDButton->show();
  audioCDButton->clicked.connect(bind(slot(project_, &Project::newAudioCDProject), ""));
  table->attach(*audioCDButton, 1, 2, 1, 2, GTK_FILL);
//  pack_start(*audioCDButton, FALSE, TRUE);

  Gtk::Button *dataCDButton = manage(new Gtk::Button());
  dataCDButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_datacd.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("New Data CD project"));
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  dataCDButton->add(*hbox);
  dataCDButton->show();
dataCDButton->set_sensitive(false);
  table->attach(*dataCDButton, 1, 2, 2, 3, GTK_FILL);
//  pack_start(*dataCDButton, FALSE, TRUE);

  Gtk::Button *mixedCDButton = manage(new Gtk::Button());
  mixedCDButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_mixedcd.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("New Mixed CD project"));
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  mixedCDButton->add(*hbox);
  mixedCDButton->show();
mixedCDButton->set_sensitive(false);
  table->attach(*mixedCDButton, 1, 2, 3, 4, GTK_FILL);
//  pack_start(*mixedCDButton, TRUE, TRUE);

  Gtk::Button *copyCDButton = manage(new Gtk::Button());
  copyCDButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_copycd.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("Duplicate CD"));
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  copyCDButton->add(*hbox);
  copyCDButton->show();
  copyCDButton->clicked.connect(slot(gcdmaster, &GCDMaster::recordCD2CD));
  table->attach(*copyCDButton, 1, 2, 4, 5, GTK_FILL);
//  pack_start(*copyCDButton, TRUE, TRUE);

  Gtk::Button *dumpCDButton = manage(new Gtk::Button());
  dumpCDButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_dumpcd.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("Copy CD to disk"));
//  label->set_alignment(0, 0.5);
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  dumpCDButton->add(*hbox);
  dumpCDButton->show();
  dumpCDButton->clicked.connect(slot(gcdmaster, &GCDMaster::recordCD2HD));
  table->attach(*dumpCDButton, 1, 2, 5, 6, GTK_FILL);
//  pack_start(*dumpCDButton, TRUE, TRUE);

  Gtk::Button *helpButton = manage(new Gtk::Button());
  helpButton->set_relief(BUTTONS_RELIEF);
  hbox = manage(new Gtk::HBox);
  pixmap = manage(new Gnome::Pixmap(Gnome::Pixmap::find_file("gcdmaster/pixmap_help.png")));
  pixmap->show();
  hbox->pack_start(*pixmap, FALSE, FALSE, ICON_PADDING);
  label = manage(new Gtk::Label("Help"));
  label->show();
  hbox->pack_start(*label, FALSE, FALSE, LABEL_PADDING);
  hbox->show();
  helpButton->add(*hbox);
  helpButton->show();
helpButton->set_sensitive(false);
  table->attach(*helpButton, 1, 2, 6, 7, GTK_FILL);
//  pack_start(*helpButton, TRUE, TRUE);

  table->show();
  pack_start(*table, TRUE, TRUE);
}

/*
void AudioCDView::drag_data_received_cb(GdkDragContext *context,
  gint x, gint y, GtkSelectionData *selection_data, guint info, guint time)
{
  GList *names;
  
  switch (info) {
    case TARGET_URI_LIST:
      names = (GList *)gnome_uri_list_extract_filenames \
			((char *)selection_data->data);  

//  	tocEdit_->blockEdit();
//FIXME      while (names->data) {
      if (names->data) {
          string str = g_strdup(static_cast <char *>(names->data));
          const char *file = stripCwd(str.c_str());

        switch (tocEditView_->tocEdit()->appendTrack(file)) {
        case 0:
	      guiUpdate();
	      MDI_WINDOW->statusMessage("Appended track with audio data from \"%s\".", file);
	      break;
        case 1:
	      MDI_WINDOW->statusMessage("Cannot open audio file \"%s\".", file);
	      break;
        case 2:
	      MDI_WINDOW->statusMessage("Audio file \"%s\" has wrong format.", file);
	      break;
	    }
	    names = g_list_remove(names, names->data);
      }
//	tocEdit_->unblockEdit();
    break;
  }
}
*/

ProjectChooser::~ProjectChooser() 
{
}

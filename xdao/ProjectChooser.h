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

#ifndef __PROJECT_CHOOSER_H__
#define __PROJECT_CHOOSER_H__

#include <gtk--.h>

class GCDMaster;

class ProjectChooser : public Gtk::Window
{
public:
  ProjectChooser();

  virtual int delete_event_impl(GdkEventAny *event);

private:

// FIXME: When we receive a file:
//		if it is wav, mp3, ... open an AudioCD project
//		else open a DataCD.
//  void drag_data_received_cb(GdkDragContext *context, gint x, gint y,
//         GtkSelectionData *selection_data, guint info, guint time);

};
#endif


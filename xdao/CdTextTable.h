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
/*
 * $Log: CdTextTable.h,v $
 * Revision 1.1  2000/04/23 09:02:04  andreasm
 * Table entry dialog for CD-TEXT title and performer data.
 *
 */

#ifndef __CD_TEXT_TABLE_H__
#define __CD_TEXT_TABLE_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include "Toc.h"
#include "CdTextItem.h"

class TocEdit;

class CdTextTable : public Gtk::Dialog {
public:
  CdTextTable(TocEdit *, int language);
  ~CdTextTable();

  gint delete_event_impl(GdkEventAny*);

  int run();

private:
  TocEdit *tocEdit_;
  int language_;

  Gtk::Entry *performer_;
  Gtk::Entry *title_;
  Gtk::CheckButton *performerButton_;

  struct TableEntry {
    Gtk::Entry *performer;
    Gtk::Entry *title;
  };

  Gtk::Adjustment adjust_;

  TableEntry *tracks_;

  int done_;

  void cancelAction();
  void okAction();
  void fillPerformerAction();
  void activatePerformerAction();

  void importData();
  void exportData();
  void setCdTextItem(CdTextItem::PackType, int trackNr, const char *);

  const char *checkString(const string &);
};

#endif

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __CD_TEXT_DIALOG_H__
#define __CD_TEXT_DIALOG_H__

#include <gtkmm.h>
#include <gtk/gtk.h>

//#include "Toc.h"
#include "CdTextItem.h"

class TocEdit;

class CdTextDialog : public Gtk::Dialog
{
public:
  CdTextDialog();
  ~CdTextDialog();

  bool on_delete_event(GdkEventAny*);

  void update(unsigned long, TocEdit *);

  void start(TocEdit *);
  void stop();

private:
  bool active_;

  TocEdit *tocEdit_;
  int trackEntries_;

  Gtk::Button *applyButton_;
  Gtk::Notebook *languages_;

  struct TableEntry {
    Gtk::Entry *performer;
    Gtk::Entry *title;
    Gtk::Label *label;
    Gtk::HBox *hbox;
  };

  struct Language {
    Gtk::Table *table;
    Gtk::Entry *performer;
    Gtk::Entry *title;
    Gtk::Label *tabLabel;

    Gtk::CheckButton *performerButton;
    
    TableEntry *tracks;
  };

  Language page_[8];

  void adjustTableEntries(int);
  void updateTabLabels();
  void applyAction();
  void fillPerformerAction();
  void activatePerformerAction(int);

  void importData();
  void exportData();
  void setCdTextItem(CdTextItem::PackType, int trackNr, int l, const char *);

  const char *checkString(const std::string &);
};

#endif

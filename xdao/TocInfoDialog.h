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
/*
 * $Log: TocInfoDialog.h,v $
 * Revision 1.2  2000/02/20 23:34:54  llanero
 * fixed scsilib directory (files mising ?-()
 * ported xdao to 1.1.8 / gnome (MDI) app
 *
 * Revision 1.1.1.1  2000/02/05 01:38:52  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/08/21 14:17:39  mueller
 * Initial revision
 *
 */

#ifndef __TOC_INFO_DIALOG_H__
#define __TOC_INFO_DIALOG_H__

#include <gtk--.h>
#include <gtk/gtk.h>

#include "Toc.h"

class TocEdit;
class TextEdit;

class TocInfoDialog : public Gtk::Dialog {
public:
  TocInfoDialog();
  ~TocInfoDialog();

  gint delete_event_impl(GdkEventAny*);

  void update(unsigned long, TocEdit *);

  void start(TocEdit *);
  void stop();

private:
  TocEdit *tocEdit_;
  int active_;

  Gtk::Button *applyButton_;
  Gtk::Label *tocLength_;
  Gtk::Label *nofTracks_;

  TextEdit *catalog_;

//llanero  Gtk::ItemFactory_Menu *tocTypeMenuFactory_;
//llanero  Gtk::OptionMenu *tocType_;
  Toc::TocType selectedTocType_;

  struct BlockValue {
    int block;
    int value;
  };

  struct CdTextPage {
//llanero    Gtk::ItemFactory_Menu *languageMenuFactory;
    Gtk::OptionMenu *language;
    int selectedLanguage;

//llanero    Gtk::ItemFactory_Menu *genreMenuFactory;
    Gtk::OptionMenu *genre;
    int selectedGenre;
    
    Gtk::Label *label;
    Gtk::Entry *title;
    Gtk::Entry *performer;
    Gtk::Entry *songwriter;
    Gtk::Entry *composer;
    Gtk::Entry *arranger;
    Gtk::Entry *message;
    Gtk::Entry *catalog;
    Gtk::Entry *upcEan;
    Gtk::Entry *genreInfo;
  };

  CdTextPage cdTextPages_[8];

  void cancelAction();
  void applyAction();

  void createCdTextLanguageMenu(int);
  void createCdTextGenreMenu(int n);
  Gtk::VBox *createCdTextPage(int);

  void clear();
  void clearCdText();

  const char *checkString(const string &);
  int getCdTextLanguageIndex(int code);
  int getCdTextGenreIndex(int code1, int code2);

  void importCdText(const Toc *);
  void importData(const Toc *);
  void exportCdText(TocEdit *);
  void exportData(TocEdit *);

  void setSelectedTocType(Toc::TocType);
  void setSelectedCDTextLanguage(BlockValue);
  void setSelectedCDTextGenre(BlockValue);
};

#endif

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
 * Revision 1.1  2000/02/05 01:38:52  llanero
 * Initial revision
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

class TocInfoDialog : public Gtk_Dialog {
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

  Gtk_Button *applyButton_;
  Gtk_Label *tocLength_;
  Gtk_Label *nofTracks_;

  TextEdit *catalog_;

  Gtk_ItemFactory_Menu *tocTypeMenuFactory_;
  Gtk_OptionMenu *tocType_;
  Toc::TocType selectedTocType_;

  struct BlockValue {
    int block;
    int value;
  };

  struct CdTextPage {
    Gtk_ItemFactory_Menu *languageMenuFactory;
    Gtk_OptionMenu *language;
    int selectedLanguage;

    Gtk_ItemFactory_Menu *genreMenuFactory;
    Gtk_OptionMenu *genre;
    int selectedGenre;
    
    Gtk_Label *label;
    Gtk_Entry *title;
    Gtk_Entry *performer;
    Gtk_Entry *songwriter;
    Gtk_Entry *composer;
    Gtk_Entry *arranger;
    Gtk_Entry *message;
    Gtk_Entry *catalog;
    Gtk_Entry *upcEan;
    Gtk_Entry *genreInfo;
  };

  CdTextPage cdTextPages_[8];

  void cancelAction();
  void applyAction();

  void createCdTextLanguageMenu(int);
  void createCdTextGenreMenu(int n);
  Gtk_VBox *createCdTextPage(int);

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

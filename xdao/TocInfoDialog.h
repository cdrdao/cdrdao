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

#ifndef __TOC_INFO_DIALOG_H__
#define __TOC_INFO_DIALOG_H__

#include <gtkmm.h>
#include <gtk/gtk.h>

#include "Toc.h"

class TocEdit;
class TextEdit;

class TocInfoDialog : public Gtk::Dialog
{
public:
  TocInfoDialog(Gtk::Window* parent);
  ~TocInfoDialog();

  bool on_delete_event(GdkEventAny*);

  void update(unsigned long, TocEdit *);

  void start(TocEdit *);
  void stop();

private:
  TocEdit *tocEdit_;
  bool active_;

  Gtk::Button *applyButton_;
  Gtk::Label *tocLength_;
  Gtk::Label *nofTracks_;

  TextEdit *catalog_;

  Gtk::OptionMenu *tocType_;
  Toc::TocType selectedTocType_;

  struct BlockValue {
    int block;
    int value;
  };

  struct CdTextPage {
    Gtk::OptionMenu *language;
    int selectedLanguage;

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

  void closeAction();
  void applyAction();

  void createCdTextLanguageMenu(int);
  void createCdTextGenreMenu(int n);
  Gtk::VBox *createCdTextPage(int);

  void clear();
  void clearCdText();

  const char *checkString(const std::string &);
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

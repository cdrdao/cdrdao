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
#include "Toc.h"

class TocEdit;
class TextEdit;

class TocInfoDialog : public Gtk::Window
{
public:
    TocInfoDialog(Gtk::Window* parent);

    void update(unsigned long);
    void start(TocEdit *);

private:
    TocEdit *tocEdit_;

    Gtk::Button *applyButton_;
    Gtk::Button *imdbButton_;
    Gtk::Label *tocLength_;
    Gtk::Label *nofTracks_;
    TextEdit *catalog_;

    Gtk::DropDown tocType_;
    Toc::Type selectedTocType_;

    struct CdTextPage {
        Gtk::DropDown *language;
        int selectedLanguage;
        Gtk::DropDown *genre;
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

    void applyAction();
    void imdbAction();

    void createCdTextLanguageMenu(int);
    void createCdTextGenreMenu(int n);
    Gtk::Box* createCdTextPage(int);

    void clear();
    void clearCdText();

    const char *checkString(const std::string &);
    int getCdTextLanguageIndex(int code);
    int getCdTextGenreIndex(int code1, int code2);

    void importCdText(const Toc *);
    void importData(const Toc *);
    void exportCdText(TocEdit *);
    void exportData(TocEdit *);

    void setSelectedTocType();
    void setSelectedCDTextLanguage(int);
    void setSelectedCDTextGenre(int);
};

#endif

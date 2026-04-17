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

#include <vector>
#include <gtkmm.h>
#include <adwaitamm.h>
#include "Toc.h"

class TocEdit;

class TocInfoDialog : public Gtk::Window
{
public:
    TocInfoDialog(Gtk::Window* parent);

    void update(unsigned long);
    void start(TocEdit *);

private:
    TocEdit *tocEdit_;
    bool importing_ = false;

    Gtk::Button *applyButton_;
    Gtk::Button *imdbButton_;
    Gtk::Label *tocLength_;
    Gtk::Label *nofTracks_;
    Adw::EntryRow *catalog_;

    Adw::ComboRow *tocType_;
    Toc::Type selectedTocType_;

    struct TrackEntry {
        Adw::ExpanderRow *row;
        Adw::EntryRow *title;
        Adw::EntryRow *performer;
    };

    struct CdTextPage {
        // Outer tab page + tab-label widgets (kept stable across tab
        // insertions/removals — reparented when the tab is shown/hidden).
        Gtk::Widget *pageWidget;
        Gtk::Box *tabWidget;
        Gtk::Label *tabLabel;

        // Per-language controls (above inner notebook)
        Adw::ComboRow *language;
        int selectedLanguage;
        Adw::ComboRow *encoding;
        Gtk::Label *encodingWarning;

        // Disc sub-tab
        Adw::ComboRow *genre;
        int selectedGenre;
        Adw::EntryRow *title;
        Adw::EntryRow *performer;
        Adw::EntryRow *songwriter;
        Adw::EntryRow *composer;
        Adw::EntryRow *arranger;
        Adw::EntryRow *message;
        Adw::EntryRow *catalog;
        Adw::EntryRow *upcEan;
        Adw::EntryRow *genreInfo;

        // Tracks sub-tab
        Adw::PreferencesGroup *tracksGroup;
        Gtk::CheckButton *performerButton;
        std::vector<TrackEntry> tracks;
    };

    CdTextPage cdTextPages_[8];
    int trackEntries_ = 0;

    // Notebook state: which cdTextPages_ slots are currently shown, in
    // tab order. Slots not present are "unused" and their data is ignored.
    Gtk::Notebook *cdTextNotebook_;
    Gtk::Button *addBlockButton_;
    std::vector<int> visibleBlocks_;

    void applyAction();
    void imdbAction();
    void fillPerformerAction(int l);
    void activatePerformerAction(int l);

    void createCdTextLanguageMenu(int);
    void createCdTextGenreMenu(int n);
    Gtk::Widget* createCdTextPage(int);
    Gtk::Widget* createDiscTab(int);
    Gtk::Widget* createTracksTab(int);

    void adjustTableEntries(int n);
    void updateTabLabels();
    void recomputeEncoding(int l);

    // Block add/remove
    void addBlockAction();
    void removeBlock(int slot);
    int findUnusedSlot() const;
    int findUnusedLanguageIndex() const;
    void resetBlockData(int slot);
    void clearBlockInToc(TocEdit *tocEdit, int blockNr);
    void syncTabs();
    void updateAddButtonSensitivity();

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

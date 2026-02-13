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


#include "TocInfoDialog.h"
#include "config.h"
#include <glibmm/i18n.h>
#include "TocEdit.h"
#include "CdTextItem.h"
#include "TextEdit.h"
#include "Toc.h"
#include "guiUpdate.h"
#include "trackdb/log.h"
#include "trackdb/Cddb.h"
#include "MessageBox.h"
#include "PreferencesDialog.h"

#define MAX_CD_TEXT_LANGUAGE_CODES 22

struct LanguageCode {
    int code;
    const char *name;
};

static LanguageCode CD_TEXT_LANGUAGE_CODES[MAX_CD_TEXT_LANGUAGE_CODES] = {
    {-1, "Unknown"},   {-1, "Undefined"},    {0x75, "Chinese"}, {0x06, "Czech"},
    {0x07, "Danish"},  {0x1d, "Dutch"},      {0x09, "English"}, {0x27, "Finnish"},
    {0x0f, "French"},  {0x08, "German"},     {0x70, "Greek"},   {0x1b, "Hungarian"},
    {0x15, "Italian"}, {0x69, "Japanese"},   {0x65, "Korean"},  {0x1e, "Norwegian"},
    {0x20, "Polish"},  {0x21, "Portuguese"}, {0x56, "Russian"}, {0x26, "Slovene"},
    {0x0a, "Spanish"}, {0x28, "Swedish"}};

#define MAX_CD_TEXT_GENRE_CODES 28

struct GenreCode {
    int code1;
    int code2;
    const char *name;
};

static GenreCode CD_TEXT_GENRE_CODES[MAX_CD_TEXT_GENRE_CODES] = {{-1, -1, "Unknown"},
                                                                 {-1, -1, "Not Used"},
                                                                 {0x00, 0x01, "Not Defined"},
                                                                 {0x00, 0x02, "Adult Contemporary"},
                                                                 {0x00, 0x03, "Alternative Rock"},
                                                                 {0x00, 0x04, "Children's Music"},
                                                                 {0x00, 0x05, "Classical"},
                                                                 {0x00, 0x07, "Country"},
                                                                 {0x00, 0x08, "Dance"},
                                                                 {0x00, 0x09, "Easy Listening"},
                                                                 {0x00, 0x0a, "Erotic"},
                                                                 {0x00, 0x0b, "Folk"},
                                                                 {0x00, 0x0c, "Gospel"},
                                                                 {0x00, 0x0d, "Hip Hop"},
                                                                 {0x00, 0x0e, "Jazz"},
                                                                 {0x00, 0x0f, "Latin"},
                                                                 {0x00, 0x10, "Musical"},
                                                                 {0x00, 0x11, "New Age"},
                                                                 {0x00, 0x12, "Opera"},
                                                                 {0x00, 0x13, "Operetta"},
                                                                 {0x00, 0x14, "Pop Music"},
                                                                 {0x00, 0x15, "RAP"},
                                                                 {0x00, 0x16, "Reggae"},
                                                                 {0x00, 0x17, "Rock Music"},
                                                                 {0x00, 0x19, "Sound Effects"},
                                                                 {0x00, 0x1a, "Sound Track"},
                                                                 {0x00, 0x1b, "Spoken Word"},
                                                                 {0x00, 0x1c, "World Music"}};

static const char *TOC_TYPE_CD_DA = "CD-DA";
static const char *TOC_TYPE_CD_ROM = "CD-ROM";
static const char *TOC_TYPE_CD_ROM_XA = "CD-ROM-XA";
static const char *TOC_TYPE_CD_I = "CD-I";

TocInfoDialog::TocInfoDialog(Gtk::Window* parent)
{
    set_title(_("Project Info"));
    set_modal(true);
    set_transient_for(*parent);
    set_hide_on_close(true);

    auto contents = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    contents->set_margin(10);

    tocEdit_ = nullptr;
    selectedTocType_ = Toc::Type::CD_DA;

    nofTracks_ = Gtk::make_managed<Gtk::Label>("99");
    nofTracks_->set_halign(Gtk::Align::START);
    tocLength_ = Gtk::make_managed<Gtk::Label>("100:00:00");
    tocLength_->set_halign(Gtk::Align::START);

    catalog_ = Gtk::make_managed<TextEdit>("1234567890123");
    catalog_->set_max_length(13);

    {
        auto list = Gtk::StringList::create({});
        list->append(TOC_TYPE_CD_DA);
        list->append(TOC_TYPE_CD_ROM);
        list->append(TOC_TYPE_CD_ROM_XA);
        list->append(TOC_TYPE_CD_I);
        tocType_.set_model(list);
        tocType_.property_selected().signal_changed().connect(
            sigc::mem_fun(*this, &TocInfoDialog::setSelectedTocType));
    }

    // Summary frame (Grid replaces Table)
    auto frameSummary = Gtk::make_managed<Gtk::Frame>(_(" Summary "));
    auto gridSummary = Gtk::make_managed<Gtk::Grid>();
    gridSummary->set_row_spacing(5);
    gridSummary->set_column_spacing(5);
    gridSummary->set_margin(5);
    
    gridSummary->attach(*Gtk::make_managed<Gtk::Label>(_("Tracks:")), 0, 0);
    gridSummary->attach(*nofTracks_, 1, 0);
    gridSummary->attach(*Gtk::make_managed<Gtk::Label>(_("Length:")), 0, 1);
    gridSummary->attach(*tocLength_, 1, 1);
    
    frameSummary->set_child(*gridSummary);
    contents->append(*frameSummary);

    // Sub-channel frame
    auto frameSub = Gtk::make_managed<Gtk::Frame>(_(" Sub-Channel "));
    auto gridSub = Gtk::make_managed<Gtk::Grid>();
    gridSub->set_row_spacing(5);
    gridSub->set_column_spacing(5);
    gridSub->set_margin(5);

    gridSub->attach(*Gtk::make_managed<Gtk::Label>(_("Toc Type: ")), 0, 0);
    gridSub->attach(tocType_, 1, 0);
    gridSub->attach(*Gtk::make_managed<Gtk::Label>("UPC/EAN: "), 0, 1);
    gridSub->attach(*catalog_, 1, 1);
    
    frameSub->set_child(*gridSub);
    contents->append(*frameSub);

    // CD-TEXT data
    auto frameText = Gtk::make_managed<Gtk::Frame>(" CD-TEXT ");
    auto notebook = Gtk::make_managed<Gtk::Notebook>();

    for (int i = 0; i < 8; i++) {
        auto vboxPage = createCdTextPage(i);
        notebook->append_page(*vboxPage, *(cdTextPages_[i].label));
    }
    frameText->set_child(*notebook);
    contents->append(*frameText);

    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    bbox->set_margin(10);
    bbox->set_spacing(5);
    bbox->set_halign(Gtk::Align::END);

    imdbButton_ = Gtk::make_managed<Gtk::Button>("Query IMDB");
    imdbButton_->signal_clicked().connect(sigc::mem_fun(*this, &TocInfoDialog::imdbAction));
    bbox->append(*imdbButton_);
    {
        auto cb = Gtk::make_managed<Gtk::Button>("Clear");
        cb->signal_clicked().connect([this](){ this->clear(); });
        bbox->append(*cb);
    }
    applyButton_ = Gtk::make_managed<Gtk::Button>("Apply");
    applyButton_->signal_clicked().connect(sigc::mem_fun(*this, &TocInfoDialog::applyAction));
    bbox->append(*applyButton_);
    auto closeButton = Gtk::make_managed<Gtk::Button>("Close");
    closeButton->signal_clicked().connect(
        sigc::mem_fun(*this, &Gtk::Widget::hide));
    bbox->append(*closeButton);
    contents->append(*bbox);

    // Dialog setup
    set_child(*contents);

    set_default_size(400, -1);
}

Gtk::Box* TocInfoDialog::createCdTextPage(int n)
{
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(5);
    grid->set_margin(5);

    cdTextPages_[n].label = Gtk::make_managed<Gtk::Label>(std::to_string(n));

    // Initialize Entries
    cdTextPages_[n].title = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].performer = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].songwriter = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].composer = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].arranger = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].message = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].catalog = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].upcEan = Gtk::make_managed<Gtk::Entry>();
    cdTextPages_[n].genreInfo = Gtk::make_managed<Gtk::Entry>();

    createCdTextLanguageMenu(n);
    createCdTextGenreMenu(n);

    auto add_row = [&](int row, const std::string& labelText, Gtk::Widget& widget) {
        auto lbl = Gtk::make_managed<Gtk::Label>(labelText);
        lbl->set_halign(Gtk::Align::END);
        grid->attach(*lbl, 0, row);
        grid->attach(widget, 1, row);
        widget.set_hexpand(true);
    };

    add_row(0, _("Language:"), *(cdTextPages_[n].language));
    add_row(1, _("Title:"), *(cdTextPages_[n].title));
    add_row(2, _("Performer:"), *(cdTextPages_[n].performer));
    add_row(3, _("Songwriter:"), *(cdTextPages_[n].songwriter));
    add_row(4, _("Composer:"), *(cdTextPages_[n].composer));
    add_row(5, _("Arranger:"), *(cdTextPages_[n].arranger));
    add_row(6, _("Message:"), *(cdTextPages_[n].message));
    add_row(7, _("Catalog:"), *(cdTextPages_[n].catalog));
    add_row(8, _("UPC/EAN:"), *(cdTextPages_[n].upcEan));
    add_row(9, _("Genre:"), *(cdTextPages_[n].genre));
    add_row(10, _("Genre Info:"), *(cdTextPages_[n].genreInfo));

    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    vbox->append(*grid);
    return vbox;
}

void TocInfoDialog::start(TocEdit *view)
{
    tocEdit_ = view;
    update(UPD_ALL);
}

void TocInfoDialog::setSelectedTocType()
{
    auto sel = dropDownGet(&tocType_);
    if (!sel.has_value())
        return;
    if (*sel == TOC_TYPE_CD_DA)
        selectedTocType_ = Toc::Type::CD_DA;
    else if (*sel == TOC_TYPE_CD_ROM)
        selectedTocType_ = Toc::Type::CD_ROM;
    else if (*sel == TOC_TYPE_CD_ROM_XA)
        selectedTocType_ = Toc::Type::CD_ROM_XA;
    else if (*sel == TOC_TYPE_CD_I)
        selectedTocType_ = Toc::Type::CD_I;
}

void TocInfoDialog::setSelectedCDTextLanguage(int block)
{
    int i;

    if (block < 0 || block >= 8)
        return;

    int value = cdTextPages_[block].language->get_selected();

    if (value <= 0) {
        // cannot set to 'unknown' or invalid value, restore old setting
        if (cdTextPages_[block].selectedLanguage != 0)
            cdTextPages_[block].language->set_selected(cdTextPages_[block].selectedLanguage);

        return;
    }

    if (value != 1) {
        // check if same language is already used
        bool found = false;

        for (i = 0; i < 8; i++) {
            if (i != block && cdTextPages_[i].selectedLanguage == value) {
                found = true;
                break;
            }
        }

        if (found || (block > 0 && cdTextPages_[block - 1].selectedLanguage == 1)) {
            // reset to old value if the same language is already used or
            // if the language of the previous block is undefined
            cdTextPages_[block].language->set_selected(cdTextPages_[block].selectedLanguage);
            return;
        }
    }

    cdTextPages_[block].selectedLanguage = value;
}

void TocInfoDialog::setSelectedCDTextGenre(int block)
{
    if (block < 0 || block >= 8)
        return;

    int value = cdTextPages_[block].genre->get_selected();
    if (value <= 0) {
        // cannot set to 'unknown', restore old setting
        if (cdTextPages_[block].selectedGenre != 0)
            cdTextPages_[block].genre->set_selected(cdTextPages_[block].selectedGenre);

        return;
    }

    cdTextPages_[block].selectedGenre = value;
}

void TocInfoDialog::createCdTextLanguageMenu(int n)
{
    cdTextPages_[n].language = Gtk::make_managed<Gtk::DropDown>();
    auto list = Gtk::StringList::create();
    for (int i = 0; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
        list->append(CD_TEXT_LANGUAGE_CODES[i].name);
    }
    cdTextPages_[n].language->set_model(list);
    cdTextPages_[n].language->property_selected().signal_changed().connect(
        bind(mem_fun(*this, &TocInfoDialog::setSelectedCDTextLanguage), n));
}

void TocInfoDialog::createCdTextGenreMenu(int n)
{
    cdTextPages_[n].genre = Gtk::make_managed<Gtk::DropDown>();
    auto list = Gtk::StringList::create();
    for (int i = 0; i < MAX_CD_TEXT_GENRE_CODES; ++i) {
        list->append(CD_TEXT_GENRE_CODES[i].name);
    }
    cdTextPages_[n].genre->set_model(list);
    cdTextPages_[n].genre->property_selected().signal_changed().connect(
        bind(mem_fun(*this, &TocInfoDialog::setSelectedCDTextGenre), n));
}

void TocInfoDialog::clear()
{
    nofTracks_->set_text("");
    tocLength_->set_text("");

    tocType_.set_selected(0);
    selectedTocType_ = Toc::Type::CD_DA;

    catalog_->set_text("");
    catalog_->set_editable(false);

    clearCdText();
}

void TocInfoDialog::update(unsigned long level)
{
    const Toc *toc;

    if (tocEdit_ == NULL) {
        clear();
        return;
    }

    auto s = tocEdit_->filename().string();
    s += " - ";
    s += APP_NAME;
    if (tocEdit_->tocDirty())
        s += "(*)";
    set_title(s);

    if (level & UPD_TOC_DATA) {
        toc = tocEdit_->toc();
        importData(toc);
    }

    if (level & UPD_EDITABLE_STATE) {
        applyButton_->set_sensitive(tocEdit_->editable() ? true : false);
        imdbButton_->set_sensitive(tocEdit_->editable() ? true : false);
    }

    if (tocEdit_->empty())
        imdbButton_->set_sensitive(false);
}

void TocInfoDialog::clearCdText()
{
    int l;

    for (l = 0; l < 8; l++) {
        cdTextPages_[l].title->set_text("");
        cdTextPages_[l].title->set_editable(false);

        cdTextPages_[l].performer->set_text("");
        cdTextPages_[l].performer->set_editable(false);

        cdTextPages_[l].songwriter->set_text("");
        cdTextPages_[l].songwriter->set_editable(false);

        cdTextPages_[l].composer->set_text("");
        cdTextPages_[l].composer->set_editable(false);

        cdTextPages_[l].arranger->set_text("");
        cdTextPages_[l].arranger->set_editable(false);

        cdTextPages_[l].message->set_text("");
        cdTextPages_[l].message->set_editable(false);

        cdTextPages_[l].catalog->set_text("");
        cdTextPages_[l].catalog->set_editable(false);

        cdTextPages_[l].upcEan->set_text("");
        cdTextPages_[l].upcEan->set_editable(false);

        cdTextPages_[l].language->set_selected(1);
        cdTextPages_[l].selectedLanguage = 1;

        cdTextPages_[l].genre->set_selected(1);
        cdTextPages_[l].selectedGenre = 1;
    }
}

void TocInfoDialog::applyAction()
{
    if (tocEdit_ == NULL || !tocEdit_->editable())
        return;

    exportData(tocEdit_);

    guiUpdate(UPD_TOC_DATA);
}

const char *TocInfoDialog::checkString(const std::string &str)
{
    static char *buf = NULL;
    static long bufLen = 0;
    char *p, *s;
    long len = strlen(str.c_str());

    if (len == 0)
        return NULL;

    if (buf == NULL || len + 1 > bufLen) {
        delete[] buf;
        bufLen = len + 1;
        buf = new char[bufLen];
    }

    strcpy(buf, str.c_str());

    s = buf;
    p = buf + len - 1;

    while (*s != 0 && isspace(*s))
        s++;

    if (*s == 0)
        return NULL;

    while (p > s && isspace(*p)) {
        *p = 0;
        p--;
    }

    return s;
}

int TocInfoDialog::getCdTextLanguageIndex(int code)
{
    int i;

    if (code < 0)
        return 1; // undefined

    for (i = 2; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
        if (CD_TEXT_LANGUAGE_CODES[i].code == code)
            return i;
    }

    return 0; // unknown
}

int TocInfoDialog::getCdTextGenreIndex(int code1, int code2)
{
    int i;

    for (i = 2; i < MAX_CD_TEXT_GENRE_CODES; i++) {
        if (CD_TEXT_GENRE_CODES[i].code1 == code1 && CD_TEXT_GENRE_CODES[i].code2 == code2)
            return i;
    }

    return 0;
}

void TocInfoDialog::importCdText(const Toc *toc)
{
    int l;
    const CdTextItem *item;

    for (l = 0; l < 8; l++) {
        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::TITLE)) != NULL)
            cdTextPages_[l].title->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].title->set_text("");
        cdTextPages_[l].title->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::PERFORMER)) != NULL)
            cdTextPages_[l].performer->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].performer->set_text("");
        cdTextPages_[l].performer->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::SONGWRITER)) != NULL)
            cdTextPages_[l].songwriter->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].songwriter->set_text("");
        cdTextPages_[l].songwriter->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::COMPOSER)) != NULL)
            cdTextPages_[l].composer->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].composer->set_text("");
        cdTextPages_[l].composer->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::ARRANGER)) != NULL)
            cdTextPages_[l].arranger->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].arranger->set_text("");
        cdTextPages_[l].arranger->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::MESSAGE)) != NULL)
            cdTextPages_[l].message->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].message->set_text("");
        cdTextPages_[l].message->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::DISK_ID)) != NULL)
            cdTextPages_[l].catalog->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].catalog->set_text("");
        cdTextPages_[l].catalog->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::UPCEAN_ISRC)) != NULL)
            cdTextPages_[l].upcEan->set_text((const char *)(item->data()));
        else
            cdTextPages_[l].upcEan->set_text("");
        cdTextPages_[l].upcEan->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::GENRE)) != NULL) {
            if (item->dataLen() >= 2) {
                cdTextPages_[l].selectedGenre =
                    getCdTextGenreIndex(item->data()[0], item->data()[1]);
            } else {
                cdTextPages_[l].selectedGenre = 0; // Unknwon
            }

            if (item->dataLen() > 2) {
                // Copy the supplementary genre information from the CD-TEXT item.
                // Carefully handle the case that the terminating 0 is missing.
                int len = item->dataLen() - 2 + 1;
                char *s = new char[len];
                memcpy(s, item->data() + 2, len - 1);
                s[len - 1] = 0;

                cdTextPages_[l].genreInfo->set_text(s);

                delete[] s;
            } else {
                cdTextPages_[l].genreInfo->set_text("");
            }
        } else {
            cdTextPages_[l].selectedGenre = 1; // not used
            cdTextPages_[l].genreInfo->set_text("");
        }
        cdTextPages_[l].genre->set_selected(cdTextPages_[l].selectedGenre);

        cdTextPages_[l].selectedLanguage = getCdTextLanguageIndex(toc->cdTextLanguage(l));
        cdTextPages_[l].language->set_selected(cdTextPages_[l].selectedLanguage);
    }
}

void TocInfoDialog::importData(const Toc *toc)
{
    char buf[50];
    int i;

    snprintf(buf, sizeof(buf), "%3d:%02d:%02d", toc->length().min(), toc->length().sec(),
             toc->length().frac());
    tocLength_->set_text(buf);

    snprintf(buf, sizeof(buf), "%3d", toc->nofTracks());
    nofTracks_->set_text(buf);

    if (toc->catalogValid()) {
        for (i = 0; i < 13; i++)
            buf[i] = toc->catalog(i) + '0';

        buf[13] = 0;

        catalog_->set_text(buf);
    } else {
        catalog_->set_text("");
    }

    catalog_->set_editable(true);

    switch (toc->tocType()) {
    case Toc::Type::CD_DA:
        tocType_.set_selected(0);
        break;
    case Toc::Type::CD_ROM:
        tocType_.set_selected(1);
        break;
    case Toc::Type::CD_ROM_XA:
        tocType_.set_selected(2);
        break;
    case Toc::Type::CD_I:
        tocType_.set_selected(3);
        break;
    }
    selectedTocType_ = toc->tocType();

    importCdText(toc);
}

void TocInfoDialog::exportData(TocEdit *tocEdit)
{
    const char *s, *s1;
    Toc *toc = tocEdit->toc();

    if (toc->tocType() != selectedTocType_) {
        tocEdit->setTocType(selectedTocType_);
    }

    s = checkString(catalog_->get_text());

    if (s == NULL) {
        if (toc->catalogValid())
            tocEdit->setCatalogNumber(NULL);
    } else if (strlen(s) == 13) {
        if ((s1 = toc->catalog()) == NULL || strcmp(s1, s) != 0)
            tocEdit->setCatalogNumber(s);
    }

    exportCdText(tocEdit);
}

void TocInfoDialog::exportCdText(TocEdit *tocEdit)
{
    int l;
    const char *s;
    const Toc *toc = tocEdit->toc();

    const CdTextItem *item;
    CdTextItem *newItem;

    for (l = 0; l < 8; l++) {
        // Title
        if ((s = checkString(cdTextPages_[l].title->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::TITLE, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::TITLE)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::TITLE, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::TITLE, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::TITLE, l, s);
        }

        delete newItem;

        // Performer
        if ((s = checkString(cdTextPages_[l].performer->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::PERFORMER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::PERFORMER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::PERFORMER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::PERFORMER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::PERFORMER, l, s);
        }

        delete newItem;

        // Songwriter
        if ((s = checkString(cdTextPages_[l].songwriter->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::SONGWRITER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::SONGWRITER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::SONGWRITER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::SONGWRITER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::SONGWRITER, l, s);
        }

        delete newItem;

        // Composer
        if ((s = checkString(cdTextPages_[l].composer->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::COMPOSER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::COMPOSER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::COMPOSER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::COMPOSER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::COMPOSER, l, s);
        }

        delete newItem;

        // Arranger
        if ((s = checkString(cdTextPages_[l].arranger->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::ARRANGER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::ARRANGER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::ARRANGER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::ARRANGER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::ARRANGER, l, s);
        }

        delete newItem;

        // Message
        if ((s = checkString(cdTextPages_[l].message->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::MESSAGE, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::MESSAGE)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::MESSAGE, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::MESSAGE, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::MESSAGE, l, s);
        }

        delete newItem;

        // Catalog
        if ((s = checkString(cdTextPages_[l].catalog->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::DISK_ID, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::DISK_ID)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::DISK_ID, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::DISK_ID, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::DISK_ID, l, s);
        }

        delete newItem;

        // Upc/Ean
        if ((s = checkString(cdTextPages_[l].upcEan->get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::UPCEAN_ISRC, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::UPCEAN_ISRC)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::UPCEAN_ISRC, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, CdTextItem::PackType::UPCEAN_ISRC, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, CdTextItem::PackType::UPCEAN_ISRC, l, s);
        }

        delete newItem;

        // Genre
        if (cdTextPages_[l].selectedGenre != 0) {
            int code1 = CD_TEXT_GENRE_CODES[cdTextPages_[l].selectedGenre].code1;
            int code2 = CD_TEXT_GENRE_CODES[cdTextPages_[l].selectedGenre].code2;
            s = checkString(cdTextPages_[l].genreInfo->get_text());

            if (cdTextPages_[l].selectedGenre > 1) {
                newItem = new CdTextItem(CdTextItem::PackType::GENRE, l);
                newItem->setGenre(code1, code2, s);
            } else
                newItem = NULL;

            if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::GENRE)) != NULL) {
                if (newItem == NULL)
                    tocEdit->setCdTextGenreItem(l, -1, -1, NULL);
                else if (*newItem != *item)
                    tocEdit->setCdTextGenreItem(l, code1, code2, s);
            } else if (newItem != NULL) {
                tocEdit->setCdTextGenreItem(l, code1, code2, s);
            }

            delete newItem;
        }

        // language
        if (cdTextPages_[l].selectedLanguage != 0) {
            int langCode = CD_TEXT_LANGUAGE_CODES[cdTextPages_[l].selectedLanguage].code;

            if (langCode != toc->cdTextLanguage(l))
                tocEdit->setCdTextLanguage(l, langCode);
        }
    }
}

void TocInfoDialog::imdbAction()
{
    log_message(0, "IMDB");

    Cddb cddb(tocEdit_->toc());

    if (cddb.connectDb("unknown", "unknown", "cdrdao", VERSION) != 0) {
        ErrorBox::message(*this, _("Unable to connect to CDDB database."));
        return;
    }
    if (cddb.queryDb() != 0) {
        ErrorBox::message(*this, _("Querying of CDDB database failed."));
        return;
    }

    auto results = cddb.queryResults();

    if (results.size() == 0) {
        ErrorBox::message(*this, _("No CDDB record found."));
        return;
    }

    // We skip the choice and go with the first one.
    for (const auto& res : results) {
        log_message(0, "%d %s %s %s", res.exactMatch,
                    res.category.c_str(),
                    res.diskId.c_str(),
                    res.title.c_str());
    }

    auto dbentry = cddb.readDb(results[0].category, results[0].diskId);
    if (!dbentry) {
        ErrorBox::message(*this, _("Unable to download CDDB record."));
        return;
    }
    cdTextPages_[0].title->set_text(dbentry->diskTitle);
    cdTextPages_[0].performer->set_text(dbentry->diskArtist);
}

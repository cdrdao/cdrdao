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
#include <algorithm>
#include <glibmm/i18n.h>
#include "TocEdit.h"
#include "CdTextItem.h"
#include "CdTextContainer.h"
#include "Toc.h"
#include "util.h"
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

namespace {

// Audio CD ("Red Book") supported text encodings; dropdown-index order.
constexpr Util::Encoding kEncodings[] = {
    Util::Encoding::ASCII,
    Util::Encoding::LATIN,
    Util::Encoding::MSJIS,
};
constexpr int kEncodingCount = sizeof(kEncodings) / sizeof(kEncodings[0]);
constexpr int kDefaultEncodingIndex = 1; // ISO-8859-1

int encodingToIndex(Util::Encoding enc)
{
    for (int i = 0; i < kEncodingCount; i++) {
        if (kEncodings[i] == enc)
            return i;
    }
    return kDefaultEncodingIndex;
}

} // namespace

TocInfoDialog::TocInfoDialog(Gtk::Window* parent)
{
    set_title(_("Project Info"));
    set_modal(true);
    set_transient_for(*parent);
    set_hide_on_close(true);

    auto contents = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    contents->set_margin(12);

    tocEdit_ = nullptr;
    selectedTocType_ = Toc::Type::CD_DA;

    nofTracks_ = Gtk::make_managed<Gtk::Label>("99");
    nofTracks_->add_css_class("dim-label");
    tocLength_ = Gtk::make_managed<Gtk::Label>("100:00:00");
    tocLength_->add_css_class("dim-label");

    // Summary group
    auto groupSummary = Gtk::make_managed<Adw::PreferencesGroup>();
    groupSummary->set_title(_("Summary"));
    {
        auto row = Gtk::make_managed<Adw::ActionRow>();
        row->set_title(_("Tracks"));
        row->add_suffix(*nofTracks_);
        groupSummary->add(*row);
    }
    {
        auto row = Gtk::make_managed<Adw::ActionRow>();
        row->set_title(_("Length"));
        row->add_suffix(*tocLength_);
        groupSummary->add(*row);
    }

    // Sub-channel group
    auto groupSub = Gtk::make_managed<Adw::PreferencesGroup>();
    groupSub->set_title(_("Sub-Channel"));

    tocType_ = Gtk::make_managed<Adw::ComboRow>();
    tocType_->set_title(_("TOC Type"));
    {
        auto list = Gtk::StringList::create({});
        list->append(TOC_TYPE_CD_DA);
        list->append(TOC_TYPE_CD_ROM);
        list->append(TOC_TYPE_CD_ROM_XA);
        list->append(TOC_TYPE_CD_I);
        tocType_->set_model(list);
        tocType_->property_selected().signal_changed().connect(
            sigc::mem_fun(*this, &TocInfoDialog::setSelectedTocType));
    }
    groupSub->add(*tocType_);

    catalog_ = Gtk::make_managed<Adw::EntryRow>();
    catalog_->set_title(_("UPC/EAN"));
    groupSub->add(*catalog_);

    // Summary and Sub-Channel side by side.
    auto topRow = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 12);
    groupSummary->set_hexpand(true);
    groupSub->set_hexpand(true);
    topRow->append(*groupSummary);
    topRow->append(*groupSub);
    contents->append(*topRow);

    // CD-TEXT section heading + notebook
    auto cdTextHeading = Gtk::make_managed<Gtk::Label>(_("CD-TEXT"));
    cdTextHeading->add_css_class("heading");
    cdTextHeading->set_halign(Gtk::Align::START);
    cdTextHeading->set_margin_top(6);
    contents->append(*cdTextHeading);

    cdTextNotebook_ = Gtk::make_managed<Gtk::Notebook>();
    cdTextNotebook_->set_vexpand(true);

    // Build all 8 page widgets up front, but keep them detached — they
    // are appended to the notebook only when the corresponding slot is
    // a "visible" language block.
    for (int i = 0; i < 8; i++)
        createCdTextPage(i);

    addBlockButton_ = Gtk::make_managed<Gtk::Button>();
    addBlockButton_->set_icon_name("list-add-symbolic");
    addBlockButton_->set_tooltip_text(_("Add language block"));
    addBlockButton_->signal_clicked().connect(
        sigc::mem_fun(*this, &TocInfoDialog::addBlockAction));
    cdTextNotebook_->set_action_widget(addBlockButton_, Gtk::PackType::END);

    contents->append(*cdTextNotebook_);

    // Button bar
    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    bbox->set_spacing(6);
    bbox->set_halign(Gtk::Align::END);
    bbox->set_margin_top(6);

    imdbButton_ = Gtk::make_managed<Gtk::Button>(_("Query IMDB"));
    imdbButton_->signal_clicked().connect(sigc::mem_fun(*this, &TocInfoDialog::imdbAction));
    bbox->append(*imdbButton_);
    {
        auto cb = Gtk::make_managed<Gtk::Button>(_("Clear"));
        cb->signal_clicked().connect([this](){ this->clear(); });
        bbox->append(*cb);
    }
    {
        auto closeButton = Gtk::make_managed<Gtk::Button>(_("Close"));
        closeButton->signal_clicked().connect(
            sigc::mem_fun(*this, &Gtk::Widget::hide));
        bbox->append(*closeButton);
    }
    applyButton_ = Gtk::make_managed<Gtk::Button>(_("Apply"));
    applyButton_->add_css_class("suggested-action");
    applyButton_->signal_clicked().connect(sigc::mem_fun(*this, &TocInfoDialog::applyAction));
    bbox->append(*applyButton_);
    contents->append(*bbox);

    set_child(*contents);
    set_default_size(720, 720);
}

Gtk::Widget* TocInfoDialog::createCdTextPage(int n)
{
    auto& page = cdTextPages_[n];

    // Composite tab label: language-name label + close button.
    page.tabLabel = Gtk::make_managed<Gtk::Label>(std::to_string(n));
    auto closeButton = Gtk::make_managed<Gtk::Button>();
    closeButton->set_icon_name("window-close-symbolic");
    closeButton->set_has_frame(false);
    closeButton->set_tooltip_text(_("Remove this language block"));
    closeButton->signal_clicked().connect([this, n] { removeBlock(n); });
    page.tabWidget = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    page.tabWidget->append(*(page.tabLabel));
    page.tabWidget->append(*closeButton);

    createCdTextLanguageMenu(n);
    createCdTextGenreMenu(n);

    // Per-language encoding selector (row above inner notebook)
    page.encoding = Gtk::make_managed<Adw::ComboRow>();
    page.encoding->set_title(_("Encoding"));
    {
        auto encList = Gtk::StringList::create();
        encList->append("ASCII");
        encList->append("ISO-8859-1");
        encList->append("MS-JIS");
        page.encoding->set_model(encList);
        page.encoding->set_selected(kDefaultEncodingIndex);
    }
    page.encoding->property_selected().signal_changed().connect(
        [this, n] { recomputeEncoding(n); });

    page.encodingWarning = Gtk::make_managed<Gtk::Label>();
    page.encodingWarning->set_halign(Gtk::Align::START);
    page.encodingWarning->set_visible(false);
    page.encodingWarning->add_css_class("error");
    page.encodingWarning->set_margin_start(6);

    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    vbox->set_margin(12);

    // Language + encoding live in a PreferencesGroup above the inner notebook
    auto settingsGroup = Gtk::make_managed<Adw::PreferencesGroup>();
    settingsGroup->add(*(page.language));
    settingsGroup->add(*(page.encoding));
    vbox->append(*settingsGroup);

    vbox->append(*(page.encodingWarning));

    // Inner notebook with Disc and Tracks sub-tabs
    auto inner = Gtk::make_managed<Gtk::Notebook>();
    inner->set_vexpand(true);
    inner->append_page(*createDiscTab(n), *Gtk::make_managed<Gtk::Label>(_("Disc")));
    inner->append_page(*createTracksTab(n), *Gtk::make_managed<Gtk::Label>(_("Tracks")));
    vbox->append(*inner);

    page.pageWidget = vbox;

    // Managed widgets die when their parent drops its last ref. Since we
    // reparent these between the notebook and "detached" state, hold an
    // extra reference on the outer page + tab widgets to keep them alive
    // across remove_page/append_page cycles. (Inner children are owned
    // by the outer widget and follow its lifetime.)
    page.pageWidget->reference();
    page.tabWidget->reference();

    return vbox;
}

Gtk::Widget* TocInfoDialog::createDiscTab(int n)
{
    auto& page = cdTextPages_[n];

    auto scrolled = Gtk::make_managed<Gtk::ScrolledWindow>();
    scrolled->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scrolled->set_vexpand(true);

    auto group = Gtk::make_managed<Adw::PreferencesGroup>();
    group->set_margin(12);

    auto mkEntryRow = [](const Glib::ustring& title) {
        auto r = Gtk::make_managed<Adw::EntryRow>();
        r->set_title(title);
        return r;
    };

    page.title = mkEntryRow(_("Title"));
    page.performer = mkEntryRow(_("Performer"));
    page.songwriter = mkEntryRow(_("Songwriter"));
    page.composer = mkEntryRow(_("Composer"));
    page.arranger = mkEntryRow(_("Arranger"));
    page.message = mkEntryRow(_("Message"));
    page.catalog = mkEntryRow(_("Catalog"));
    page.upcEan = mkEntryRow(_("UPC/EAN"));
    page.genreInfo = mkEntryRow(_("Genre Info"));

    group->add(*(page.title));
    group->add(*(page.performer));
    group->add(*(page.songwriter));
    group->add(*(page.composer));
    group->add(*(page.arranger));
    group->add(*(page.message));
    group->add(*(page.catalog));
    group->add(*(page.upcEan));
    group->add(*(page.genre));
    group->add(*(page.genreInfo));

    auto attachEncodingRecompute = [this, n](Adw::EntryRow *e) {
        e->signal_changed().connect([this, n] { recomputeEncoding(n); });
    };
    attachEncodingRecompute(page.title);
    attachEncodingRecompute(page.performer);
    attachEncodingRecompute(page.songwriter);
    attachEncodingRecompute(page.composer);
    attachEncodingRecompute(page.arranger);
    attachEncodingRecompute(page.message);
    attachEncodingRecompute(page.catalog);
    attachEncodingRecompute(page.upcEan);
    attachEncodingRecompute(page.genreInfo);

    scrolled->set_child(*group);
    return scrolled;
}

Gtk::Widget* TocInfoDialog::createTracksTab(int n)
{
    auto& page = cdTextPages_[n];

    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 12);
    vbox->set_margin(12);

    // Controls row: enable-performer + fill-performer
    auto controls = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    page.performerButton = Gtk::make_managed<Gtk::CheckButton>(_("Enable Performer Entries"));
    page.performerButton->set_active(false);
    page.performerButton->signal_toggled().connect(
        [this, n] { activatePerformerAction(n); });
    controls->append(*(page.performerButton));

    auto fillButton = Gtk::make_managed<Gtk::Button>(_("Fill Performer"));
    fillButton->signal_clicked().connect([this, n] { fillPerformerAction(n); });
    controls->append(*fillButton);
    vbox->append(*controls);

    // One Adw::ExpanderRow per track; rows are added/removed by adjustTableEntries().
    page.tracksGroup = Gtk::make_managed<Adw::PreferencesGroup>();

    auto swin = Gtk::make_managed<Gtk::ScrolledWindow>();
    swin->set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    swin->set_child(*(page.tracksGroup));
    swin->set_expand(true);
    vbox->append(*swin);

    return vbox;
}

void TocInfoDialog::adjustTableEntries(int n)
{
    if (trackEntries_ == n) return;

    for (int l = 0; l < 8; l++) {
        auto& page = cdTextPages_[l];

        if (n < trackEntries_) {
            for (int i = n; i < trackEntries_; i++)
                page.tracksGroup->remove(*page.tracks[i].row);
            page.tracks.resize(n);
        } else {
            bool performerActive = page.performerButton->get_active();
            page.tracks.resize(n);

            for (int i = trackEntries_; i < n; i++) {
                char buf[20];
                snprintf(buf, sizeof(buf), _("Track %02d"), i + 1);

                page.tracks[i].title = Gtk::make_managed<Adw::EntryRow>();
                page.tracks[i].title->set_title(_("Title"));

                page.tracks[i].performer = Gtk::make_managed<Adw::EntryRow>();
                page.tracks[i].performer->set_title(_("Performer"));
                page.tracks[i].performer->set_sensitive(performerActive);

                page.tracks[i].row = Gtk::make_managed<Adw::ExpanderRow>();
                page.tracks[i].row->set_title(buf);
                page.tracks[i].row->add_row(*(page.tracks[i].title));
                page.tracks[i].row->add_row(*(page.tracks[i].performer));

                page.tracksGroup->add(*(page.tracks[i].row));

                // The row's subtitle mirrors the current track title so the
                // collapsed view is still informative.
                int iCopy = i;
                page.tracks[i].title->signal_changed().connect(
                    [this, l, iCopy] {
                        auto& pg = cdTextPages_[l];
                        pg.tracks[iCopy].row->set_subtitle(
                            pg.tracks[iCopy].title->get_text());
                        recomputeEncoding(l);
                    });
                page.tracks[i].performer->signal_changed().connect(
                    [this, l] { recomputeEncoding(l); });
            }
        }
    }
    trackEntries_ = n;
}

void TocInfoDialog::updateTabLabels()
{
    // Labels reflect the language currently selected in each visible tab.
    for (int slot : visibleBlocks_) {
        auto& page = cdTextPages_[slot];
        int langIdx = page.selectedLanguage;
        if (langIdx < 0 || langIdx >= MAX_CD_TEXT_LANGUAGE_CODES)
            langIdx = 1;
        const char *s =
            CdTextContainer::languageName(CD_TEXT_LANGUAGE_CODES[langIdx].code);
        if (page.tabLabel->get_label() != s)
            page.tabLabel->set_label(s);
    }
}

int TocInfoDialog::findUnusedSlot() const
{
    for (int i = 0; i < 8; i++) {
        if (std::find(visibleBlocks_.begin(), visibleBlocks_.end(), i) ==
            visibleBlocks_.end())
            return i;
    }
    return -1;
}

int TocInfoDialog::findUnusedLanguageIndex() const
{
    // CD_TEXT_LANGUAGE_CODES indices 0 and 1 are "Unknown"/"Undefined"; skip.
    for (int i = 2; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
        bool used = false;
        for (int slot : visibleBlocks_) {
            if (cdTextPages_[slot].selectedLanguage == i) {
                used = true;
                break;
            }
        }
        if (!used)
            return i;
    }
    return 2; // all used — fall back to the first real language
}

void TocInfoDialog::resetBlockData(int slot)
{
    auto& page = cdTextPages_[slot];
    bool wasImporting = importing_;
    importing_ = true;

    page.title->set_text("");
    page.performer->set_text("");
    page.songwriter->set_text("");
    page.composer->set_text("");
    page.arranger->set_text("");
    page.message->set_text("");
    page.catalog->set_text("");
    page.upcEan->set_text("");
    page.genreInfo->set_text("");

    page.selectedGenre = 1; // not used
    page.genre->set_selected(page.selectedGenre);

    page.encoding->set_selected(kDefaultEncodingIndex);
    page.encodingWarning->set_visible(false);

    page.performerButton->set_active(false);
    for (auto& t : page.tracks) {
        t.title->set_text("");
        t.performer->set_text("");
        t.performer->set_sensitive(false);
    }

    importing_ = wasImporting;
}

void TocInfoDialog::updateAddButtonSensitivity()
{
    addBlockButton_->set_sensitive((int)visibleBlocks_.size() < 8);
}

void TocInfoDialog::syncTabs()
{
    // Remove every page from the notebook (without destroying the widgets),
    // then re-append in visibleBlocks_ order.
    while (cdTextNotebook_->get_n_pages() > 0)
        cdTextNotebook_->remove_page(0);

    for (int slot : visibleBlocks_) {
        cdTextNotebook_->append_page(*cdTextPages_[slot].pageWidget,
                                     *cdTextPages_[slot].tabWidget);
    }
    updateAddButtonSensitivity();
    updateTabLabels();
}

void TocInfoDialog::addBlockAction()
{
    if ((int)visibleBlocks_.size() >= 8)
        return;

    int slot = findUnusedSlot();
    if (slot < 0)
        return;

    auto& page = cdTextPages_[slot];
    resetBlockData(slot);

    // Make entries editable — a prior `clearCdText` may have left them
    // read-only for unused slots.
    page.title->set_editable(true);
    page.performer->set_editable(true);
    page.songwriter->set_editable(true);
    page.composer->set_editable(true);
    page.arranger->set_editable(true);
    page.message->set_editable(true);
    page.catalog->set_editable(true);
    page.upcEan->set_editable(true);

    // Pick a language that isn't already used by another visible block.
    int langIdx = findUnusedLanguageIndex();
    page.selectedLanguage = langIdx;
    importing_ = true;
    page.language->set_selected(langIdx);
    importing_ = false;

    visibleBlocks_.push_back(slot);

    cdTextNotebook_->append_page(*page.pageWidget, *page.tabWidget);
    updateAddButtonSensitivity();
    updateTabLabels();

    // Focus the newly added tab for immediate editing.
    int pos = cdTextNotebook_->page_num(*page.pageWidget);
    if (pos >= 0)
        cdTextNotebook_->set_current_page(pos);
}

void TocInfoDialog::removeBlock(int slot)
{
    auto it = std::find(visibleBlocks_.begin(), visibleBlocks_.end(), slot);
    if (it == visibleBlocks_.end())
        return;

    visibleBlocks_.erase(it);
    cdTextNotebook_->remove_page(*cdTextPages_[slot].pageWidget);
    resetBlockData(slot);
    updateAddButtonSensitivity();
}

void TocInfoDialog::start(TocEdit *view)
{
    tocEdit_ = view;
    update(UPD_ALL);
}

void TocInfoDialog::setSelectedTocType()
{
    switch (tocType_->get_selected()) {
    case 0: selectedTocType_ = Toc::Type::CD_DA;     break;
    case 1: selectedTocType_ = Toc::Type::CD_ROM;    break;
    case 2: selectedTocType_ = Toc::Type::CD_ROM_XA; break;
    case 3: selectedTocType_ = Toc::Type::CD_I;      break;
    default: break;
    }
}

void TocInfoDialog::setSelectedCDTextLanguage(int block)
{
    if (importing_ || block < 0 || block >= 8)
        return;

    int value = cdTextPages_[block].language->get_selected();

    // A visible tab must pick a real language. Reject "Unknown" (0) and
    // "Undefined" (1); restore the prior selection if either is chosen.
    if (value <= 1) {
        if (cdTextPages_[block].selectedLanguage > 1)
            cdTextPages_[block].language->set_selected(cdTextPages_[block].selectedLanguage);
        return;
    }

    // Reject duplicates — only visible blocks count.
    for (int other : visibleBlocks_) {
        if (other != block && cdTextPages_[other].selectedLanguage == value) {
            cdTextPages_[block].language->set_selected(cdTextPages_[block].selectedLanguage);
            return;
        }
    }

    cdTextPages_[block].selectedLanguage = value;

    // Refresh the outer tab label to reflect the new language.
    const char *s = CdTextContainer::languageName(CD_TEXT_LANGUAGE_CODES[value].code);
    cdTextPages_[block].tabLabel->set_label(s);
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
    cdTextPages_[n].language = Gtk::make_managed<Adw::ComboRow>();
    cdTextPages_[n].language->set_title(_("Language"));
    auto list = Gtk::StringList::create();
    for (int i = 0; i < MAX_CD_TEXT_LANGUAGE_CODES; i++) {
        list->append(CD_TEXT_LANGUAGE_CODES[i].name);
    }
    cdTextPages_[n].language->set_model(list);
    cdTextPages_[n].language->property_selected().signal_changed().connect(
        bind(sigc::mem_fun(*this, &TocInfoDialog::setSelectedCDTextLanguage), n));
}

void TocInfoDialog::createCdTextGenreMenu(int n)
{
    cdTextPages_[n].genre = Gtk::make_managed<Adw::ComboRow>();
    cdTextPages_[n].genre->set_title(_("Genre"));
    auto list = Gtk::StringList::create();
    for (int i = 0; i < MAX_CD_TEXT_GENRE_CODES; ++i) {
        list->append(CD_TEXT_GENRE_CODES[i].name);
    }
    cdTextPages_[n].genre->set_model(list);
    cdTextPages_[n].genre->property_selected().signal_changed().connect(
        bind(sigc::mem_fun(*this, &TocInfoDialog::setSelectedCDTextGenre), n));
}

void TocInfoDialog::clear()
{
    nofTracks_->set_text("");
    tocLength_->set_text("");

    tocType_->set_selected(0);
    selectedTocType_ = Toc::Type::CD_DA;

    catalog_->set_text("");
    catalog_->set_editable(false);

    clearCdText();
}

void TocInfoDialog::fillPerformerAction(int l)
{
    if (l < 0 || l >= 8)
        return;
    auto& page = cdTextPages_[l];
    const char *s = checkString(page.performer->get_text());
    if (!s)
        return;
    for (auto& t : page.tracks)
        t.performer->set_text(s);
}

void TocInfoDialog::activatePerformerAction(int l)
{
    if (l < 0 || l >= 8)
        return;
    bool val = cdTextPages_[l].performerButton->get_active();
    for (auto& t : cdTextPages_[l].tracks)
        t.performer->set_sensitive(val);
}

void TocInfoDialog::recomputeEncoding(int l)
{
    if (importing_ || l < 0 || l >= 8)
        return;

    auto& page = cdTextPages_[l];

    // Concatenate every text field for this language; if the blob fits
    // an encoding, every individual field does too.
    std::string all;
    auto append = [&](Gtk::Editable *e) {
        all += e->get_text().raw();
        all += '\n';
    };
    append(page.title);
    append(page.performer);
    append(page.songwriter);
    append(page.composer);
    append(page.arranger);
    append(page.message);
    append(page.catalog);
    append(page.upcEan);
    append(page.genreInfo);
    for (auto& t : page.tracks) {
        append(t.title);
        append(t.performer);
    }

    bool fits[kEncodingCount];
    for (int e = 0; e < kEncodingCount; e++) {
        std::vector<u8> out;
        fits[e] = Util::from_utf8(all, out, kEncodings[e]);
    }

    int cur = page.encoding->get_selected();
    if (cur < 0 || cur >= kEncodingCount)
        cur = kDefaultEncodingIndex;

    if (!fits[cur]) {
        // Auto-upgrade to smallest encoding that fits; never downgrade.
        for (int e = 0; e < kEncodingCount; e++) {
            if (fits[e]) {
                page.encoding->set_selected(e);
                // set_selected() re-enters via signal_changed; that call
                // refreshes the warning state.
                return;
            }
        }
    }

    bool anyFits = fits[0] || fits[1] || fits[2];
    if (!anyFits) {
        page.encodingWarning->set_text(_("Text cannot be encoded"));
        page.encodingWarning->set_visible(true);
    } else {
        page.encodingWarning->set_visible(false);
    }
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

    if (level & (UPD_TOC_DATA | UPD_TRACK_DATA)) {
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
    importing_ = true;
    for (int l = 0; l < 8; l++) {
        auto& page = cdTextPages_[l];

        page.title->set_text("");
        page.title->set_editable(false);

        page.performer->set_text("");
        page.performer->set_editable(false);

        page.songwriter->set_text("");
        page.songwriter->set_editable(false);

        page.composer->set_text("");
        page.composer->set_editable(false);

        page.arranger->set_text("");
        page.arranger->set_editable(false);

        page.message->set_text("");
        page.message->set_editable(false);

        page.catalog->set_text("");
        page.catalog->set_editable(false);

        page.upcEan->set_text("");
        page.upcEan->set_editable(false);

        page.language->set_selected(1);
        page.selectedLanguage = 1;

        page.genre->set_selected(1);
        page.selectedGenre = 1;

        page.encoding->set_selected(kDefaultEncodingIndex);
        page.encodingWarning->set_visible(false);
    }

    // Remove any track rows as well.
    adjustTableEntries(0);

    // Drop all tabs from the notebook.
    visibleBlocks_.clear();
    while (cdTextNotebook_->get_n_pages() > 0)
        cdTextNotebook_->remove_page(0);
    updateAddButtonSensitivity();

    importing_ = false;
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
    const CdTextItem *item;

    adjustTableEntries(toc->nofTracks());

    // Reset every slot first (clears fields and drops prior warnings).
    for (int l = 0; l < 8; l++)
        resetBlockData(l);

    // Determine which TOC blocks have a language defined — those are the
    // ones the user cares about. The TOC's 1:1 slot↔block mapping at
    // import time means we can use the TOC block number as the slot
    // number directly; the mapping may diverge later if the user
    // deletes tabs.
    visibleBlocks_.clear();
    for (int l = 0; l < 8; l++) {
        if (toc->cdTextLanguage(l) >= 0)
            visibleBlocks_.push_back(l);
    }

    for (int l : visibleBlocks_) {
        auto& page = cdTextPages_[l];

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::TITLE)) != NULL)
            page.title->set_text((const char *)(item->data()));
        else
            page.title->set_text("");
        page.title->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::PERFORMER)) != NULL)
            page.performer->set_text((const char *)(item->data()));
        else
            page.performer->set_text("");
        page.performer->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::SONGWRITER)) != NULL)
            page.songwriter->set_text((const char *)(item->data()));
        else
            page.songwriter->set_text("");
        page.songwriter->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::COMPOSER)) != NULL)
            page.composer->set_text((const char *)(item->data()));
        else
            page.composer->set_text("");
        page.composer->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::ARRANGER)) != NULL)
            page.arranger->set_text((const char *)(item->data()));
        else
            page.arranger->set_text("");
        page.arranger->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::MESSAGE)) != NULL)
            page.message->set_text((const char *)(item->data()));
        else
            page.message->set_text("");
        page.message->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::DISK_ID)) != NULL)
            page.catalog->set_text((const char *)(item->data()));
        else
            page.catalog->set_text("");
        page.catalog->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::UPCEAN_ISRC)) != NULL)
            page.upcEan->set_text((const char *)(item->data()));
        else
            page.upcEan->set_text("");
        page.upcEan->set_editable(true);

        if ((item = toc->getCdTextItem(0, l, CdTextItem::PackType::GENRE)) != NULL) {
            if (item->dataLen() >= 2) {
                page.selectedGenre =
                    getCdTextGenreIndex(item->data()[0], item->data()[1]);
            } else {
                page.selectedGenre = 0; // Unknown
            }

            if (item->dataLen() > 2) {
                // Carefully handle the case that the terminating 0 is missing.
                int len = item->dataLen() - 2 + 1;
                char *s = new char[len];
                memcpy(s, item->data() + 2, len - 1);
                s[len - 1] = 0;

                page.genreInfo->set_text(s);

                delete[] s;
            } else {
                page.genreInfo->set_text("");
            }
        } else {
            page.selectedGenre = 1; // not used
            page.genreInfo->set_text("");
        }
        page.genre->set_selected(page.selectedGenre);

        page.selectedLanguage = getCdTextLanguageIndex(toc->cdTextLanguage(l));
        page.language->set_selected(page.selectedLanguage);

        page.encoding->set_selected(encodingToIndex(toc->cdTextEncoding(l)));

        // Per-track entries
        int n = toc->nofTracks();
        for (int i = 0; i < n; i++) {
            auto tTitle = toc->getCdTextItem(i + 1, l, CdTextItem::PackType::TITLE);
            page.tracks[i].title->set_text(tTitle ? tTitle->getText() : "");

            auto tPerf = toc->getCdTextItem(i + 1, l, CdTextItem::PackType::PERFORMER);
            page.tracks[i].performer->set_text(tPerf ? tPerf->getText() : "");
        }
    }

    // Re-attach the right set of tabs to the notebook.
    syncTabs();
}

void TocInfoDialog::importData(const Toc *toc)
{
    importing_ = true;

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
        tocType_->set_selected(0);
        break;
    case Toc::Type::CD_ROM:
        tocType_->set_selected(1);
        break;
    case Toc::Type::CD_ROM_XA:
        tocType_->set_selected(2);
        break;
    case Toc::Type::CD_I:
        tocType_->set_selected(3);
        break;
    }
    selectedTocType_ = toc->tocType();

    importCdText(toc);
    updateTabLabels();

    importing_ = false;

    for (int l = 0; l < 8; l++)
        recomputeEncoding(l);
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
    const char *s;
    const Toc *toc = tocEdit->toc();

    const CdTextItem *item;
    CdTextItem *newItem;

    // Write the disc-level CD-TEXT pack or clear it when the field is empty.
    auto applyPack = [&](int block, CdTextItem::PackType type, Gtk::Editable *entry) {
        const char *text = checkString(entry->get_text());
        if (text != NULL) {
            newItem = new CdTextItem(type, block);
            newItem->setText(text);
        } else {
            newItem = NULL;
        }

        if ((item = toc->getCdTextItem(0, block, type)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(0, type, block, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(0, type, block, text);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(0, type, block, text);
        }

        delete newItem;
    };

    // For each tab position p, write the associated slot's data into
    // TOC block p. This compacts the blocks starting at 0 and preserves
    // the Red Book requirement that language-block numbers be
    // contiguous starting at 0.
    for (int p = 0; p < (int)visibleBlocks_.size(); p++) {
        int slot = visibleBlocks_[p];
        auto& page = cdTextPages_[slot];
        int l = p;

        applyPack(l, CdTextItem::PackType::TITLE, page.title);
        applyPack(l, CdTextItem::PackType::PERFORMER, page.performer);
        applyPack(l, CdTextItem::PackType::SONGWRITER, page.songwriter);
        applyPack(l, CdTextItem::PackType::COMPOSER, page.composer);
        applyPack(l, CdTextItem::PackType::ARRANGER, page.arranger);
        applyPack(l, CdTextItem::PackType::MESSAGE, page.message);
        applyPack(l, CdTextItem::PackType::DISK_ID, page.catalog);
        applyPack(l, CdTextItem::PackType::UPCEAN_ISRC, page.upcEan);

        // Genre
        if (page.selectedGenre != 0) {
            int code1 = CD_TEXT_GENRE_CODES[page.selectedGenre].code1;
            int code2 = CD_TEXT_GENRE_CODES[page.selectedGenre].code2;
            s = checkString(page.genreInfo->get_text());

            if (page.selectedGenre > 1) {
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

        // Language
        if (page.selectedLanguage != 0) {
            int langCode = CD_TEXT_LANGUAGE_CODES[page.selectedLanguage].code;

            if (langCode != toc->cdTextLanguage(l))
                tocEdit->setCdTextLanguage(l, langCode);
        }

        // Per-track title/performer (applyPack only handles trackNr=0,
        // so write tracks directly).
        for (int i = 0; i < (int)page.tracks.size(); i++) {
            int trackNr = i + 1;

            const char *tText = checkString(page.tracks[i].title->get_text());
            const CdTextItem *tItem =
                toc->getCdTextItem(trackNr, l, CdTextItem::PackType::TITLE);
            if (tText != NULL) {
                CdTextItem tmp(CdTextItem::PackType::TITLE, l);
                tmp.setText(tText);
                if (!tItem || tmp != *tItem)
                    tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::TITLE, l, tText);
            } else if (tItem) {
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::TITLE, l, NULL);
            }

            const char *pText = checkString(page.tracks[i].performer->get_text());
            const CdTextItem *pItem =
                toc->getCdTextItem(trackNr, l, CdTextItem::PackType::PERFORMER);
            if (pText != NULL) {
                CdTextItem tmp(CdTextItem::PackType::PERFORMER, l);
                tmp.setText(pText);
                if (!pItem || tmp != *pItem)
                    tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::PERFORMER, l, pText);
            } else if (pItem) {
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::PERFORMER, l, NULL);
            }
        }

        // Encoding
        int encIdx = page.encoding->get_selected();
        if (encIdx >= 0 && encIdx < kEncodingCount)
            tocEdit->setCdTextEncoding(l, kEncodings[encIdx]);
    }

    // TOC blocks beyond the number of visible tabs are no longer in use
    // — wipe any CD-TEXT data still hanging off them.
    for (int l = (int)visibleBlocks_.size(); l < 8; l++)
        clearBlockInToc(tocEdit, l);
}

void TocInfoDialog::clearBlockInToc(TocEdit *tocEdit, int blockNr)
{
    const Toc *toc = tocEdit->toc();

    static const CdTextItem::PackType discPacks[] = {
        CdTextItem::PackType::TITLE,       CdTextItem::PackType::PERFORMER,
        CdTextItem::PackType::SONGWRITER,  CdTextItem::PackType::COMPOSER,
        CdTextItem::PackType::ARRANGER,    CdTextItem::PackType::MESSAGE,
        CdTextItem::PackType::DISK_ID,     CdTextItem::PackType::UPCEAN_ISRC,
    };
    for (auto t : discPacks) {
        if (toc->getCdTextItem(0, blockNr, t))
            tocEdit->setCdTextItem(0, t, blockNr, NULL);
    }
    if (toc->getCdTextItem(0, blockNr, CdTextItem::PackType::GENRE))
        tocEdit->setCdTextGenreItem(blockNr, -1, -1, NULL);

    int n = toc->nofTracks();
    for (int i = 1; i <= n; i++) {
        if (toc->getCdTextItem(i, blockNr, CdTextItem::PackType::TITLE))
            tocEdit->setCdTextItem(i, CdTextItem::PackType::TITLE, blockNr, NULL);
        if (toc->getCdTextItem(i, blockNr, CdTextItem::PackType::PERFORMER))
            tocEdit->setCdTextItem(i, CdTextItem::PackType::PERFORMER, blockNr, NULL);
    }

    if (toc->cdTextLanguage(blockNr) != -1)
        tocEdit->setCdTextLanguage(blockNr, -1);
    tocEdit->setCdTextEncoding(blockNr, Util::Encoding::UNSET);
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

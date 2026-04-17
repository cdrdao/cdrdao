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

#include "CdTextDialog.h"
#include "config.h"

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include <stddef.h>
#include <string.h>

#include "Toc.h"
#include "TocEdit.h"
#include "util.h"

#include "guiUpdate.h"

namespace {

// Encodings supported by the Audio CD "Red Book" specification.
// Indices here are the positions in the dropdown model.
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

CdTextDialog::CdTextDialog(Gtk::Window* parent)
{
    set_title(_("CD-TEXT Entry"));
    set_default_size(600,400);
    set_modal(true);
    set_transient_for(*parent);

    set_child(main_vbox_);

    languages_ = Gtk::make_managed<Gtk::Notebook>();
    languages_->set_expand(true);

    for (int i = 0; i < 8; i++) {
        page_[i].table = Gtk::make_managed<Gtk::Grid>();
        page_[i].table->set_row_homogeneous(false);
        page_[i].table->set_row_spacing(5);
        page_[i].table->set_column_spacing(5);
        
        page_[i].performer = Gtk::make_managed<Gtk::Entry>();
        page_[i].title = Gtk::make_managed<Gtk::Entry>();
        page_[i].tabLabel = Gtk::make_managed<Gtk::Label>("");
        
        page_[i].performerButton = Gtk::make_managed<Gtk::CheckButton>(_("Enable Performer Entries"));
        page_[i].performerButton->set_active(false);
        page_[i].performerButton->signal_toggled().connect(
            [this, i] { activatePerformerAction(i); });

        page_[i].encoding = Gtk::make_managed<Gtk::DropDown>();
        {
            auto encList = Gtk::StringList::create();
            encList->append("ASCII");
            encList->append("ISO-8859-1");
            encList->append("MS-JIS");
            page_[i].encoding->set_model(encList);
            page_[i].encoding->set_selected(kDefaultEncodingIndex);
        }
        page_[i].encoding->property_selected().signal_changed().connect(
            [this, i] { recomputeEncoding(i); });

        page_[i].encodingWarning = Gtk::make_managed<Gtk::Label>();
        page_[i].encodingWarning->set_halign(Gtk::Align::START);
        page_[i].encodingWarning->set_visible(false);

        page_[i].title->signal_changed().connect([this, i] { recomputeEncoding(i); });
        page_[i].performer->signal_changed().connect([this, i] { recomputeEncoding(i); });

        page_[i].tracks = nullptr;

        // Grid attachment: column, row, width, height
        page_[i].table->attach(*Gtk::make_managed<Gtk::Label>(_("Performer")), 1, 0);
        page_[i].table->attach(*Gtk::make_managed<Gtk::Label>(_("Title")), 2, 0);

        // Album row
        page_[i].table->attach(*Gtk::make_managed<Gtk::Label>(_("Album")), 0, 1);
        page_[i].title->set_hexpand(true);
        page_[i].table->attach(*(page_[i].title), 2, 1);
        page_[i].table->attach(*(page_[i].performer), 1, 1);

        // Performer Toggle row
        page_[i].table->attach(*(page_[i].performerButton), 1, 2);

        // Encoding selector row
        page_[i].table->attach(*Gtk::make_managed<Gtk::Label>(_("Encoding")), 0, 3);
        page_[i].table->attach(*(page_[i].encoding), 1, 3);
        page_[i].table->attach(*(page_[i].encodingWarning), 2, 3);

        // Scrolled Window for the tracks
        auto swin = Gtk::make_managed<Gtk::ScrolledWindow>();
        swin->set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
        swin->set_child(*page_[i].table);
        swin->set_expand(true);

        languages_->append_page(*swin, *(page_[i].tabLabel));
    }

    main_vbox_.append(*languages_);

    // Action Area (Manual implementation since we aren't using Gtk::Dialog)
    auto action_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    action_hbox->set_margin(10);
    action_hbox->set_halign(Gtk::Align::CENTER);

    applyButton_ = Gtk::make_managed<Gtk::Button>(_("Apply"));
    applyButton_->signal_clicked().connect(sigc::mem_fun(*this, &CdTextDialog::applyAction));
    action_hbox->append(*applyButton_);

    auto fillButton = Gtk::make_managed<Gtk::Button>(_("Fill Performer"));
    fillButton->signal_clicked().connect(sigc::mem_fun(*this, &CdTextDialog::fillPerformerAction));
    action_hbox->append(*fillButton);

    auto closeButton = Gtk::make_managed<Gtk::Button>(_("Close"));
    closeButton->signal_clicked().connect(sigc::mem_fun(*this, &CdTextDialog::stop));
    action_hbox->append(*closeButton);

    main_vbox_.append(*action_hbox);
}

CdTextDialog::~CdTextDialog()
{
    for(int l = 0; l < 8; l++) {
        delete[] page_[l].tracks; // Cleanup allocated array of structs
    }
}

bool CdTextDialog::on_close_request()
{
    stop();
    return true;
}

void CdTextDialog::updateTabLabels()
{
    const Toc *toc = tocEdit_->toc();
    for (int l = 0; l < 8; l++) {
        auto *s = CdTextContainer::languageName(toc->cdTextLanguage(l));

        if (page_[l].tabLabel->get_label() != s)
            page_[l].tabLabel->set_label(s);
    }
}

void CdTextDialog::adjustTableEntries(int n)
{
    if (trackEntries_ == n) return;

    for (int l = 0; l < 8; l++) {
        if (n < trackEntries_) {
            for (int i = n; i < trackEntries_; i++) {
                // In GTK4, removing from grid is done via remove()
                page_[l].table->remove(*page_[i].tracks[i].hbox);
                page_[l].table->remove(*page_[i].tracks[i].title);
                page_[l].table->remove(*page_[i].tracks[i].performer);
            }
        } else {
            bool performerActive = page_[l].performerButton->get_active();
            TableEntry *newTracks = new TableEntry[n];

            for (int i = 0; i < trackEntries_; i++)
                newTracks[i] = page_[l].tracks[i];

            delete[] page_[l].tracks;
            page_[l].tracks = newTracks;

            for (int i = trackEntries_; i < n; i++) {
                char buf[20];
                snprintf(buf, sizeof(buf), _("Track %02d"), i + 1);

                page_[l].tracks[i].performer = Gtk::make_managed<Gtk::Entry>();
                page_[l].tracks[i].performer->set_sensitive(performerActive);
                page_[l].tracks[i].title = Gtk::make_managed<Gtk::Entry>();
                page_[l].tracks[i].label = Gtk::make_managed<Gtk::Label>(buf);
                page_[l].tracks[i].hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);

                page_[l].tracks[i].hbox->append(*(page_[l].tracks[i].label));

                page_[l].table->attach(*(page_[l].tracks[i].hbox), 0, i + 4);
                page_[l].table->attach(*(page_[l].tracks[i].title), 2, i + 4);
                page_[l].table->attach(*(page_[l].tracks[i].performer), 1, i + 4);

                page_[l].tracks[i].title->signal_changed().connect(
                    [this, l] { recomputeEncoding(l); });
                page_[l].tracks[i].performer->signal_changed().connect(
                    [this, l] { recomputeEncoding(l); });
            }
        }
    }
    trackEntries_ = n;
}

void CdTextDialog::set(Glib::RefPtr<TocEdit> tocEdit)
{
    tocEdit_ = tocEdit;
    update(UPD_ALL);
}

void CdTextDialog::update(unsigned long level)
{
    std::string s(tocEdit_->filename());
    s += " - ";
    s += APP_NAME;
    if (tocEdit_->tocDirty())
        s += "(*)";
    set_title(s);

    if (level & UPD_TOC_DATA) updateTabLabels();
    if ((level & UPD_TOC_DATA) || (level & UPD_TRACK_DATA))
        importData();

    if (level & UPD_EDITABLE_STATE)
        applyButton_->set_sensitive(tocEdit_->editable());
}

void CdTextDialog::start()
{
    present();
    active_ = true;
}

void CdTextDialog::stop()
{
    hide();
    active_ = false;
}

void CdTextDialog::applyAction()
{
    if (!tocEdit_ || !tocEdit_->editable())
        return;

    exportData();
    guiUpdate();
}

void CdTextDialog::fillPerformerAction()
{
    int l = languages_->get_current_page();

    if (l >= 0 && l <= 7) {
        auto s = checkString(page_[l].performer->get_text());

        if (s.empty())
            return;

        for (int i = 0; i < trackEntries_; i++) {
            page_[l].tracks[i].performer->set_text(s);
        }
    }
}

void CdTextDialog::activatePerformerAction(int l)
{
    int val = page_[l].performerButton->get_active();

    for (int i = 0; i < trackEntries_; i++) {
        page_[l].tracks[i].performer->set_sensitive(val);
    }
}

void CdTextDialog::recomputeEncoding(int l)
{
    if (importing_ || l < 0 || l >= 8)
        return;

    // Concatenate all text fields for this language; if the whole blob
    // fits an encoding, every individual field does too.
    std::string all;
    all += page_[l].title->get_text().raw();
    all += '\n';
    all += page_[l].performer->get_text().raw();
    for (int i = 0; i < trackEntries_; i++) {
        all += '\n';
        all += page_[l].tracks[i].title->get_text().raw();
        all += '\n';
        all += page_[l].tracks[i].performer->get_text().raw();
    }

    bool fits[kEncodingCount];
    for (int e = 0; e < kEncodingCount; e++) {
        std::vector<u8> out;
        fits[e] = Util::from_utf8(all, out, kEncodings[e]);
    }

    int cur = page_[l].encoding->get_selected();
    if (cur < 0 || cur >= kEncodingCount)
        cur = kDefaultEncodingIndex;

    if (!fits[cur]) {
        // Current selection cannot represent the text — upgrade to the
        // smallest encoding that can. Never auto-downgrade.
        for (int e = 0; e < kEncodingCount; e++) {
            if (fits[e]) {
                page_[l].encoding->set_selected(e);
                // set_selected() re-enters this function via signal_changed;
                // the recursive call will refresh the warning state.
                return;
            }
        }
    }

    bool anyFits = fits[0] || fits[1] || fits[2];
    if (!anyFits) {
        page_[l].encodingWarning->set_markup(
            Glib::ustring("<span color=\"red\">") +
            Glib::Markup::escape_text(_("Text cannot be encoded")) +
            "</span>");
        page_[l].encodingWarning->set_visible(true);
    } else {
        page_[l].encodingWarning->set_visible(false);
    }
}

void CdTextDialog::importData()
{
    importing_ = true;
    const Toc *toc = tocEdit_->toc();
    int n = toc->nofTracks();
    adjustTableEntries(n);

    for (int l = 0; l < 8; l++) {
        auto titleItem = toc->getCdTextItem(0, l, CdTextItem::PackType::TITLE);
        page_[l].title->set_text(titleItem ? titleItem->getText() : "");

        auto perfItem = toc->getCdTextItem(0, l, CdTextItem::PackType::PERFORMER);
        page_[l].performer->set_text(perfItem ? perfItem->getText() : "");

        page_[l].encoding->set_selected(encodingToIndex(toc->cdTextEncoding(l)));

        for (int i = 0; i < n; i++) {
            auto tTitle = toc->getCdTextItem(i + 1, l, CdTextItem::PackType::TITLE);
            page_[l].tracks[i].title->set_text(tTitle ? tTitle->getText() : "");

            auto tPerf = toc->getCdTextItem(i + 1, l, CdTextItem::PackType::PERFORMER);
            page_[l].tracks[i].performer->set_text(tPerf ? tPerf->getText() : "");
        }
    }
    importing_ = false;

    for (int l = 0; l < 8; l++)
        recomputeEncoding(l);
}

void CdTextDialog::exportData()
{
    for (int l = 0; l < 8; l++) {
        setCdTextItem(CdTextItem::PackType::TITLE, 0, l, checkString(page_[l].title->get_text()));
        setCdTextItem(CdTextItem::PackType::PERFORMER, 0, l, checkString(page_[l].performer->get_text()));

        for (int i = 0; i < trackEntries_; i++) {
            setCdTextItem(CdTextItem::PackType::TITLE, i + 1, l, checkString(page_[l].tracks[i].title->get_text()));
            setCdTextItem(CdTextItem::PackType::PERFORMER, i + 1, l, checkString(page_[l].tracks[i].performer->get_text()));
        }

        int encIdx = page_[l].encoding->get_selected();
        if (encIdx >= 0 && encIdx < kEncodingCount)
            tocEdit_->setCdTextEncoding(l, kEncodings[encIdx]);
    }
}

void CdTextDialog::setCdTextItem(CdTextItem::PackType type, int trackNr, int l,
                                 const Glib::ustring& s)
{
    const Toc *toc = tocEdit_->toc();
    const CdTextItem *item = toc->getCdTextItem(trackNr, l, type);

    if (s != nullptr) {
        if (!item || item->getText() != s)
            tocEdit_->setCdTextItem(trackNr, type, l, s.c_str());
    } else if (item) {
        tocEdit_->setCdTextItem(trackNr, type, l, nullptr);
    }
}

Glib::ustring CdTextDialog::checkString(const Glib::ustring &src)
{
    auto str(src);
    if (str.empty()) return str;

    // Basic trim logic
    size_t first = str.find_first_not_of(" \t\n\r\f\v");
    if (first == std::string::npos)
        return str;
    size_t last = str.find_last_not_of(" \t\n\r\f\v");
    
    str = str.substr(first, (last - first + 1));
    return str;
}

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

#include "TrackInfoDialog.h"
#include "config.h"

#include <ctype.h>
#include <glibmm/i18n.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "CdTextItem.h"
#include "TextEdit.h"
#include "Toc.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "Track.h"
#include "guiUpdate.h"

Glib::RefPtr<TrackInfoDialog> TrackInfoDialog::create(Gtk::Window* parent)
{
    return Glib::make_refptr_for_instance<TrackInfoDialog>(new TrackInfoDialog(parent));
}


TrackInfoDialog::TrackInfoDialog(Gtk::Window* parent)
    : mainVBox_(Gtk::Orientation::VERTICAL, 10),
      contentBox_(Gtk::Orientation::VERTICAL, 10),
      copyFlag_(_("Copy")),
      preEmphasisFlag_(_("Pre Emphasis")),
      twoChannelAudio_(_("Two Channel Audio")),
      fourChannelAudio_(_("Four Channel Audio")),
      applyButton_(_("Apply")),
      closeButton_(_("Close")),
      parent_(parent)
{
    set_title(_("Track Info"));
    set_default_size(400, -1);
    set_hide_on_close(true);
    set_modal();
    set_transient_for(*parent_);

    tocEditView_ = nullptr;
    active_ = false;
    selectedTrack_ = 0;

    isrcCodeCountry_ = Gtk::make_managed<TextEdit>("XX");
    isrcCodeCountry_->set_max_length(2);
    
    isrcCodeOwner_ = Gtk::make_managed<TextEdit>("XXX");
    isrcCodeOwner_->set_max_length(3);

    isrcCodeYear_ = Gtk::make_managed<TextEdit>("00");
    isrcCodeYear_->set_max_length(2);

    isrcCodeSerial_ = Gtk::make_managed<TextEdit>("00000");
    isrcCodeSerial_->set_max_length(5);

    // Top Header: Track Number
    auto trackHeaderBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto trackLabel = Gtk::make_managed<Gtk::Label>(_("Track: "));
    trackHeaderBox->append(*trackLabel);
    trackHeaderBox->append(trackNr_);
    contentBox_.append(*trackHeaderBox);

    // Summary and Sub-channel horizontal container
    auto topRowBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);

    // 1. Summary Frame
    auto summaryFrame = Gtk::make_managed<Gtk::Frame>(_(" Summary "));
    auto summaryGrid = Gtk::make_managed<Gtk::Grid>();
    summaryGrid->set_row_spacing(5);
    summaryGrid->set_column_spacing(10);
    summaryGrid->set_margin(10);

    auto add_row = [&](const std::string& labelText, Gtk::Widget& widget, int row) {
        auto l = Gtk::make_managed<Gtk::Label>(labelText);
        l->set_halign(Gtk::Align::END);
        summaryGrid->attach(*l, 0, row);
        widget.set_halign(Gtk::Align::START);
        summaryGrid->attach(widget, 1, row);
    };

    add_row(_("Pre-Gap:"), pregapLen_, 0);
    add_row(_("Start:"), trackStart_, 1);
    add_row(_("End:"), trackEnd_, 2);
    add_row(_("Length:"), trackLen_, 3);
    add_row(_("Index Marks:"), indexMarks_, 4);
    summaryFrame->set_child(*summaryGrid);
    topRowBox->append(*summaryFrame);

    // 2. Sub-Channel Frame
    auto subChannelFrame = Gtk::make_managed<Gtk::Frame>(_(" Sub-Channel "));
    auto subVBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
    subVBox->set_margin(10);
    subVBox->append(copyFlag_);
    subVBox->append(preEmphasisFlag_);
    
    // Grouping Radios in GTK4
    fourChannelAudio_.set_group(twoChannelAudio_);
    subVBox->append(twoChannelAudio_);
    subVBox->append(fourChannelAudio_);

    auto isrcBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 2);
    isrcBox->append(*Gtk::make_managed<Gtk::Label>("ISRC: "));
    isrcBox->append(*isrcCodeCountry_);
    isrcBox->append(*Gtk::make_managed<Gtk::Label>("-"));
    isrcBox->append(*isrcCodeOwner_);
    isrcBox->append(*Gtk::make_managed<Gtk::Label>("-"));
    isrcBox->append(*isrcCodeYear_);
    isrcBox->append(*Gtk::make_managed<Gtk::Label>("-"));
    isrcBox->append(*isrcCodeSerial_);
    subVBox->append(*isrcBox);

    subChannelFrame->set_child(*subVBox);
    topRowBox->append(*subChannelFrame);
    contentBox_.append(*topRowBox);

    // 3. CD-TEXT Notebook
    auto cdTextFrame = Gtk::make_managed<Gtk::Frame>(_(" CD-TEXT "));
    auto notebook = Gtk::make_managed<Gtk::Notebook>();
    for (int i = 0; i < 8; i++) {
        setupCdTextPage(i);
        notebook->append_page(cdTextPages_[i].pageBox, cdTextPages_[i].label);
    }
    cdTextFrame->set_child(*notebook);
    contentBox_.append(*cdTextFrame);

    // Action Buttons
    auto actionBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    actionBox->set_halign(Gtk::Align::END);
    actionBox->set_margin(10);
    
    applyButton_.set_has_frame(true);
    applyButton_.signal_clicked().connect(sigc::mem_fun(*this, &TrackInfoDialog::applyAction));
    
    closeButton_.signal_clicked().connect(sigc::mem_fun(*this, &TrackInfoDialog::closeAction));

    actionBox->append(applyButton_);
    actionBox->append(closeButton_);

    // Final Assembly
    contentBox_.set_margin(10);
    mainVBox_.append(contentBox_);
    mainVBox_.append(*actionBox);
    set_child(mainVBox_);
}

void TrackInfoDialog::setupCdTextPage(int n)
{
    cdTextPages_[n].label.set_text(std::to_string(n));
    
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(10);
    grid->set_margin(10);

    auto add_entry = [&](const std::string& labelText, Gtk::Entry& entry, int row) {
        auto l = Gtk::make_managed<Gtk::Label>(labelText);
        l->set_halign(Gtk::Align::END);
        grid->attach(*l, 0, row);
        grid->attach(entry, 1, row);
        entry.set_expand(true);
    };

    add_entry(_("Title:"), cdTextPages_[n].title, 0);
    add_entry(_("Performer:"), cdTextPages_[n].performer, 1);
    add_entry(_("Songwriter:"), cdTextPages_[n].songwriter, 2);
    add_entry(_("Composer:"), cdTextPages_[n].composer, 3);
    add_entry(_("Arranger:"), cdTextPages_[n].arranger, 4);
    add_entry(_("Message:"), cdTextPages_[n].message, 5);
    add_entry(_("ISRC:"), cdTextPages_[n].isrc, 6);

    cdTextPages_[n].pageBox.append(*grid);
}

void TrackInfoDialog::start(TocEditView *view)
{
    if (active_) {
        present();
        return;
    }
    active_ = true;
    update(0xFFFFFFFF, view); // UPD_ALL
    present();
}

void TrackInfoDialog::stop()
{
    if (active_) {
        hide();
        active_ = false;
    }
}

void TrackInfoDialog::closeAction()
{
    stop();
}

void TrackInfoDialog::clear()
{
    trackNr_.set_text("");
    pregapLen_.set_text("");
    trackStart_.set_text("");
    trackEnd_.set_text("");
    trackLen_.set_text("");
    indexMarks_.set_text("");

    isrcCodeCountry_->set_text("");
    isrcCodeCountry_->set_editable(false);
    isrcCodeOwner_->set_text("");
    isrcCodeOwner_->set_editable(false);
    isrcCodeYear_->set_text("");
    isrcCodeYear_->set_editable(false);
    isrcCodeSerial_->set_text("");
    isrcCodeSerial_->set_editable(false);

    copyFlag_.set_sensitive(false);
    preEmphasisFlag_.set_sensitive(false);
    twoChannelAudio_.set_sensitive(false);
    fourChannelAudio_.set_sensitive(false);

    clearCdText();
}

void TrackInfoDialog::update(unsigned long level, TocEditView *view)
{
    const Toc *toc;

    if (!active_)
        return;

    tocEditView_ = view;

    if (view == NULL || !view->trackSelection(&selectedTrack_)) {
        selectedTrack_ = 0;
        applyButton_.set_sensitive(FALSE);
        clear();
        return;
    }

    std::string s(view->tocEdit()->filename());
    s += " - ";
    s += APP_NAME;
    if (view->tocEdit()->tocDirty())
        s += "(*)";
    set_title(s);

    if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
        toc = view->tocEdit()->toc();
        importData(toc, selectedTrack_);
        applyButton_.set_sensitive(view->tocEdit()->editable() ? TRUE : FALSE);
    } else if (level & UPD_EDITABLE_STATE) {
        applyButton_.set_sensitive(view->tocEdit()->editable() ? TRUE : FALSE);
    }
}

void TrackInfoDialog::clearCdText()
{
    int l;

    for (l = 0; l < 8; l++) {
        cdTextPages_[l].title.set_text("");
        cdTextPages_[l].title.set_editable(false);

        cdTextPages_[l].performer.set_text("");
        cdTextPages_[l].performer.set_editable(false);

        cdTextPages_[l].songwriter.set_text("");
        cdTextPages_[l].songwriter.set_editable(false);

        cdTextPages_[l].composer.set_text("");
        cdTextPages_[l].composer.set_editable(false);

        cdTextPages_[l].arranger.set_text("");
        cdTextPages_[l].arranger.set_editable(false);

        cdTextPages_[l].message.set_text("");
        cdTextPages_[l].message.set_editable(false);

        cdTextPages_[l].isrc.set_text("");
        cdTextPages_[l].isrc.set_editable(false);
    }
}

void TrackInfoDialog::applyAction()
{
    if (tocEditView_ == NULL || !tocEditView_->tocEdit()->editable() || selectedTrack_ == 0)
        return;

    exportData(tocEditView_->tocEdit(), selectedTrack_);

    guiUpdate(UPD_TRACK_DATA);
}

const char *TrackInfoDialog::checkString(const std::string &str)
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

void TrackInfoDialog::importCdText(const Toc *toc, int trackNr)
{
    int l;
    const CdTextItem *item;

    for (l = 0; l < 8; l++) {
        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::TITLE)) != NULL)
            cdTextPages_[l].title.set_text((const char *)item->data());
        else
            cdTextPages_[l].title.set_text("");
        cdTextPages_[l].title.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::PERFORMER)) != NULL)
            cdTextPages_[l].performer.set_text((const char *)item->data());
        else
            cdTextPages_[l].performer.set_text("");
        cdTextPages_[l].performer.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::SONGWRITER)) != NULL)
            cdTextPages_[l].songwriter.set_text((const char *)item->data());
        else
            cdTextPages_[l].songwriter.set_text("");
        cdTextPages_[l].songwriter.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::COMPOSER)) != NULL)
            cdTextPages_[l].composer.set_text((const char *)item->data());
        else
            cdTextPages_[l].composer.set_text("");
        cdTextPages_[l].composer.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::ARRANGER)) != NULL)
            cdTextPages_[l].arranger.set_text((const char *)item->data());
        else
            cdTextPages_[l].arranger.set_text("");
        cdTextPages_[l].arranger.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::MESSAGE)) != NULL)
            cdTextPages_[l].message.set_text((const char *)item->data());
        else
            cdTextPages_[l].message.set_text("");
        cdTextPages_[l].message.set_editable(true);

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::UPCEAN_ISRC)) != NULL)
            cdTextPages_[l].isrc.set_text((const char *)item->data());
        else
            cdTextPages_[l].isrc.set_text("");
        cdTextPages_[l].isrc.set_editable(true);
    }
}

void TrackInfoDialog::importData(const Toc *toc, int trackNr)
{
    char buf[50];
    TrackIterator itr(toc);
    Msf start, end;

    const Track *track = itr.find(trackNr, start, end);

    if (track == NULL) {
        clear();
        return;
    }

    snprintf(buf, sizeof(buf), "%d", trackNr);
    trackNr_.set_text(buf);

    snprintf(buf, sizeof(buf), "%3d:%02d:%02d", track->start().min(), track->start().sec(),
             track->start().frac());
    pregapLen_.set_text(buf);

    snprintf(buf, sizeof(buf), "%3d:%02d:%02d", start.min(), start.sec(), start.frac());
    trackStart_.set_text(buf);

    snprintf(buf, sizeof(buf), "%3d:%02d:%02d", end.min(), end.sec(), end.frac());
    trackEnd_.set_text(buf);

    Msf len(track->length() - track->start());
    snprintf(buf, sizeof(buf), "%3d:%02d:%02d", len.min(), len.sec(), len.frac());
    trackLen_.set_text(buf);

    snprintf(buf, sizeof(buf), "%3d", track->nofIndices());
    indexMarks_.set_text(buf);

    copyFlag_.set_sensitive(true);
    preEmphasisFlag_.set_sensitive(true);
    twoChannelAudio_.set_sensitive(true);
    fourChannelAudio_.set_sensitive(true);

    copyFlag_.set_active(track->copyPermitted());
    preEmphasisFlag_.set_active(track->preEmphasis());

    if (track->audioType() == 0)
        twoChannelAudio_.set_sensitive(true);
    else
        fourChannelAudio_.set_sensitive(true);

    if (track->isrcValid()) {
        snprintf(buf, sizeof(buf), "%c%c", track->isrcCountry(0), track->isrcCountry(1));
        isrcCodeCountry_->set_text(buf);

        snprintf(buf, sizeof(buf), "%c%c%c", track->isrcOwner(0), track->isrcOwner(1),
                 track->isrcOwner(2));
        isrcCodeOwner_->set_text(buf);

        snprintf(buf, sizeof(buf), "%c%c", '0' + track->isrcYear(0), '0' + track->isrcYear(1));
        isrcCodeYear_->set_text(buf);

        snprintf(buf, sizeof(buf), "%c%c%c%c%c", '0' + track->isrcSerial(0),
                 '0' + track->isrcSerial(1), '0' + track->isrcSerial(2), '0' + track->isrcSerial(3),
                 '0' + track->isrcSerial(4));
        isrcCodeSerial_->set_text(buf);
    } else {
        isrcCodeCountry_->set_text("");
        isrcCodeOwner_->set_text("");
        isrcCodeYear_->set_text("");
        isrcCodeSerial_->set_text("");
    }

    isrcCodeCountry_->set_editable(true);
    isrcCodeOwner_->set_editable(true);
    isrcCodeYear_->set_editable(true);
    isrcCodeSerial_->set_editable(true);

    importCdText(toc, trackNr);
}

void TrackInfoDialog::exportData(Glib::RefPtr<TocEdit> tocEdit, int trackNr)
{
    char buf[13];
    const char *s;
    Toc *toc = tocEdit->toc();
    Track *t = toc->getTrack(trackNr);
    int flag;

    if (t == NULL)
        return;

    flag = copyFlag_.get_sensitive() ? 1 : 0;
    if (t->copyPermitted() != flag)
        tocEdit->setTrackCopyFlag(trackNr, flag);

    if (t->type() == TrackData::AUDIO) {
        flag = preEmphasisFlag_.get_sensitive() ? 1 : 0;
        if (t->preEmphasis() != flag)
            tocEdit->setTrackPreEmphasisFlag(trackNr, flag);

        flag = twoChannelAudio_.get_sensitive() ? 0 : 1;
        if (t->audioType() != flag)
            tocEdit->setTrackAudioType(trackNr, flag);

        buf[0] = 0;
        if ((s = checkString(isrcCodeCountry_->get_text())) != NULL && strlen(s) == 2) {
            strcat(buf, s);
        }
        if ((s = checkString(isrcCodeOwner_->get_text())) != NULL && strlen(s) == 3) {
            strcat(buf, s);
        }
        if ((s = checkString(isrcCodeYear_->get_text())) != NULL && strlen(s) == 2) {
            strcat(buf, s);
        }
        if ((s = checkString(isrcCodeSerial_->get_text())) != NULL && strlen(s) == 5) {
            strcat(buf, s);
        }

        if (strlen(buf) == 0) {
            if (t->isrcValid())
                tocEdit->setTrackIsrcCode(trackNr, NULL);
        } else if (strlen(buf) == 12) {
            if ((s = t->isrc()) == NULL || strcmp(s, buf) != 0)
                tocEdit->setTrackIsrcCode(trackNr, buf);
        }
    }

    exportCdText(tocEdit, trackNr);
}

void TrackInfoDialog::exportCdText(Glib::RefPtr<TocEdit> tocEdit, int trackNr)
{
    int l;
    const char *s;
    const Toc *toc = tocEdit->toc();

    const CdTextItem *item;
    CdTextItem *newItem;

    for (l = 0; l < 8; l++) {
        // Title
        if ((s = checkString(cdTextPages_[l].title.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::TITLE, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::TITLE)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::TITLE, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::TITLE, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::TITLE, l, s);
        }

        delete newItem;

        // Performer
        if ((s = checkString(cdTextPages_[l].performer.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::PERFORMER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::PERFORMER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::PERFORMER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::PERFORMER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::PERFORMER, l, s);
        }

        delete newItem;

        // Songwriter
        if ((s = checkString(cdTextPages_[l].songwriter.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::SONGWRITER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::SONGWRITER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::SONGWRITER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::SONGWRITER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::SONGWRITER, l, s);
        }

        delete newItem;

        // Composer
        if ((s = checkString(cdTextPages_[l].composer.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::COMPOSER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::COMPOSER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::COMPOSER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::COMPOSER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::COMPOSER, l, s);
        }

        delete newItem;

        // Arranger
        if ((s = checkString(cdTextPages_[l].arranger.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::ARRANGER, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::ARRANGER)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::ARRANGER, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::ARRANGER, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::ARRANGER, l, s);
        }

        delete newItem;

        // Message
        if ((s = checkString(cdTextPages_[l].message.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::MESSAGE, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::MESSAGE)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::MESSAGE, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::MESSAGE, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::MESSAGE, l, s);
        }

        delete newItem;

        // Isrc
        if ((s = checkString(cdTextPages_[l].isrc.get_text())) != NULL) {
            newItem = new CdTextItem(CdTextItem::PackType::UPCEAN_ISRC, l);
            newItem->setText(s);
        } else
            newItem = NULL;

        if ((item = toc->getCdTextItem(trackNr, l, CdTextItem::PackType::UPCEAN_ISRC)) != NULL) {
            if (newItem == NULL)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::UPCEAN_ISRC, l, NULL);
            else if (*newItem != *item)
                tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::UPCEAN_ISRC, l, s);
        } else if (newItem != NULL) {
            tocEdit->setCdTextItem(trackNr, CdTextItem::PackType::UPCEAN_ISRC, l, s);
        }

        delete newItem;
    }
}

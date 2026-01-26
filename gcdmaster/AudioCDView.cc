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

#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <gtkmm.h>
#include <iostream>

#include "MessageBox.h"
#include "SampleDisplay.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "config.h"
#include "guiUpdate.h"
#include "util.h"
#include "xcdrdao.h"

#include "AddFileDialog.h"
#include "AddSilenceDialog.h"
#include "AudioCDProject.h"
#include "AudioCDView.h"
#include "Project.h"
#include "TrackInfoDialog.h"

AudioCDView::AudioCDView(AudioCDProject *project)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
{
    project_ = project;
    tocEditView_ = new TocEditView(project->tocEdit());

    addSilenceDialog_ = AddSilenceDialog::create(project_);
    addSilenceDialog_->signal_tocModified.connect(sigc::mem_fun(*this, &AudioCDView::update));

    addFileDialog_ = AddFileDialog::create(project_);

    setup_actions();

    trackInfoDialog_ = TrackInfoDialog::create(project_);

    // Drag and Drop setup
    auto drop_target = Gtk::DropTarget::create(GDK_TYPE_FILE_LIST, Gdk::DragAction::COPY);
    drop_target->signal_drop().connect(sigc::mem_fun(*this, &AudioCDView::on_drop), false);
    add_controller(drop_target);


    sampleDisplay_ = Gtk::make_managed<SampleDisplay>();
    sampleDisplay_->setTocEdit(project->tocEdit());
    sampleDisplay_->set_size_request(200, 200);
    sampleDisplay_->set_expand(true);
    append(*sampleDisplay_);

    scrollBar_ = Gtk::make_managed<Gtk::Scrollbar>(
        sampleDisplay_->get_adjustment(), Gtk::Orientation::HORIZONTAL);
    append(*scrollBar_);

    auto selectionInfoBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    gint entry_width = 90;

    markerPos_ = Gtk::make_managed<Gtk::Entry>();
    markerPos_->set_size_request(entry_width, -1);
    markerPos_->signal_activate().connect(sigc::mem_fun(*this, &AudioCDView::markerSet));

    cursorPos_ = Gtk::make_managed<Gtk::Label>();
    cursorPos_->set_size_request(entry_width, -1);

    selectionStartPos_ = Gtk::make_managed<Gtk::Entry>();
    selectionStartPos_->set_size_request(entry_width, -1);
    selectionStartPos_->signal_activate().connect(sigc::mem_fun(*this, &AudioCDView::selectionSet));

    selectionEndPos_ = Gtk::make_managed<Gtk::Entry>();
    selectionEndPos_->set_size_request(entry_width, -1);
    selectionEndPos_->signal_activate().connect(sigc::mem_fun(*this, &AudioCDView::selectionSet));

    auto label = Gtk::make_managed<Gtk::Label>(_("Cursor: "));
    selectionInfoBox->append(*label);
    selectionInfoBox->append(*cursorPos_);

    label = Gtk::make_managed<Gtk::Label>(_("Marker: "));
    selectionInfoBox->append(*label);
    selectionInfoBox->append(*markerPos_);

    label = Gtk::make_managed<Gtk::Label>(_("Selection: "));
    selectionInfoBox->append(*label);
    selectionInfoBox->append(*selectionStartPos_);

    label = Gtk::make_managed<Gtk::Label>(" - ");
    selectionInfoBox->append(*label);
    selectionInfoBox->append(*selectionEndPos_);

    selectionInfoBox->set_margin_bottom(2);
    append(*selectionInfoBox);

    
    setMode(SELECT);

    sampleDisplay_->markerSet.connect(sigc::mem_fun(*this, &AudioCDView::markerSetCallback));
    sampleDisplay_->selectionSet.connect(sigc::mem_fun(*this, &AudioCDView::selectionSetCallback));
    sampleDisplay_->selectionCleared.connect(
        sigc::mem_fun(*this, &AudioCDView::selectionClearedCallback));
    sampleDisplay_->cursorMoved.connect(sigc::mem_fun(*this, &AudioCDView::cursorMovedCallback));
    sampleDisplay_->trackMarkSelected.connect(
        sigc::mem_fun(*this, &AudioCDView::trackMarkSelectedCallback));
    sampleDisplay_->trackMarkMoved.connect(
        sigc::mem_fun(*this, &AudioCDView::trackMarkMovedCallback));
    sampleDisplay_->viewModified.connect(sigc::mem_fun(*this, &AudioCDView::viewModifiedCallback));

    tocEditView_->sampleViewFull();
}

void AudioCDView::setup_actions()
{
    project_->add_action("track-info", sigc::mem_fun(*this, &AudioCDView::trackInfo));
    project_->add_action("cut", sigc::mem_fun(*this, &AudioCDView::cutTrackData));
    project_->add_action("paste", sigc::mem_fun(*this, &AudioCDView::pasteTrackData));
    project_->add_action("select-all", sigc::mem_fun(*this, &AudioCDView::selectAll));
    project_->add_action("add-track-mark", sigc::mem_fun(*this, &AudioCDView::addTrackMark));
    project_->add_action("add-index-mark", sigc::mem_fun(*this, &AudioCDView::addIndexMark));
    project_->add_action("add-pre-gap", sigc::mem_fun(*this, &AudioCDView::addPregap));
    project_->add_action("remove-track-mark", sigc::mem_fun(*this, &AudioCDView::removeTrackMark));
    project_->add_action("append-track", sigc::mem_fun(*this, &AudioCDView::appendTrack));
    project_->add_action("append-file", sigc::mem_fun(*this, &AudioCDView::appendFile));
    project_->add_action("insert-file", sigc::mem_fun(*this, &AudioCDView::insertFile));
    project_->add_action("append-silence", sigc::mem_fun(*this, &AudioCDView::appendSilence));
    project_->add_action("insert-silence", sigc::mem_fun(*this, &AudioCDView::insertSilence));
}

void AudioCDView::update(unsigned long level)
{
    if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA)) {
        cursorPos_->set_text("");
    }

    if (level & UPD_TRACK_MARK_SEL) {
        int trackNr, indexNr;

        if (tocEditView_->trackSelection(&trackNr) && tocEditView_->indexSelection(&indexNr)) {
            sampleDisplay_->setSelectedTrackMarker(trackNr, indexNr);
        } else {
            sampleDisplay_->setSelectedTrackMarker(0, 0);
        }
    }

    if (level & UPD_SAMPLES) {
        unsigned long smin, smax;

        tocEditView_->sampleView(&smin, &smax);
        sampleDisplay_->updateToc(smin, smax);
    } else if (level & (UPD_TRACK_DATA | UPD_TRACK_MARK_SEL)) {
        sampleDisplay_->updateTrackMarks();
    }

    if (level & UPD_SAMPLE_MARKER) {
        unsigned long marker;

        if (tocEditView_->sampleMarker(&marker)) {
            markerPos_->set_text(sample2string(marker));
            sampleDisplay_->setMarker(marker);
        } else {
            markerPos_->set_text("");
            sampleDisplay_->clearMarker();
        }
    }

    if (level & UPD_SAMPLE_SEL) {
        unsigned long start, end;

        if (tocEditView_->sampleSelection(&start, &end)) {
            selectionStartPos_->set_text(sample2string(start));
            selectionEndPos_->set_text(sample2string(end));
            sampleDisplay_->setRegion(start, end);
        } else {
            selectionStartPos_->set_text("");
            selectionEndPos_->set_text("");
            sampleDisplay_->clearRegion();
        }
    }

    if (level & UPD_PLAY_STATUS) {
        switch (project_->playStatus()) {
        case AudioCDProject::PLAYING:
            sampleDisplay_->setCursor(1, project_->playPosition() - project_->getDelay());
            // FIXME: What about using a separate cursor for playing?
            cursorPos_->set_text(sample2string(project_->playPosition() - project_->getDelay()));
            break;
        case AudioCDProject::PAUSED:
            sampleDisplay_->setCursor(1, project_->playPosition() - project_->getDelay());
            // FIXME: What about using a separate cursor for playing?
            cursorPos_->set_text(sample2string(project_->playPosition() - project_->getDelay()));
            break;
        case AudioCDProject::STOPPED:
            sampleDisplay_->setCursor(0, 0);
            break;
        default:
            std::cerr << "invalid play status" << std::endl;
        }
    }

    trackInfoDialog_->update(level, tocEditView_);
    addSilenceDialog_->update(level, tocEditView_);
}

void AudioCDView::zoomIn()
{
    unsigned long start, end;

    if (tocEditView_->sampleSelection(&start, &end)) {
        if (tocEditView_->sampleView(start, end)) {
            update(UPD_SAMPLES);
        }
    }
}

void AudioCDView::zoomx2()
{
    unsigned long start, end, len, center;

    tocEditView_->sampleView(&start, &end);

    len = end - start + 1;
    center = start + len / 2;

    start = center - len / 4;
    end = center + len / 4;

    if (tocEditView_->sampleView(start, end)) {
        update(UPD_SAMPLES);
    }
}

void AudioCDView::zoomOut()
{
    unsigned long start, end, len, center;

    tocEditView_->sampleView(&start, &end);

    len = end - start + 1;
    center = start + len / 2;

    if (center > len)
        start = center - len;
    else
        start = 0;

    end = center + len;
    if (end >= tocEditView_->tocEdit()->toc()->length().samples())
        end = tocEditView_->tocEdit()->toc()->length().samples() - 1;

    if (tocEditView_->sampleView(start, end)) {
        update(UPD_SAMPLES);
    }
}

void AudioCDView::fullView()
{
    tocEditView_->sampleViewFull();
    update(UPD_SAMPLES);
}

int AudioCDView::getMarker(unsigned long *sample)
{
    if (tocEditView_->tocEdit()->lengthSample() == 0)
        return 0;

    if (sampleDisplay_->getMarker(sample) == 0) {
        project_->statusMessage(_("Please set marker."));
        return 0;
    }

    return 1;
}

void AudioCDView::trackMarkSelectedCallback(const Track *, int trackNr, int indexNr)
{
    tocEditView_->trackSelection(trackNr);
    tocEditView_->indexSelection(indexNr);
    update(UPD_TRACK_MARK_SEL);
}

// Called when the user clicks on the SampleDisplay
void AudioCDView::markerSetCallback(unsigned long sample)
{
    tocEditView_->sampleMarker(sample);
    update(UPD_SAMPLE_MARKER);
}

// Called when the user makes a selection on the SampleDisplay
void AudioCDView::selectionSetCallback(unsigned long start, unsigned long end)
{
    if (mode_ == ZOOM) {
        if (tocEditView_->sampleView(start, end)) {
            update(UPD_SAMPLES);
        }
    } else {
        tocEditView_->sampleSelect(start, end);
        update(UPD_SAMPLE_SEL);
    }
}

void AudioCDView::selectAll()
{
    tocEditView_->sampleSelectAll();
    update(UPD_SAMPLE_SEL);
}

void AudioCDView::selectionClearedCallback()
{
    if (mode_ != ZOOM) {
        if (tocEditView_->sampleSelectionClear()) {
            update(UPD_SAMPLE_SEL);
        }
    }
}

void AudioCDView::cursorMovedCallback(unsigned long pos)
{
    cursorPos_->set_text(sample2string(pos));
}

void AudioCDView::viewModifiedCallback(unsigned long start, unsigned long end)
{
    if (tocEditView_->sampleView(start, end)) {
        update(UPD_SAMPLES);
    }
}

void AudioCDView::setMode(Mode m)
{
    mode_ = m;
}

// Called when the user enters a value in the marker entry
void AudioCDView::markerSet()
{
    unsigned long s = string2sample(markerPos_->get_text().c_str());

    tocEditView_->sampleMarker(s);
    update(UPD_SAMPLE_MARKER);
}

// Called when the user enters a value in one of the two selection entries
void AudioCDView::selectionSet()
{
    unsigned long s1 = string2sample(selectionStartPos_->get_text().c_str());
    unsigned long s2 = string2sample(selectionEndPos_->get_text().c_str());

    tocEditView_->sampleSelect(s1, s2);
    update(UPD_SAMPLE_SEL);
}


bool AudioCDView::on_drop(const Glib::ValueBase& value, double, double)
{
    if (project_->playStatus() != AudioCDProject::STOPPED)
        return false;

    if (G_VALUE_HOLDS(value.gobj(), GDK_TYPE_FILE_LIST)) {
        auto files = Glib::Value<Glib::RefPtr<Gdk::FileList>>::get(value);
        auto gs_files = files->get_files();

        for (const auto& file : gs_files) {
            std::string fn = file->get_path();
            if (fn.empty()) continue;

            auto type = Util::fileExtension(fn.c_str());
            if (type == Util::FileExtension::WAV || type == Util::FileExtension::M3U
#ifdef HAVE_MP3_SUPPORT
                || type == Util::FileExtension::MP3
#endif
#ifdef HAVE_OGG_SUPPORT
                || type == Util::FileExtension::OGG
#endif
#ifdef HAVE_FLAC_SUPPORT
                || type == Util::FileExtension::FLAC
#endif
                ) {
                project_->appendTrack(fn.c_str());
            }
        }
        return true;
    }
    return false;
}

void AudioCDView::trackInfo()
{
    int track;

    if (tocEditView_->trackSelection(&track)) {
        trackInfoDialog_->start(tocEditView_);
    } else {
        MessageBox::message(*project_,_("Please select a track first"))
    }
}

void AudioCDView::tocBlockedMsg()
{
    MessageBox::message(*project_,
                        "Cannot perform operation",
                        { "Project is in read-only state." });
}

void AudioCDView::cutTrackData()
{
    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    switch (project_->tocEdit()->removeTrackData(tocEditView_)) {
    case 0:
        project_->statusMessage(_("Removed selected samples."));
        signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL | UPD_SAMPLE_MARKER |
                           UPD_SAMPLES);
        break;
    case 1:
        project_->statusMessage(_("Please select samples."));
        break;
    case 2:
        project_->statusMessage(_("Selected sample range crosses track "
                                  "boundaries."));
        break;
    }
}

void AudioCDView::pasteTrackData()
{
    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    switch (project_->tocEdit()->insertTrackData(tocEditView_)) {
    case 0:
        project_->statusMessage(_("Pasted samples."));
        signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_SEL);
        break;
    case 1:
        project_->statusMessage(_("No samples in scrap."));
        break;
    }
}

void AudioCDView::addTrackMark()
{
    unsigned long sample;

    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    if (getMarker(&sample)) {
        long lba;
        int snapped = snapSampleToBlock(sample, &lba);

        switch (project_->tocEdit()->addTrackMarker(lba)) {
        case 0:
            project_->statusMessage(_("Added track mark at %s%s."), Msf(lba).str(),
                                    snapped ? _(" (snapped to next block)") : "");
            signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
            break;

        case 2:
            project_->statusMessage(_("Cannot add track at this point."));
            break;

        case 3:
        case 4:
            project_->statusMessage(_("Resulting track would be shorter than "
                                      "4 seconds."));
            break;

        case 5:
            project_->statusMessage(_("Cannot modify a data track."));
            break;

        default:
            project_->statusMessage(_("Internal error in addTrackMark(), please "
                                      "report."));
            break;
        }
    }
}

void AudioCDView::addIndexMark()
{
    unsigned long sample;

    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    if (getMarker(&sample)) {
        long lba;
        int snapped = snapSampleToBlock(sample, &lba);

        switch (project_->tocEdit()->addIndexMarker(lba)) {
        case 0:
            project_->statusMessage(_("Added index mark at %s%s."), Msf(lba).str(),
                                    snapped ? _(" (snapped to next block)") : "");
            signal_tocModified(UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
            break;

        case 2:
            project_->statusMessage(_("Cannot add index at this point."));
            break;

        case 3:
            project_->statusMessage(_("Track has already 98 index marks."));
            break;

        default:
            project_->statusMessage(_("Internal error in addIndexMark(), "
                                      "please report."));
            break;
        }
    }
}

void AudioCDView::addPregap()
{
    unsigned long sample;

    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    if (getMarker(&sample)) {
        long lba;
        int snapped = snapSampleToBlock(sample, &lba);

        switch (project_->tocEdit()->addPregap(lba)) {
        case 0:
            project_->statusMessage(_("Added pre-gap mark at %s%s."), Msf(lba).str(),
                                    snapped ? _(" (snapped to next block)") : "");
            signal_tocModified(UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
            break;

        case 2:
            project_->statusMessage(_("Cannot add pre-gap at this point."));
            break;

        case 3:
            project_->statusMessage(_("Track would be shorter than 4 seconds."));
            break;

        case 4:
            project_->statusMessage(_("Cannot modify a data track."));
            break;

        default:
            project_->statusMessage(_("Internal error in addPregap(), "
                                      "please report."));
            break;
        }
    }
}

void AudioCDView::removeTrackMark()
{
    int trackNr;
    int indexNr;

    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    if (tocEditView_->trackSelection(&trackNr) && tocEditView_->indexSelection(&indexNr)) {
        switch (project_->tocEdit()->removeTrackMarker(trackNr, indexNr)) {
        case 0:
            project_->statusMessage(_("Removed track/index marker."));
            signal_tocModified(UPD_TOC_DATA | UPD_TRACK_DATA | UPD_SAMPLE_MARKER);
            break;
        case 1:
            project_->statusMessage(_("Cannot remove first track."));
            break;
        case 3:
            project_->statusMessage(_("Cannot modify a data track."));
            break;
        default:
            project_->statusMessage(_("Internal error in removeTrackMark(), "
                                      "please report."));
            break;
        }
    } else {
        project_->statusMessage(_("Please select a track/index mark."));
    }
}

int AudioCDView::snapSampleToBlock(unsigned long sample, long *block)
{
    unsigned long rest = sample % SAMPLES_PER_BLOCK;

    *block = sample / SAMPLES_PER_BLOCK;

    if (rest == 0)
        return 0;

    if (rest > SAMPLES_PER_BLOCK / 2)
        *block += 1;

    return 1;
}

void AudioCDView::trackMarkMovedCallback(const Track *, int trackNr, int indexNr,
                                         unsigned long sample)
{
    if (!project_->tocEdit()->editable()) {
        project_->tocBlockedMsg();
        return;
    }

    long lba;
    int snapped = snapSampleToBlock(sample, &lba);

    switch (project_->tocEdit()->moveTrackMarker(trackNr, indexNr, lba)) {
    case 0:
        project_->statusMessage(_("Moved track marker to %s%s."), Msf(lba).str(),
                                snapped ? _(" (snapped to next block)") : "");
        break;

    case 6:
        project_->statusMessage(_("Cannot modify a data track."));
        break;
    default:
        project_->statusMessage(_("Illegal track marker position."));
        break;
    }

    tocEditView_->trackSelection(trackNr);
    tocEditView_->indexSelection(indexNr);

    update(UPD_TRACK_MARK_SEL);
}

void AudioCDView::appendTrack()
{
    addFileDialog_->start(AddFileDialog::M_APPEND_TRACK);
}

void AudioCDView::appendFile()
{
    addFileDialog_->start(AddFileDialog::M_APPEND_FILE);
}

void AudioCDView::insertFile()
{
    addFileDialog_->start(AddFileDialog::M_INSERT_FILE);
}

void AudioCDView::appendSilence()
{
    addSilenceDialog_->start(AddSilenceDialog::M_APPEND, tocEditView_);
}

void AudioCDView::insertSilence()
{
    addSilenceDialog_->start(AddSilenceDialog::M_INSERT, tocEditView_);
}

const char *AudioCDView::sample2string(unsigned long sample)
{
    static char buf[50];

    unsigned long min = sample / (60 * 44100);
    sample %= 60 * 44100;

    unsigned long sec = sample / 44100;
    sample %= 44100;

    unsigned long frame = sample / 588;
    sample %= 588;

    snprintf(buf, sizeof(buf), "%2lu:%02lu:%02lu.%03lu", min, sec, frame, sample);

    return buf;
}

unsigned long AudioCDView::string2sample(const char *str)
{
    int m = 0;
    int s = 0;
    int f = 0;
    int n = 0;

    sscanf(str, "%d:%d:%d.%d", &m, &s, &f, &n);

    if (m < 0)
        m = 0;

    if (s < 0 || s > 59)
        s = 0;

    if (f < 0 || f > 74)
        f = 0;

    if (n < 0 || n > 587)
        n = 0;

    return Msf(m, s, f).samples() + n;
}

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

#include "config.h"
#include "log.h"

#include <assert.h>
#include <cstring>
#include <glibmm/i18n.h>
#include <gtkmm.h>
#include <filesystem>
#include <iostream>

#include "AudioCDProject.h"
#include "AudioCDView.h"
#include "CdTextDialog.h"
#include "Icons.h"
#include "MessageBox.h"
#include "RecordTocDialog.h"
#include "SoundIF.h"
#include "Toc.h"
#include "TocEdit.h"
#include "TocEditView.h"
#include "TocInfoDialog.h"
#include "gcdmaster.h"
#include "guiUpdate.h"
#include "util.h"
#include "xcdrdao.h"

AudioCDProject* AudioCDProject::create(Glib::RefPtr<Gtk::Builder> builder,
				       const Glib::ustring& path)
{
    static int nextNumber = 0;
    builder->add_from_resource("/org/gnome/gcdmaster/ui/window.ui");
    builder->add_from_resource("/org/gnome/gcdmaster/ui/gears_audio_menu.ui");

    auto window = Gtk::Builder::get_widget_derived<AudioCDProject>(builder, "app_window");
    if (!window)
	throw std::runtime_error("app_window resource error");

    window->setup(nextNumber++, path);
    return window;
}

AudioCDProject::AudioCDProject(BaseObjectType* cobject,
 			      const Glib::RefPtr<Gtk::Builder>& builder) :
    GCDWindow(cobject), builder_(builder)
{
    // Load toolbar.
    auto tbar = builder->get_object<Gtk::Box>("audio-toolbar");
    auto vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
    vbox->append(*tbar);

    // Add actions
    add_action("save", sigc::mem_fun(*this, &AudioCDProject::saveProject));
    add_action("save-as", sigc::mem_fun(*this, &AudioCDProject::saveAsProject));
    add_action("project-info", sigc::mem_fun(*this, &AudioCDProject::projectInfo));
    add_action("cdtext", sigc::mem_fun(*this, &AudioCDProject::cdTextDialog));
    add_action("record", sigc::mem_fun(*this, &AudioCDProject::recordToc2CD));
    play_action_ =
        add_action("play", sigc::mem_fun(*this, &AudioCDProject::on_play_clicked));
    stop_action_ =
        add_action("stop", sigc::mem_fun(*this, &AudioCDProject::on_stop_clicked));
    pause_action_ =
        add_action("pause", sigc::mem_fun(*this, &AudioCDProject::on_pause_clicked));

    add_action("select-mode", sigc::mem_fun(*this, &AudioCDProject::on_select_clicked));
    add_action("zoom-mode", sigc::mem_fun(*this, &AudioCDProject::on_zoom_clicked));
    add_action("zoom-in", sigc::mem_fun(*this, &AudioCDProject::on_zoom_in_clicked));
    add_action("zoom-out", sigc::mem_fun(*this, &AudioCDProject::on_zoom_out_clicked));
    add_action("zoom-fit", sigc::mem_fun(*this, &AudioCDProject::on_zoom_fit_clicked));

    // Window menu
    auto gears = builder->get_object<Gtk::MenuButton>("gears");
    auto wmenu = builder->get_object<Gio::MenuModel>("audiocd-menu");
    if (!wmenu)
        throw std::runtime_error("Menu resource not found");
    gears->set_menu_model(wmenu);

    tocInfoDialog_ = NULL;
    cdTextDialog_ = NULL;
    soundInterface_ = NULL;
    buttonPlay_ = NULL;
    buttonStop_ = NULL;
    buttonPause_ = NULL;
    audioCDView_ = NULL;

    set_child(*vbox);

    playStatus_ = STOPPED;
    playBurst_ = 588 * 10;
    playBuffer_ = new Sample[playBurst_];

    // Setup widgets on status bar.
    progressbar_ = builder_->get_object<Gtk::ProgressBar>("progress-bar");
    progressbar_->set_pulse_step(0.01);
    
    statusbar_ = builder_->get_object<Gtk::Label>("status-bar-label");

    stopButton_ = builder_->get_object<Gtk::Button>("gtk-stop");
    stopButton_->signal_clicked().connect(
        sigc::mem_fun(*this, &AudioCDProject::on_cancel_clicked));
}

void AudioCDProject::setup(int number, const Glib::ustring& path)
{
    projectNumber_ = number;
    if (path.empty()) {
        tocEdit_ = Glib::make_refptr_for_instance<TocEdit>(new TocEdit(NULL, NULL));
    } else {
	auto ext = Util::fileExtension(path);
 
	switch (ext) {
	case Util::FileExtension::TOC:
	case Util::FileExtension::CUE:
	    tocEdit_ = Glib::make_refptr_for_instance<TocEdit>(new TocEdit(NULL, NULL));
	    if (tocEdit_->readToc(path.c_str()) != 0)
		throw std::runtime_error("Could not parse TOC file");
	    break;
	default:
	    throw std::runtime_error("Unrecognized format");
	}

	if (ext == Util::FileExtension::TOC) {
	    new_ = false;
	    std::ostringstream oss;
	    oss << "unnamed-" << projectNumber_ << ".toc";
	    tocEdit_->filename(oss.str().c_str());
	}
    }

    if (tocEdit_) {
        // Connect TocEdit signals to us.
        tocEdit_->signalStatusMessage.connect(sigc::mem_fun(*this, &AudioCDProject::status));
        tocEdit_->signalProgressFraction.connect(sigc::mem_fun(*this, &AudioCDProject::progress));
        tocEdit_->signalSpinner.connect(sigc::mem_fun(*this, &AudioCDProject::spin));
        tocEdit_->signalFullView.connect(sigc::mem_fun(*this, &AudioCDProject::fullView));
        tocEdit_->signalSampleSelection.connect(sigc::mem_fun(*this, &AudioCDProject::sampleSelect));
        tocEdit_->signalCancelEnable.connect(sigc::mem_fun(*this, &AudioCDProject::cancelEnable));
        tocEdit_->signalError.connect(sigc::mem_fun(*this, &AudioCDProject::errorDialog));

        tocEdit_->signalProgressPulse.connect(
            sigc::mem_fun(*progressbar_, &Gtk::ProgressBar::pulse));
        signalCancelClicked.connect(sigc::mem_fun(*tocEdit_,
                                                  &TocEdit::queueAbort));

        if (tocEdit_->isQueueActive())
            cancelEnable(true);
    }

    // audioCDView_ = new AudioCDView(this);
    // audioCDView_->set_expand(true);
    // auto vbox = dynamic_cast<Gtk::Box*>(get_first_child());
    // vbox->append(*audioCDView_);
    // audioCDView_->tocEditView()->sampleViewFull();
    updateWindowTitle();
    guiUpdate(UPD_ALL);
    present();
}

AudioCDProject::~AudioCDProject()
{
    if (playBuffer_)
	delete playBuffer_;
}

void AudioCDProject::updateWindowTitle()
{
    std::string s(tocEdit_->filename());
    s += " - ";
    if (tocEdit_->tocDirty())
        s += " (*)";

    set_title(s);
}

void AudioCDProject::status(const char *msg)
{
    statusMessage(msg);
}

void AudioCDProject::errorDialog(const char *msg)
{
    ErrorBox::message(*this, msg);
}

void AudioCDProject::progress(double val)
{
    progressbar_->set_fraction(val);
}

void AudioCDProject::spin(bool val)
{
    if (val) {
        auto busy = Gdk::Cursor::create("wait");
        set_cursor(busy);
    } else {
        set_cursor();
    }
}

void AudioCDProject::fullView()
{
//    if (audioCDView_)
//        audioCDView_->fullView();
}

void AudioCDProject::sampleSelect(unsigned long start, unsigned long len)
{
//    audioCDView_->tocEditView()->sampleSelect(start, len);
}

void AudioCDProject::cancelEnable(bool enable)
{
    if (stopButton_)
        stopButton_->set_sensitive(enable);
}

void AudioCDProject::closeProject()
{
    if (tocEdit_->tocDirty()) {

        Glib::ustring message = "Project ";
        message += tocEdit_->filename();
        message += " not saved.";

        auto box = Ask2Box::create(*this, message, 1,
                                   { _("Are you sure you want to close it ?") });

        box->dialogDone.connect([this](int result) {
            if (result != 0)
                return;

            if (tocEdit_ && tocEdit_->isQueueActive()) {
                tocEdit_->queueAbort();
            }
            
            if (playStatus_ == PLAYING || playStatus_ == PAUSED) {
                playStop();
            }
        });
    }
}

void AudioCDProject::saveProject()
{
    if (new_) {
        saveAsProject();
        return;
    }

    if (tocEdit_->saveToc() == 0) {
        statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename());
        guiUpdate(UPD_TOC_DIRTY);
    } else {
        std::string s(_("Cannot save toc to \""));
        s += tocEdit_->filename();
        s += "\":";

        errorDialog(s.c_str());
    }
}

void AudioCDProject::saveAsProject()
{
}

void AudioCDProject::recordToc2CD()
{
    // if (!recordTocDialog_)
    //     recordTocDialog_ = Glib::make_refptr_for_instance<RecordTocDialog>(
    //         new RecordTocDialog(tocEdit_));

    // recordTocDialog_->start(this);
}

void AudioCDProject::projectInfo()
{
    // if (!tocInfoDialog_)
    //     tocInfoDialog_ = Glib::make_refptr_for_instance<TocInfoDialog>(
    //         new TocInfoDialog(this));

    // tocInfoDialog_->start(tocEdit_.get());
}

void AudioCDProject::cdTextDialog()
{
    // if (cdTextDialog_ == 0)
    //     cdTextDialog_ = Glib::make_refptr_for_instance<CdTextDialog>(
    //         new CdTextDialog());

    // cdTextDialog_->start(tocEdit_.get());
}

void AudioCDProject::update(unsigned long level)
{
    // FIXME: Here we should update the menus and the icons
    //        this is, enabled/disabled.

    level |= tocEdit_->updateLevel();

    if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA))
        updateWindowTitle();

//    audioCDView_->update(level);

    // if (tocInfoDialog_)
    //     tocInfoDialog_->update(level, tocEdit_.get());

    // if (cdTextDialog_ != 0)
    //     cdTextDialog_->update(level, tocEdit_.get());
    // if (recordTocDialog_ != 0)
    //     recordTocDialog_->update(level);

    if (level & UPD_PLAY_STATUS) {
        bool sensitivity[3][3] = {
            // PLAY  PAUSE STOP
            {false, true, true}, // Playing
            {true, true, true},  // Paused
            {true, false, false} // Stopped
        };

        play_action_ ->set_enabled(sensitivity[playStatus_][0]);
        pause_action_->set_enabled(sensitivity[playStatus_][1]);
        stop_action_ ->set_enabled(sensitivity[playStatus_][2]);
    }

     if (level & UPD_EDITABLE_STATE) {
        bool editable = tocEdit_->editable();
        play_action_->set_enabled(editable);
    }

    stopButton_->set_sensitive(tocEdit_->isQueueActive());
}

void AudioCDProject::playStart()
{
    unsigned long start, end;

    // If we're in paused mode, resume playing.
    if (playStatus_ == PAUSED) {
        playStatus_ = PLAYING;
        Glib::signal_idle().connect(sigc::mem_fun(*this, &AudioCDProject::playCallback));
        return;
    } else if (playStatus_ == PLAYING) {
        return;
    }

//     if (audioCDView_ && audioCDView_->tocEditView()) {
//        if (!audioCDView_->tocEditView()->sampleSelection(&start, &end))
//            audioCDView_->tocEditView()->sampleView(&start, &end);
//
//        playStart(start, end);
//    }
}

void AudioCDProject::playStart(unsigned long start, unsigned long end)
{
    unsigned long level = 0;

    if (playStatus_ == PLAYING)
        return;

    if (tocEdit_->lengthSample() == 0) {
        guiUpdate(UPD_PLAY_STATUS);
        return;
    }

    if (soundInterface_ == NULL) {
        soundInterface_ = new SoundIF;
        if (soundInterface_->init() != 0) {
            delete soundInterface_;
            soundInterface_ = NULL;
            guiUpdate(UPD_PLAY_STATUS);
            statusMessage(_("WARNING: Cannot open \"/dev/dsp\""));
            return;
        }
    }

    if (soundInterface_->start() != 0) {
        statusMessage(_("WARNING: Cannot open sound device"));
        guiUpdate(UPD_PLAY_STATUS);
        return;
    }

    tocReader.init(tocEdit_->toc());
    if (tocReader.openData() != 0) {
        tocReader.init(NULL);
        soundInterface_->end();
        guiUpdate(UPD_PLAY_STATUS);
        return;
    }

    if (tocReader.seekSample(start) != 0) {
        tocReader.init(NULL);
        soundInterface_->end();
        guiUpdate(UPD_PLAY_STATUS);
        return;
    }

    playLength_ = end - start + 1;
    playPosition_ = start;
    playStatus_ = PLAYING;
    playAbort_ = false;

    level |= UPD_PLAY_STATUS;

    // FIXME: Selection / Zooming does not depend
    //        on the Child, but the View.
    //        we should have different blocks!
    tocEdit_->blockEdit();

    guiUpdate(level);

    Glib::signal_idle().connect(sigc::mem_fun(*this, &AudioCDProject::playCallback));
}

void AudioCDProject::playPause()
{
    if (playStatus_ == PAUSED) {
        playStatus_ = PLAYING;
        Glib::signal_idle().connect(sigc::mem_fun(*this, &AudioCDProject::playCallback));
    } else if (playStatus_ == PLAYING) {
        playStatus_ = PAUSED;
    }
}

void AudioCDProject::playStop()
{
    if (playStatus() == PAUSED) {
        soundInterface_->end();
        tocReader.init(NULL);
        playStatus_ = STOPPED;
        tocEdit_->unblockEdit();
        playStatus_ = STOPPED;
        guiUpdate(UPD_PLAY_STATUS | UPD_EDITABLE_STATE);
    } else {
        playAbort_ = true;
    }
}

bool AudioCDProject::playCallback()
{
    unsigned long level = 0;

    long len = playLength_ > playBurst_ ? playBurst_ : playLength_;

    if (playStatus_ == PAUSED) {
        level |= UPD_PLAY_STATUS;
        guiUpdate(level);
        return false; // remove idle handler
    }

    if (tocReader.readSamples(playBuffer_, len) != len ||
        soundInterface_->play(playBuffer_, len) != 0) {
        soundInterface_->end();
        tocReader.init(NULL);
        playStatus_ = STOPPED;
        level |= UPD_PLAY_STATUS;
        tocEdit_->unblockEdit();
        guiUpdate(level);
        return false; // remove idle handler
    }

    playLength_ -= len;
    playPosition_ += len;

    unsigned long delay = soundInterface_->getDelay();

    if (delay <= playPosition_)
        level |= UPD_PLAY_STATUS;

    if (len == 0 || playAbort_) {
        soundInterface_->end();
        tocReader.init(NULL);
        playStatus_ = STOPPED;
        level |= UPD_PLAY_STATUS | UPD_EDITABLE_STATE;
        tocEdit_->unblockEdit();
        guiUpdate(level);
        return false; // remove idle handler
    } else {
        guiUpdate(level);
        return true; // keep idle handler
    }
}

unsigned long AudioCDProject::playPosition()
{
    return playPosition_;
}

unsigned long AudioCDProject::getDelay()
{
    return soundInterface_->getDelay();
}

void AudioCDProject::on_play_clicked()
{
    playStart();
}

void AudioCDProject::on_stop_clicked()
{
    playStop();
}

void AudioCDProject::on_pause_clicked()
{
    playPause();
}

void AudioCDProject::on_zoom_in_clicked()
{
//    if (audioCDView_)
//        audioCDView_->zoomx2();
}

void AudioCDProject::on_zoom_out_clicked()
{
//    if (audioCDView_)
//        audioCDView_->zoomOut();
}

void AudioCDProject::on_zoom_fit_clicked()
{
//    if (audioCDView_)
//        audioCDView_->fullView();
}

void AudioCDProject::on_zoom_clicked()
{
//    if (audioCDView_)
//        audioCDView_->setMode(AudioCDView::ZOOM);
}

void AudioCDProject::on_select_clicked()
{
//    if (audioCDView_)
//        audioCDView_->setMode(AudioCDView::SELECT);
}

void AudioCDProject::on_cancel_clicked()
{
    signalCancelClicked();
}

bool AudioCDProject::appendTrack(const char *file)
{
    auto type = Util::fileExtension(file);

    switch (type) {

    case Util::FileExtension::M3U: {
        std::list<std::string> list;
        if (parseM3u(file, list)) {
            std::list<std::string>::iterator i = list.begin();
            for (; i != list.end(); i++) {
                tocEdit_->queueAppendTrack((*i).c_str());
            }
        } else {
            std::string msg = "Could not read M3U file \"";
            msg += file;
            msg += "\"";
            errorDialog(msg.c_str());
            return false;
        }
        break;
    }

    default:
        tocEdit_->queueAppendTrack(file);
    }

    return true;
}

bool AudioCDProject::appendTracks(std::list<std::string> &files)
{
    std::list<std::string>::iterator i = files.begin();
    for (; i != files.end(); i++) {
        tocEdit_->queueAppendTrack((*i).c_str());
    }
    return true;
}

bool AudioCDProject::appendFiles(std::list<std::string> &files)
{
    std::list<std::string>::iterator i = files.begin();
    for (; i != files.end(); i++) {
        tocEdit_->queueAppendFile((*i).c_str());
    }
    return true;
}

bool AudioCDProject::insertFiles(std::list<std::string> &files)
{
    // unsigned long pos;

    // TocEditView *view = audioCDView_->tocEditView();
    // if (!view)
    //     return false;
    // if (!view->sampleMarker(&pos))
    //     pos = 0;

    // std::list<std::string>::iterator i = files.end();
    // do {
    //     i--;
    //     tocEdit_->queueInsertFile((*i).c_str(), pos);
    // } while (i != files.begin());

    return true;
}

void AudioCDProject::statusMessage(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char *s = g_strdup_vprintf(fmt, args);

    statusbar_->set_text(s);

    free(s);

    va_end(args);
}


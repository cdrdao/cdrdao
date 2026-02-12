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

#include "CdrdaoDirect.h"
#include "AudioCDProject.h"
#include "AudioCDView.h"
#include "AudioCDRead.h"
#include "CdTextDialog.h"
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
#include "CddbWindow.h"

AudioCDProject* AudioCDProject::create(Glib::RefPtr<Gtk::Builder> builder,
				       const Glib::ustring& path)
{
    static int nextNumber = 0;
    builder->add_from_resource("/org/gnome/gcdmaster/window.ui");
    builder->add_from_resource("/org/gnome/gcdmaster/gears_audio_menu.ui");
    builder->add_from_resource("/org/gnome/gcdmaster/app_menu.ui");

    auto cssProvider = Gtk::CssProvider::create();
    cssProvider->load_from_resource("/org/gnome/gcdmaster/gcdmaster.css");
    
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
    // Add actions
    save_action_ = add_action("save", sigc::mem_fun(*this, &AudioCDProject::saveProject));
    save_as_action_ = add_action("save-as", sigc::mem_fun(*this, &AudioCDProject::saveAsProject));
    add_action("project-info", sigc::mem_fun(*this, &AudioCDProject::projectInfo));
    add_action("cdtext", sigc::mem_fun(*this, &AudioCDProject::cdTextDialog));
    add_action("cddb", sigc::mem_fun(*this, &AudioCDProject::cddbQuery));
    toc_read_action_ = add_action("toc-read", sigc::mem_fun(*this, &AudioCDProject::tocRead));
    cd_read_action_ = add_action("cd-read", sigc::mem_fun(*this, &AudioCDProject::CDRead));
    add_action("record", sigc::mem_fun(*this, &AudioCDProject::recordToc2CD));
    play_action_ =
        add_action("play", sigc::mem_fun(*this, &AudioCDProject::on_play_clicked));
    stop_action_ =
        add_action("stop", sigc::mem_fun(*this, &AudioCDProject::on_stop_clicked));
    pause_action_ =
        add_action("pause", sigc::mem_fun(*this, &AudioCDProject::on_pause_clicked));

    add_action("zoom-in", sigc::mem_fun(*this, &AudioCDProject::on_zoom_in_clicked));
    add_action("zoom-out", sigc::mem_fun(*this, &AudioCDProject::on_zoom_out_clicked));
    add_action("zoom-fit", sigc::mem_fun(*this, &AudioCDProject::on_zoom_fit_clicked));

    // Radio Buttons nonsense. Just doesn't work from UI, requires
    // some ridiculously complicated hoop jumping.  Action needs to be
    // added BEFORE the UI is loaded, which is impossible since we're
    // using get_widget_derived().
    auto action = Gio::SimpleAction::create_radio_string("drag-mode", "select");
    action->signal_change_state().connect([action, this](const Glib::VariantBase& state) {
        auto str_state = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring>>(state);
        if (str_state.get() == "select")
            this->audioCDView_->mode(AudioCDView::SELECT);
        else
            this->audioCDView_->mode(AudioCDView::ZOOM);
        action->set_state(state);
    });
    add_action(action);

    auto audio_toolbar = builder_->get_object<Gtk::Box>("audio-toolbar");
    auto sep_mode = builder->get_object<Gtk::Separator>("sep-mode");
    auto rbt1 = Gtk::make_managed<Gtk::ToggleButton>();
    rbt1->set_active(true);
    rbt1->set_icon_name("zoom-fit-best-symbolic");
    rbt1->set_action_name("win.drag-mode");
    rbt1->set_action_target_value(Glib::Variant<Glib::ustring>::create("zoom"));
    audio_toolbar->insert_child_after(*rbt1, *sep_mode);
    auto rbt2 = Gtk::make_managed<Gtk::ToggleButton>("Z");
    rbt2->set_active(false);
    rbt2->set_group(*rbt1);
    rbt2->set_icon_name("edit-select-all-symbolic");
    rbt2->set_action_name("win.drag-mode");
    rbt2->set_action_target_value(Glib::Variant<Glib::ustring>::create("select"));
    audio_toolbar->insert_child_after(*rbt2, *rbt1);

    // App menu
    auto app_menu_button = builder->get_object<Gtk::MenuButton>("app-menu-button");
    auto app_menu = builder->get_object<Gio::MenuModel>("app-menu");
    app_menu_button->set_menu_model(app_menu);

    {
	// Window menus
	auto gears = builder->get_object<Gtk::MenuButton>("gears");
	auto wmenu = builder->get_object<Gio::MenuModel>("audiocd-menu");
	auto disc_ops = builder->get_object<Gtk::MenuButton>("disc-ops");
	auto disc_ops_menu = builder->get_object<Gio::MenuModel>("disc-ops-menu");
	if (!(gears && wmenu && disc_ops && disc_ops_menu))
	    throw std::runtime_error("Missing UI resources");
	gears->set_menu_model(wmenu);
	disc_ops->set_menu_model(disc_ops_menu);
    }

    headerBarTitle_ = builder->get_object<Gtk::Label>("header-bar-title");
    selectToggle_ = builder->get_object<Gtk::ToggleButton>("select-mode");
    zoomToggle_ = builder->get_object<Gtk::ToggleButton>("zoom-mode");

    soundInterface_ = nullptr;
    buttonPlay_ = nullptr;
    buttonStop_ = nullptr;
    buttonPause_ = nullptr;
    audioCDView_ = nullptr;

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

    soundInterface_ = SoundIF::create();
    if (!soundInterface_) {
        play_action_ ->set_enabled(false);
        pause_action_->set_enabled(false);
        stop_action_ ->set_enabled(false);
    }
}

void AudioCDProject::loadNewToc(const std::string& tocfile, bool anonymous)
{
     if (tocEdit_->readToc(tocfile.c_str()) != 0) {
	errorDialog("Unable to parse audio CD TOC content");
	return;
    }
     if (anonymous) {
	 std::ostringstream oss;
	 oss << "unnamed-" << projectNumber_ << ".toc";
	 tocEdit_->filename(oss.str().c_str());
	 new_ = true;
	 tocEdit_->tocDirty(true);
     }
    updateWindowTitle();
    guiUpdate(UPD_ALL);
    fullView();
}

void AudioCDProject::setup(int number, const Glib::ustring& path)
{
    projectNumber_ = number;
    if (path.empty()) {
        tocEdit_ = Glib::make_refptr_for_instance<TocEdit>(new TocEdit(nullptr, ""));
	set_size_request(600, 200);
        std::ostringstream oss;
        oss << "unnamed-" << projectNumber_ << ".toc";
        tocEdit_->filename(oss.str().c_str());
        new_ = true;
    } else {
	auto ext = Util::fileExtension(path);
 
	switch (ext) {
	case Util::FileExtension::TOC:
	case Util::FileExtension::CUE:
	    tocEdit_ = Glib::make_refptr_for_instance<TocEdit>(new TocEdit(nullptr, ""));
	    if (tocEdit_->readToc(path.c_str()) != 0)
		throw std::runtime_error("Could not parse TOC file");
	    break;
	default:
	    throw std::runtime_error("Unrecognized format");
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

    audioCDView_ = Gtk::make_managed<AudioCDView>(this);
    audioCDView_->set_expand(true);
    audioCDView_->signal_tocModified.connect(
        sigc::mem_fun(*this, &AudioCDProject::update));

    auto vbox = dynamic_cast<Gtk::Box*>(get_first_child());
    auto audio_toolbar = builder_->get_object<Gtk::Box>("audio-toolbar");
    vbox->insert_child_after(*audioCDView_, *audio_toolbar);
    audioCDView_->tocEditView()->sampleViewFull();
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
    std::string s;
    if (tocEdit_->tocDirty())
        s += "* ";
    s += tocEdit_->filename().filename();

    set_title(s);
    s += " - Gnome CD Master";
    if (headerBarTitle_)
	headerBarTitle_->set_label(s);
}

void AudioCDProject::status(const char *msg)
{
    statusMessage(msg);
}

void AudioCDProject::errorDialog(const Glib::ustring& msg)
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
    if (audioCDView_)
        audioCDView_->fullView();
}

void AudioCDProject::sampleSelect(unsigned long start, unsigned long len)
{
    audioCDView_->tocEditView()->sampleSelect(start, len);
}

void AudioCDProject::cancelEnable(bool enable)
{
    if (stopButton_)
        stopButton_->set_sensitive(enable);
}

bool AudioCDProject::on_close_request()
{
    log_message(0, "AudioCDProject: close request");
    if (tocEdit_->tocDirty()) {

        Glib::ustring message = "Project ";
        message += tocEdit_->filename().string();
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
	    this->set_visible(false);
        });
	box->choose();
	return true;
    }
    return false;
}

void AudioCDProject::saveProject()
{
    if (new_) {
        saveAsProject();
        return;
    }

    if (tocEdit_->saveToc() == 0) {
        statusMessage(_("Project saved to \"%s\"."), tocEdit_->filename().c_str());
        guiUpdate(UPD_TOC_DIRTY);
    } else {
	Glib::ustring s(_("Cannot save toc to \""));
        s += tocEdit_->filename().string();
        s += "\":";

        errorDialog(s);
    }
}

void AudioCDProject::saveAsProject()
{
    auto dialog = Gtk::FileDialog::create();
    dialog->set_modal(true);
    dialog->set_title(_("Save Project"));
    dialog->save(sigc::bind<0>(sigc::mem_fun(*this, &AudioCDProject::saveAsDone), dialog));
}

void AudioCDProject::saveAsDone(Glib::RefPtr<Gtk::FileDialog> dialog,
				const Glib::RefPtr<Gio::AsyncResult>& result)
{
    try {
	auto file = dialog->save_finish(result);
	if (file) {
	    if (tocEdit_->saveAsToc(file->get_path()) == 0) {
		statusMessage(_("Project saved to \"%s\"."),
			      tocEdit_->filename().c_str());
		updateWindowTitle();
		guiUpdate(UPD_TOC_DIRTY);
		new_ = false;
	    } else {
		std::string m(_("Cannot save toc to \""));
		m += tocEdit_->filename();
		m += "\":";
		errorDialog(m);
	    }
	}
    } catch (...) {
    }
}

void AudioCDProject::recordToc2CD()
{
    if (!recordTocDialog_)
        recordTocDialog_ = Glib::make_refptr_for_instance<RecordTocDialog>(
            new RecordTocDialog(tocEdit_));

    recordTocDialog_->start(this);
    recordTocDialog_->present();
}

void AudioCDProject::tocRead()
{
    if (tocEdit_->tocDirty()) {
	MessageBox::message(*this, _("Save your project first"),
			   {_("This will replace your current project.")});
	return;
    }
    try {
	auto task = new FastTocRead();
	task->signalDone.connect([this, task](Toc* toc) {
	    if (toc) {
		this->tocEdit_->toc(toc, "", true);
	    } else {
		this->errorDialog(_("Unable to extra disc TOC"));
	    }
	    delete(task);
	    guiUpdate(UPD_ALL);
	    this->fullView();
	});
	tocEdit_->taskManager().addJob(task);
    } catch (...) {
	MessageBox::message(*this, _("Insert an audio CD first."));
    }
}

void AudioCDProject::CDRead()
{
    if (tocEdit_->tocDirty()) {
	MessageBox::message(*this, _("Save your project first"),
			   {_("This will replace your current project.")});
	return;
    }
    if (!cdReader_)
	cdReader_ = AudioCDRead::create(this);

    cdReader_->present();
}

void AudioCDProject::projectInfo()
{
    if (!tocInfoDialog_)
        tocInfoDialog_ = Glib::make_refptr_for_instance<TocInfoDialog>(
            new TocInfoDialog(this));

    tocInfoDialog_->start(tocEdit_.get());
    tocInfoDialog_->present();
}

void AudioCDProject::cdTextDialog()
{
    if (!cdTextDialog_)
        cdTextDialog_ = Glib::make_refptr_for_instance<CdTextDialog>(
            new CdTextDialog(this));

    cdTextDialog_->set(tocEdit_);
    cdTextDialog_->start();
}

void AudioCDProject::cddbQuery()
{
    spin(true);
    if (!cddbWindow_) {
	cddbWindow_ = CddbWindow::create(tocEdit_);
	cddbWindow_->signalQueryDone.connect(sigc::mem_fun(*this,
						   &AudioCDProject::cddbQuery1));
	cddbWindow_->signalReadDone.connect(sigc::mem_fun(*this,
						   &AudioCDProject::cddbQuery2));
    }
    cddbWindow_->setTocEdit(tocEdit_);
    cddbWindow_->doQuery();
}

void AudioCDProject::cddbQuery1(Cddb* cddb, CddbWindow::Error err)
{
    if (err != CddbWindow::OK) {
	spin(false);
	if (err == CddbWindow::CONNERR) {
	    ErrorBox::message(*this, _("Unable to connect to CDDB server."));
	} else if (err == CddbWindow::QUERYERR) {
	    ErrorBox::message(*this, _("An error occured while talking to CDDB server."));
	}
	statusMessage("");
	return;
    }

    auto rc = cddb->queryResults().size();
    
    log_message(0, "CDDB Query done with %d results, err=%d", rc, err);

    if (rc > 0) {
	statusMessage("Downloading CDDB data for %s...",
		      cddb->queryResults()[0].title.c_str());
	cddbWindow_->getEntry(cddb->queryResults()[0].category,
			      cddb->queryResults()[0].diskId);
    } else {
	statusMessage("CDDB query yielded %d possible match%s.",
		      rc, (rc > 1 ? "es" : ""));
	spin(false);
    }
}

void AudioCDProject::cddbQuery2(Cddb* cddb, CddbWindow::Error err)
{
    spin(false);

    if (err == CddbWindow::READERR) {
	ErrorBox::message(*this,
			  _("An error occured while downloading data from the CDDB server."));
	return;
    }

    assert(cddb->dbEntry().has_value());

    statusMessage("Data found for \"%s / %s\". CD-TEXT updated.",
		  cddb->dbEntry()->diskArtist.c_str(),
		  cddb->dbEntry()->diskTitle.c_str());

    log_message(0, "CDDB Read done: has result? %d, err=%d",
		cddb->dbEntry().has_value(), err);
}

void AudioCDProject::update(unsigned long level)
{
    log_message(1, "[update %x] AudioCDProject", level);
    // FIXME: Here we should update the menus and the icons
    //        this is, enabled/disabled.

    level |= tocEdit_->updateLevel();

    if (level & (UPD_TOC_DIRTY | UPD_TOC_DATA))
        updateWindowTitle();

    audioCDView_->update(level);

    if (tocInfoDialog_)
        tocInfoDialog_->update(level);

    if (cdTextDialog_)
        cdTextDialog_->update(level);

    if (recordTocDialog_)
        recordTocDialog_->update(level);

    if (cdReader_)
	cdReader_->update(level);

    if (soundInterface_) {
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
            play_action_->set_enabled(tocEdit_->editable());
        }
    }

    save_action_->set_enabled(!tocEdit_->empty());
    save_as_action_->set_enabled(!tocEdit_->empty());
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

    if (audioCDView_->tocEditView()) {
        if (!audioCDView_->tocEditView()->sampleSelection(&start, &end))
            audioCDView_->tocEditView()->sampleView(&start, &end);
        
        playStart(start, end);
    }
}

void AudioCDProject::playStart(unsigned long start, unsigned long end)
{
    unsigned long level = 0;

    if (!soundInterface_ || playStatus_ == PLAYING)
        return;

    if (tocEdit_->lengthSample() == 0) {
        guiUpdate(UPD_PLAY_STATUS);
        return;
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
    audioCDView_->zoomx2();
}

void AudioCDProject::on_zoom_out_clicked()
{
    audioCDView_->zoomOut();
}

void AudioCDProject::on_zoom_fit_clicked()
{
    audioCDView_->fullView();
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
	    Glib::ustring msg = "Could not read M3U file \"";
            msg += file;
            msg += "\"";
            errorDialog(msg);
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
    unsigned long pos;

    TocEditView *view = audioCDView_->tocEditView();
    if (!view)
        return false;
    if (!view->sampleMarker(&pos))
        pos = 0;

    std::list<std::string>::iterator i = files.end();
    do {
        i--;
        tocEdit_->queueInsertFile((*i).c_str(), pos);
    } while (i != files.begin());

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


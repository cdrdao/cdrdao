/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <config.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "CdDevice.h"
#include "MessageBox.h"
#include "ProgressDialog.h"
#include "TocEdit.h"
#include "guiUpdate.h"

#include "remote.h"

#include "ProgressDialog.h"
#include <glibmm/i18n.h>
#include <cstdio>
#include "CdDevice.h"

ProgressDialog::ProgressDialog(ProgressDialogPool *father)
    : poolFather_(father)
{
    set_title(_("Progress"));
    set_default_size(400, -1);
    set_resizable(false);

    auto main_vbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 10);
    main_vbox->set_margin(10);
    set_child(*main_vbox);

    // Project Info
    auto project_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 5);
    auto label = Gtk::make_managed<Gtk::Label>(_("Project: "));
    tocName_ = Gtk::make_managed<Gtk::Label>();
    project_hbox->append(*label);
    project_hbox->append(*tocName_);
    main_vbox->append(*project_hbox);

    statusMsg_ = Gtk::make_managed<Gtk::Label>();
    statusMsg_->set_halign(Gtk::Align::START);
    main_vbox->append(*statusMsg_);

    // Time Info
    auto time_hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    time_hbox->append(*Gtk::make_managed<Gtk::Label>(_("Elapsed Time: ")));
    currentTime_ = Gtk::make_managed<Gtk::Label>("0:00:00");
    time_hbox->append(*currentTime_);
    
    auto remaining_label = Gtk::make_managed<Gtk::Label>(_("Remaining Time: "));
    remaining_label->set_margin_start(10);
    time_hbox->append(*remaining_label);
    remainingTime_ = Gtk::make_managed<Gtk::Label>("");
    time_hbox->append(*remainingTime_);
    main_vbox->append(*time_hbox);

    // Progress Grid (Replacing Table)
    auto grid = Gtk::make_managed<Gtk::Grid>();
    grid->set_row_spacing(5);
    grid->set_column_spacing(10);
    main_vbox->append(*grid);

    trackLabel_ = Gtk::make_managed<Gtk::Label>(_("Track:"));
    trackLabel_->set_halign(Gtk::Align::END);
    trackProgress_ = Gtk::make_managed<Gtk::ProgressBar>();
    trackProgress_->set_hexpand(true);
    grid->attach(*trackLabel_, 0, 0);
    grid->attach(*trackProgress_, 1, 0);

    auto totalLabel = Gtk::make_managed<Gtk::Label>(_("Total:"));
    totalLabel->set_halign(Gtk::Align::END);
    totalProgress_ = Gtk::make_managed<Gtk::ProgressBar>();
    totalProgress_->set_hexpand(true);
    grid->attach(*totalLabel, 0, 1);
    grid->attach(*totalProgress_, 1, 1);

    bufferFillRateLabel_ = Gtk::make_managed<Gtk::Label>(_("Input Buffer:"));
    bufferFillRateLabel_->set_halign(Gtk::Align::END);
    bufferFillRate_ = Gtk::make_managed<Gtk::ProgressBar>();
    grid->attach(*bufferFillRateLabel_, 0, 2);
    grid->attach(*bufferFillRate_, 1, 2);

    writerFillRateLabel_ = Gtk::make_managed<Gtk::Label>(_("Write Buffer:"));
    writerFillRateLabel_->set_halign(Gtk::Align::END);
    writerFillRate_ = Gtk::make_managed<Gtk::ProgressBar>();
    grid->attach(*writerFillRateLabel_, 0, 3);
    grid->attach(*writerFillRate_, 1, 3);

    // Buttons
    auto bbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
    bbox->set_halign(Gtk::Align::CENTER);
    bbox->set_margin_top(10);

    cancelButton_ = Gtk::make_managed<Gtk::Button>(_("Cancel"));
    closeButton_ = Gtk::make_managed<Gtk::Button>(_("Close"));
    ejectButton_ = Gtk::make_managed<Gtk::Button>(_("Eject"));

    bbox->append(*cancelButton_);
    bbox->append(*closeButton_);
    bbox->append(*ejectButton_);
    main_vbox->append(*bbox);

    cancelButton_->signal_clicked().connect(sigc::mem_fun(*this, &ProgressDialog::closeAction));
    closeButton_->signal_clicked().connect(sigc::mem_fun(*this, &ProgressDialog::closeAction));
    ejectButton_->signal_clicked().connect(sigc::mem_fun(*this, &ProgressDialog::ejectAction));

    setCloseButtonLabel(1);
}

void ProgressDialog::start(CdDevice *device, const char *tocFileName)
{
    if (!device) return;
    if (active_) {
        present(); 
        return;
    }

    active_ = true;
    device_ = device;
    clear();

    timeout_connection_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &ProgressDialog::on_timeout), 1000);

    statusMsg_->set_text(_("Initializing..."));
    tocName_->set_text(tocFileName);

    setCloseButtonLabel(1);
    cancelButton_->set_sensitive(true);
    ejectButton_->set_sensitive(false);

    set_title(Glib::ustring(device->vendor()) + " " + Glib::ustring(device->product()));
    set_modal(true);
    present();
}

bool ProgressDialog::on_timeout()
{
    char buf[50];
    struct timeval timenow;
    gettimeofday(&timenow, NULL);

    long elapsed = timenow.tv_sec - time_start_.tv_sec;
    long hours = elapsed / 3600;
    long mins = (elapsed % 3600) / 60;
    long secs = elapsed % 60;

    snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", hours, mins, secs);
    currentTime_->set_text(buf);

    if (actTotalProgress_ > 10) {
        if (!leadTimeFilled_) {
            leadTime_ = elapsed;
            leadTimeFilled_ = true;
        }

        double total_est = (double)elapsed / (actTotalProgress_ / 1000.0);
        long time_remain = (long)(total_est + 0.5) - elapsed;
        if (time_remain < 0) time_remain = 0;

        hours = time_remain / 3600;
        mins = (time_remain % 3600) / 60;
        secs = time_remain % 60;

        snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", hours, mins, secs);
        remainingTime_->set_text(buf);
    }

    return !finished_; 
}

bool ProgressDialog::on_close_request()
{
    if (finished_) {
        poolFather_->stop(this);
    }
    return true; // Prevents the window from being destroyed immediately
}

void ProgressDialog::setCloseButtonLabel(int l)
{
    if (l == 1) {
        closeButton_->hide();
        cancelButton_->show();
    } else {
        cancelButton_->hide();
        closeButton_->show();
    }
    actCloseButtonLabel_ = l;
}

void ProgressDialog::stop()
{
    if (active_) {
        hide();
        active_ = false;
        device_ = nullptr;
        timeout_connection_.disconnect();
    }
}

ProgressDialog::~ProgressDialog()
{
}

void ProgressDialog::ejectAction()
{
    if (device_)
        if (device_->ejectCd()) {
            ejectButton_->set_sensitive(false);
        }
}

void ProgressDialog::closeAction()
{
    if (finished_) {
        poolFather_->stop(this);
        return;
    }

    switch (device_->action()) {
    case CdDevice::A_RECORD: {
        auto msg = Ask2Box::create(*this, _("Abort Recording"), 1, {_("Abort recording process?")});

	msg->dialogDone.connect([this](int result) {
	    if (result == 0 && this->device_ != NULL) {
		this->cancelButton_->set_sensitive(false);
		this->ejectButton_->set_sensitive(true);
		this->device_->abortDaoRecording();
	    }
        });
	msg->choose();
    } break;

    case CdDevice::A_READ: {
        auto msg = Ask2Box::create(*this, _("Abort Reading"), 1, {_("Abort reading process?")});

	msg->dialogDone.connect([this](int result) {
	    if (result == 0 && this->device_ != NULL) {
		this->cancelButton_->set_sensitive(false);
		this->ejectButton_->set_sensitive(true);
		this->device_->abortDaoReading();
	    }
	});
	msg->choose();
    } break;

    case CdDevice::A_DUPLICATE: {
        auto msg = Ask2Box::create(*this, _("Abort Process"), 1, {_("Abort duplicating process?")});

	msg->dialogDone.connect([this](int result) {
	    if (result == 0 && this->device_ != NULL) {
		this->cancelButton_->set_sensitive(false);
		this->ejectButton_->set_sensitive(true);
		this->device_->abortDaoDuplication();
	    }
	});
	msg->choose();
    } break;

    case CdDevice::A_BLANK: {
        auto msg = Ask2Box::create(*this, _("Abort Process"), 1, {_("Abort blanking process?")});

	msg->dialogDone.connect([this](int result) {
	    if (result == 0 && this->device_ != NULL) {
		this->cancelButton_->set_sensitive(false);
		this->ejectButton_->set_sensitive(true);
		this->device_->abortBlank();
        }
	});
	msg->choose();
    } break;
    default:
        break;
    }
}

void ProgressDialog::clear()
{
    finished_ = 0;
    actStatus_ = 0;
    actTrack_ = 0;
    actTrackProgress_ = 0;
    actTotalProgress_ = 0;
    actBufferFill_ = 0;
    actWriterFill_ = 0;

    gettimeofday(&time_start_, NULL);
    currentTime_->set_text("0:00:00");
    remainingTime_->set_text("");
    leadTimeFilled_ = false;
    statusMsg_->set_text("");
    trackProgress_->set_fraction(0.0);
    totalProgress_->set_fraction(0.0);
    bufferFillRate_->set_fraction(0.0);
    writerFillRate_->set_fraction(0.0);

    set_title("");
}

void ProgressDialog::update(unsigned long level)
{
    int status;
    int totalTracks;
    int track;
    int trackProgress;
    int totalProgress;
    int bufferFill;
    int writerFill;
    char buf[40];
    std::string s;

    if (!active_ || device_ == NULL)
        return;

    if (finished_)
        return;

    if ((level & UPD_PROGRESS_STATUS) && device_->progressStatusChanged()) {
        device_->progress(&status, &totalTracks, &track, &trackProgress, &totalProgress,
                          &bufferFill, &writerFill);

        if (status != actStatus_ || track != actTrack_) {
            actStatus_ = status;
            actTrack_ = track;

            switch (status) {
            case PGSMSG_RCD_ANALYZING:
                actTrack_ = track;

                s = _("Analyzing track ");
                snprintf(buf, sizeof(buf), "%d of %d", track, totalTracks);
                s += buf;

                statusMsg_->set_text(s);
                break;

            case PGSMSG_RCD_EXTRACTING:
                actTrack_ = track;

                s = _("Extracting ");
                snprintf(buf, sizeof(buf), "%d", totalTracks);
                s += buf;
                s += _(" tracks...");

                statusMsg_->set_text(s);
                break;

            case PGSMSG_WCD_LEADIN:
                statusMsg_->set_text(_("Writing lead-in..."));
                break;

            case PGSMSG_WCD_DATA:
                actTrack_ = track;

                s = _("Writing track ");
                snprintf(buf, sizeof(buf), "%d of %d", track, totalTracks);
                s += buf;

                statusMsg_->set_text(s);
                break;

            case PGSMSG_WCD_LEADOUT:
                statusMsg_->set_text(_("Writing lead-out..."));
                break;

            case PGSMSG_BLK:
                statusMsg_->set_text(_("Blanking..."));
                break;
            }
        }

        if (trackProgress != actTrackProgress_) {
            actTrackProgress_ = trackProgress;
            if (trackProgress <= 1000)
                trackProgress_->set_fraction(trackProgress / 1000.0);
        }

        if (totalProgress != actTotalProgress_) {
            if (actTotalProgress_ == 0)
                gettimeofday(&time_start_, 0);
            actTotalProgress_ = totalProgress;
            if (totalProgress <= 1000)
                totalProgress_->set_fraction(totalProgress / 1000.0);
        }

        if (bufferFill != actBufferFill_) {
            actBufferFill_ = bufferFill;
            if (bufferFill <= 1000)
                bufferFillRate_->set_fraction(bufferFill / 100.0);
        }

        if (writerFill != actWriterFill_) {
            actWriterFill_ = writerFill;
            if (writerFill <= 1000)
                writerFillRate_->set_fraction(writerFill / 100.0);
        }
    }

    switch (device_->action()) {
    case CdDevice::A_RECORD:
        if (device_->status() != CdDevice::DEV_RECORDING) {
            switch (device_->exitStatus()) {
            case 0:
                statusMsg_->set_text(_("Recording finished successfully."));
                break;

            case 255:
                statusMsg_->set_text(_("Cannot execute cdrdao. Please check "
                                       "your PATH."));
                break;

            default:
                statusMsg_->set_text(_("Recording aborted with error."));
                break;
            }

            finished_ = 1;

            setCloseButtonLabel(2);
            ejectButton_->set_sensitive(true);
        }
        break;

    case CdDevice::A_READ:
        if (device_->status() != CdDevice::DEV_READING) {
            switch (device_->exitStatus()) {
            case 0:
                statusMsg_->set_text(_("Reading finished successfully."));
                break;

            case 255:
                statusMsg_->set_text(_("Cannot execute cdrdao. Please check "
                                       "your PATH."));
                break;

            default:
                statusMsg_->set_text(_("Reading aborted."));
                break;
            }

            finished_ = 1;

            setCloseButtonLabel(2);
            ejectButton_->set_sensitive(true);
        }

        break;

    case CdDevice::A_DUPLICATE:
        if (device_->status() != CdDevice::DEV_RECORDING) {
            switch (device_->exitStatus()) {
            case 0:
                statusMsg_->set_text(_("CD copying finished successfully."));
                break;

            case 255:
                statusMsg_->set_text(_("Cannot execute cdrdao. Please check "
                                       "your PATH."));
                break;

            default:
                statusMsg_->set_text(_("CD copying aborted with error."));
                break;
            }

            finished_ = 1;

            setCloseButtonLabel(2);
            ejectButton_->set_sensitive(true);
        }

        break;

    case CdDevice::A_BLANK:
        if (device_->status() != CdDevice::DEV_BLANKING) {
            switch (device_->exitStatus()) {
            case 0:
                statusMsg_->set_text(_("Blanking finished successfully."));
                break;

            case 255:
                statusMsg_->set_text(_("Cannot execute cdrdao. Please check "
                                       "your PATH."));
                break;

            default:
                statusMsg_->set_text(_("Blanking aborted with error."));
                break;
            }

            finished_ = 1;

            setCloseButtonLabel(2);
            ejectButton_->set_sensitive(true);
        }

        break;

    default:
        statusMsg_->set_text(_("Unknown device action!"));
        break;
    }
}

void ProgressDialog::needBufferProgress(bool visible)
{
    if (visible) {
        bufferFillRate_->show();
        bufferFillRateLabel_->show();
        writerFillRate_->show();
        writerFillRateLabel_->show();
    } else {
        bufferFillRate_->hide();
        bufferFillRateLabel_->hide();
        writerFillRate_->hide();
        writerFillRateLabel_->hide();
    }
}

void ProgressDialog::needTrackProgress(bool visible)
{
    if (visible) {
        trackProgress_->show();
        trackLabel_->show();
    } else {
        trackProgress_->hide();
        trackLabel_->hide();
    }
}

ProgressDialogPool::ProgressDialogPool()
{
    activeDialogs_ = NULL;
    pool_ = NULL;
}

ProgressDialogPool::~ProgressDialogPool()
{
}

void ProgressDialogPool::update(unsigned long status)
{
    ProgressDialog *run;

    for (run = activeDialogs_; run != NULL; run = run->poolNext_)
        run->update(status);
}

ProgressDialog *ProgressDialogPool::start(CdDevice *device, const char *tocFileName,
                                          bool showBuffer, bool showTrack)
{
    ProgressDialog *dialog;

    if (pool_ == NULL) {
        dialog = new ProgressDialog(this);
    } else {
        dialog = pool_;
        pool_ = pool_->poolNext_;
    }

    dialog->poolNext_ = activeDialogs_;
    activeDialogs_ = dialog;

    dialog->needBufferProgress(showBuffer);
    dialog->needTrackProgress(showTrack);

    dialog->start(device, tocFileName);

    return dialog;
}

ProgressDialog *ProgressDialogPool::start(Gtk::Window &parent, CdDevice *device,
                                          const char *tocFileName, bool showBuffer, bool showTrack)
{
    ProgressDialog *dialog = start(device, tocFileName, showBuffer, showTrack);
    dialog->set_transient_for(parent);
    return dialog;
}

void ProgressDialogPool::stop(ProgressDialog *dialog)
{
    ProgressDialog *run, *pred;

    for (pred = NULL, run = activeDialogs_; run != NULL; pred = run, run = run->poolNext_) {
        if (run == dialog)
            break;
    }

    if (run == NULL)
        return;

    dialog->stop();

    if (pred == NULL)
        activeDialogs_ = activeDialogs_->poolNext_;
    else
        pred->poolNext_ = run->poolNext_;

    dialog->poolNext_ = pool_;
    pool_ = dialog;
}

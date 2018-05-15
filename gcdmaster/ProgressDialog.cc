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

#include <stdio.h>
#include <stddef.h>
#include <ctype.h>

#include <gtkmm.h>
#include <gnome.h>

#include "ProgressDialog.h"
#include "MessageBox.h"
#include "TocEdit.h"
#include "guiUpdate.h"
#include "CdDevice.h"

#include "remote.h"

ProgressDialog::ProgressDialog(ProgressDialogPool *father)
{
  Gtk::Label *label;
  Gtk::HBox *hbox;
  Gtk::VBox *contents = manage(new Gtk::VBox);
  Gtk::Table *table;
  Gtk::Alignment *align;

  poolFather_ = father;
  active_ = 0;
  device_ = NULL;
  poolNext_ = NULL;

  contents->set_spacing(5);

  statusMsg_ = manage(new Gtk::Label());
  trackProgress_ = manage(new Gtk::ProgressBar);
  totalProgress_ = manage(new Gtk::ProgressBar);
  bufferFillRate_ = manage(new Gtk::ProgressBar);
  writerFillRate_ = manage(new Gtk::ProgressBar);
  tocName_ = manage(new Gtk::Label);

  hbox = manage(new Gtk::HBox);
  label = manage(new Gtk::Label(_("Project: ")));
  hbox->pack_start(*label, Gtk::PACK_SHRINK);
  hbox->pack_start(*tocName_, Gtk::PACK_SHRINK);
  contents->pack_start(*hbox, Gtk::PACK_SHRINK);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*statusMsg_, Gtk::PACK_SHRINK);
  contents->pack_start(*hbox, Gtk::PACK_SHRINK);

  hbox = manage(new Gtk::HBox(true, true));
  label = manage(new Gtk::Label(_("Elapsed Time: "), 1));
  hbox->pack_start(*label, Gtk::PACK_SHRINK);
  currentTime_ = manage(new Gtk::Label());
  hbox->pack_start(*currentTime_, Gtk::PACK_SHRINK);
  label = manage(new Gtk::Label(_("Remaining Time: "), 1));
  hbox->pack_start(*label, Gtk::PACK_SHRINK);
  remainingTime_ = manage(new Gtk::Label("", 0));
  hbox->pack_start(*remainingTime_, Gtk::PACK_SHRINK);
  contents->pack_start(*hbox, Gtk::PACK_SHRINK);

  table = manage(new Gtk::Table(4, 2, false));
  table->set_row_spacings(5);
  table->set_col_spacings(5);
  contents->pack_start(*table, Gtk::PACK_SHRINK);

  trackLabel_ = manage(new Gtk::Label(_("Track:")));
  align = manage(new Gtk::Alignment(1.0, 0.5, 0.0, 0.0));
  align->add(*trackLabel_);
  table->attach(*align, 0, 1, 0, 1, Gtk::FILL);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*trackProgress_);
  table->attach(*hbox, 1, 2, 0, 1);

  label = manage(new Gtk::Label(_("Total:")));
  align = manage(new Gtk::Alignment(1.0, 0.5, 0.0, 0.0));
  align->add(*label);
  table->attach(*align, 0, 1, 1, 2, Gtk::FILL);

  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*totalProgress_);
  table->attach(*hbox, 1, 2, 1, 2);

  bufferFillRateLabel_ = manage(new Gtk::Label(_("Input Buffer:")));
  align = manage(new Gtk::Alignment(1.0, 0.5, 0.0, 0.0));
  align->add(*bufferFillRateLabel_);
  table->attach(*align, 0, 1, 2, 3, Gtk::FILL);
  
  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*bufferFillRate_);
  table->attach(*hbox, 1, 2, 2, 3);
  
  writerFillRateLabel_ = manage(new Gtk::Label(_("Write Buffer:")));
  table->attach(*writerFillRateLabel_, 0, 1, 3, 4, Gtk::FILL);
  table->attach(*writerFillRate_, 1, 2, 3, 4);
  
  hbox = manage(new Gtk::HBox);
  hbox->pack_start(*contents, true, true, 10);
  get_vbox()->pack_start(*hbox, false, false, 10);

  Gtk::HButtonBox *bbox = manage(new Gtk::HButtonBox(Gtk::BUTTONBOX_SPREAD,
                                                     20));

  cancelButton_ = manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CANCEL)));
  bbox->pack_start(*cancelButton_);

  closeButton_ = manage(new Gtk::Button(Gtk::StockID(Gtk::Stock::CLOSE)));
  bbox->pack_start(*closeButton_);

  ejectButton_ = manage(new Gtk::Button("Eject"));
  bbox->pack_start(*ejectButton_);

  actCloseButtonLabel_ = 2;

  cancelButton_->signal_clicked().
    connect(sigc::mem_fun(*this, &ProgressDialog::closeAction));
  closeButton_->signal_clicked().
    connect(sigc::mem_fun(*this, &ProgressDialog::closeAction));
  ejectButton_->signal_clicked().
    connect(sigc::mem_fun(*this, &ProgressDialog::ejectAction));

  get_action_area()->pack_start(*bbox, TRUE, TRUE, 10);
  set_size_request(400, -1);
  show_all_children();
}

ProgressDialog::~ProgressDialog()
{
}

void ProgressDialog::start(CdDevice *device, const char *tocFileName)
{
  std::string s;

  if (device == NULL)
    return;

  if (active_) {
    raise();
    return;
  }

  active_ = true;
  device_ = device;

  clear();

  Glib::signal_timeout().connect(mem_fun(*this, &ProgressDialog::time), 1000);

  statusMsg_->set_text(_("Initializing..."));
  tocName_->set_text(tocFileName);

  setCloseButtonLabel(1);
  cancelButton_->set_sensitive(true);
  ejectButton_->set_sensitive(false);

  s = device->vendor();
  s += " ";
  s += device->product();

  set_title(s);
  set_modal(true);
  show();
}

void ProgressDialog::stop()
{
  if (active_) {
    hide();
    active_ = false;
    device_ = NULL;
  }
}

bool ProgressDialog::on_delete_event(GdkEventAny*)
{
  if (finished_) {
    poolFather_->stop(this);
  }
  return true;
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
  case CdDevice::A_RECORD:
    {
      Ask2Box msg(this, _("Abort Recording"), 0, 2,
                  _("Abort recording process?"), NULL);

      if (msg.run() == 1 && device_ != NULL) {
        cancelButton_->set_sensitive(false);
        ejectButton_->set_sensitive(true);
        device_->abortDaoRecording();
      }
    }
    break;

  case CdDevice::A_READ:
    {        
      Ask2Box msg(this, _("Abort Reading"), 0, 2, _("Abort reading process?"),
                  NULL);

      if (msg.run() == 1 && device_ != NULL) {
        cancelButton_->set_sensitive(false);
        ejectButton_->set_sensitive(true);
        device_->abortDaoReading();
      }
    }
    break;

  case CdDevice::A_DUPLICATE:
    {        
      Ask2Box msg(this, _("Abort Process"), 0, 2,
                  _("Abort duplicating process?"), NULL);

      if (msg.run() == 1 && device_ != NULL) {
        cancelButton_->set_sensitive(false);
        ejectButton_->set_sensitive(true);
        device_->abortDaoDuplication();
      }
    }
    break;

  case CdDevice::A_BLANK:
    {        
      Ask2Box msg(this, _("Abort Process"), 0, 2, _("Abort blanking process?"),
                  NULL);

      if (msg.run() == 1 && device_ != NULL) {
        cancelButton_->set_sensitive(false);
        ejectButton_->set_sensitive(true);
        device_->abortBlank();
      }
    }
    break;
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

  gettimeofday(&time_, NULL);
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
    device_->progress(&status, &totalTracks, &track, &trackProgress,
		      &totalProgress, &bufferFill, &writerFill);

    if (status != actStatus_ || track != actTrack_) {
      actStatus_ = status;
      actTrack_ = track;

      switch (status) {
      case PGSMSG_RCD_ANALYZING:
      	actTrack_ = track;

      	s = _("Analyzing track ");
      	sprintf(buf, "%d of %d", track, totalTracks);
      	s += buf;

      	statusMsg_->set_text(s);
      	break;

      case PGSMSG_RCD_EXTRACTING:
      	actTrack_ = track;

      	s = _("Extracting ");
	sprintf(buf, "%d", totalTracks);
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
	sprintf(buf, "%d of %d", track, totalTracks);
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
        gettimeofday(&time_, 0);
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
    statusMsg_->set_text(_("Unknow device action!"));
    break;
  }
}

// Sets label of close button.
// l: 1: 'abort'	--> CANCEL gnome stock button (i18n)
//    2: 'dismiss'  --> CLOSE  gnome stock button (i18n)
void ProgressDialog::setCloseButtonLabel(int l)
{
  if (actCloseButtonLabel_ == l)
    return;

  switch (l) {
  case 1:
    closeButton_->hide();
    cancelButton_->show();
    break;
  case 2:
    cancelButton_->hide();
    closeButton_->show();
    break;
  }

  actCloseButtonLabel_ = l;
}

bool ProgressDialog::time()
{
  char buf[50];
  struct timeval timenow;
  long time, time_remain, hours, mins, secs;

  gettimeofday(&timenow, NULL);

  time = timenow.tv_sec - time_.tv_sec;

  hours = time / 3600;
  mins = (time - (hours * 3600)) / 60;
  secs = time - ((hours * 3600) + (mins * 60));

  sprintf(buf, "%ld:%02ld:%02ld", hours, mins, secs);
  currentTime_->set_text(buf);

  if (actTotalProgress_ > 10)
  {
//Hack!
// Denis: no shit
    gfloat aux1, aux2, aux3;

    if (!leadTimeFilled_)
    {
      leadTime_ = time;
      leadTimeFilled_ = true;
    }

    time_remain = (int)
      (((double)time / ((double)actTotalProgress_ / 1000.0)) + 0.5);
    time_remain -= time;
    if (time_remain < 0) time_remain = 0;

    hours = time_remain / 3600;
    mins = (time_remain - (hours * 3600)) / 60;
    secs = time_remain - ((hours * 3600) + (mins * 60));

    sprintf(buf, "%ld:%02ld:%02ld", hours, mins, secs);
    remainingTime_->set_text(buf);
  }

  if (finished_) 
    return false;
  else
    return true;
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
  
ProgressDialog *ProgressDialogPool::start(CdDevice *device,
                                          const char *tocFileName,
                                          bool showBuffer, bool showTrack)
{
  ProgressDialog *dialog;

  if (pool_ == NULL) {
    dialog = new ProgressDialog(this);
  }
  else {
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

ProgressDialog *ProgressDialogPool::start(Gtk::Window& parent,
                                          CdDevice *device,
                                          const char *tocFileName,
                                          bool showBuffer, bool showTrack)
{
  ProgressDialog* dialog = start(device, tocFileName, showBuffer, showTrack);
  dialog->set_transient_for(parent);
  return dialog;
}
  
void ProgressDialogPool::stop(ProgressDialog *dialog)
{
  ProgressDialog *run, *pred;

  for (pred = NULL, run = activeDialogs_; run != NULL;
       pred = run, run = run->poolNext_) {
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

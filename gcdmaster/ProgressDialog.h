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

#ifndef __PROGRESS_DIALOG_H__
#define __PROGRESS_DIALOG_H__

#include <gtkmm.h>
#include <sys/time.h>
#include <string>

class CdDevice;
class ProgressDialogPool;

class ProgressDialog : public Gtk::Window
{
  public:
    ProgressDialog(ProgressDialogPool *father);
    virtual ~ProgressDialog();

    void update(unsigned long);
    void start(CdDevice *, const std::string& tocFileName);
    void stop();

    void needBufferProgress(bool visible);
    void needTrackProgress(bool visible);

  private:
    friend class ProgressDialogPool;

    ProgressDialogPool *poolFather_;

    bool active_ = false;
    CdDevice *device_ = nullptr;

    int finished_ = 0;
    int actStatus_ = 0;
    int actTrack_ = 0;
    int actTrackProgress_ = 0;
    int actTotalProgress_ = 0;
    int actBufferFill_ = 0;
    int actWriterFill_ = 0;

    int actCloseButtonLabel_ = 0;

    Gtk::Label *currentTime_;
    Gtk::Label *remainingTime_;

    long leadTime_ = 0;
    bool leadTimeFilled_ = false;

    struct timeval time_start_;
    bool on_timeout();

    Gtk::Button *cancelButton_;
    Gtk::Button *closeButton_;
    Gtk::Button *ejectButton_;
    Gtk::Label *tocName_;

    Gtk::Label *statusMsg_;
    Gtk::ProgressBar *trackProgress_;
    Gtk::Label *trackLabel_;
    Gtk::ProgressBar *totalProgress_;
    Gtk::ProgressBar *bufferFillRate_;
    Gtk::ProgressBar *writerFillRate_;
    Gtk::Label *bufferFillRateLabel_;
    Gtk::Label *writerFillRateLabel_;

    sigc::connection timeout_connection_;

    void closeAction();
    void ejectAction();
    void clear();
    void setCloseButtonLabel(int l);
    bool on_close_request() override;

    ProgressDialog *poolNext_ = nullptr;
};

class ProgressDialogPool
{
  public:
    ProgressDialogPool();
    ~ProgressDialogPool();

    void update(unsigned long);

    ProgressDialog *start(CdDevice *, const std::string& tocFileName,
			  bool showBuffer = true, bool showTrack = true);
    ProgressDialog *start(Gtk::Window &parent_window, CdDevice *, const std::string& tocFileName,
			  bool showBuffer = true, bool showTrack = true);
    void stop(ProgressDialog *);

  private:
    ProgressDialog *activeDialogs_;
    ProgressDialog *pool_;
};

#endif

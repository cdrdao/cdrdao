/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2000  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __AUDIOCDPROJECT_H__
#define __AUDIOCDPROJECT_H__

#include <gtk/gtk.h>
#include <gtkmm.h>

class Track;
class Sample;
class SoundIF;
class AudioCDView;
class TocInfoDialog;
class CdTextDialog;
class TocEdit;
class RecordTocDialog;
class AudioCDRead;

#include "Toc.h"
#include "gcdmaster.h"

class AudioCDProject : public GCDWindow
{
  public:
    enum PlayStatus {
        PLAYING,
        PAUSED,
        STOPPED
    };

    static AudioCDProject* create(Glib::RefPtr<Gtk::Builder> builder,
				  const Glib::ustring& path);

    virtual ~AudioCDProject();
    // Only meant to be called by the builder get_widget_derived().
    AudioCDProject(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder);

    bool on_close_request() override;
    void saveProject();
    void saveAsProject();
    void recordToc2CD();
    void tocRead();
    void CDRead();

    unsigned long playPosition();

    unsigned long getDelay();

    bool appendTrack(const char *file);
    bool appendTracks(std::list<std::string> &);
    bool appendFiles(std::list<std::string> &);
    bool insertFiles(std::list<std::string> &);

    PlayStatus playStatus() { return playStatus_;  }

    Glib::RefPtr<TocEdit> tocEdit() { return tocEdit_; }

    void statusMessage(const char *fmt, ...);

    // Controls for app bar
    void cancelEnable(bool);
    sigc::signal<void()> signalCancelClicked;

  protected:
    void setup(int number, const Glib::ustring& path);

    Gtk::Button *buttonPlay_;
    Gtk::Button *buttonStop_;
    Gtk::Button *buttonPause_;

    virtual void on_play_clicked();
    virtual void on_stop_clicked();
    virtual void on_pause_clicked();
    virtual void on_zoom_in_clicked();
    virtual void on_zoom_out_clicked();
    virtual void on_zoom_fit_clicked();
    virtual void on_cancel_clicked();

    virtual void status(const char *msg);
    virtual void errorDialog(const Glib::ustring& msg);
    virtual void progress(double val);
    virtual void spin(bool);
    virtual void fullView();
    virtual void sampleSelect(unsigned long, unsigned long);
    virtual void saveAsDone(Glib::RefPtr<Gtk::FileDialog>, const Glib::RefPtr<Gio::AsyncResult>&);

    virtual void updateWindowTitle();

    void playStart();
    void playStart(unsigned long start, unsigned long end);
    void playPause();
    void playStop();

    Glib::RefPtr<TocEdit> tocEdit_;

  private:
    TocReader tocReader;
    Glib::RefPtr<Gtk::Builder> builder_;
    int projectNumber_;
    bool new_ = false;

    SoundIF *soundInterface_;
    unsigned long playLength_; // remaining play length
    unsigned long playBurst_;
    unsigned long playPosition_;
    Sample *playBuffer_;
    bool playAbort_;

    bool playCallback();

    Glib::RefPtr<Gio::SimpleAction> play_action_, pause_action_, stop_action_;
    Glib::RefPtr<Gio::SimpleAction> toc_read_action_, cd_read_action_;
    Glib::RefPtr<Gio::SimpleAction> save_action_, save_as_action_;

    AudioCDView *audioCDView_;
    Glib::RefPtr<RecordTocDialog> recordTocDialog_;
    Glib::RefPtr<TocInfoDialog> tocInfoDialog_;
    Glib::RefPtr<CdTextDialog> cdTextDialog_;
    Glib::RefPtr<Gtk::ToggleButton> selectToggle_;
    Glib::RefPtr<Gtk::ToggleButton> zoomToggle_;
    Glib::RefPtr<AudioCDRead> cdReader_;
    void projectInfo();
    void cdTextDialog();
    virtual void update(unsigned long level) override;

    Glib::RefPtr<Gtk::Label> headerBarTitle_;
    Glib::RefPtr<Gtk::Label> statusbar_;
    Glib::RefPtr<Gtk::ProgressBar> progressbar_;
    Glib::RefPtr<Gtk::Button> stopButton_;

    enum PlayStatus playStatus_;
};
#endif

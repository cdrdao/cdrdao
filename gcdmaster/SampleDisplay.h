/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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

#ifndef __SAMPLE_DISPLAY_H
#define __SAMPLE_DISPLAY_H

#include <gtkmm.h>

#include "TrackManager.h"

class Toc;
class Sample;
class TocEdit;

class SampleDisplay : public Gtk::DrawingArea
{
public:
    SampleDisplay();

    void setTocEdit(Glib::RefPtr<TocEdit>);
    bool getSelection(unsigned long* start, unsigned long* end);
    void setMarker(unsigned long sample);
    void clearMarker();
    int  getMarker(unsigned long *);
    void setView(unsigned long start, unsigned long end);
    void getView(unsigned long *start, unsigned long *end);
    void setRegion(unsigned long start, unsigned long end);
    int  getRegion(unsigned long *start, unsigned long *end);
    void clearRegion();
    Glib::RefPtr<Gtk::Adjustment> get_adjustment() { return adjustment_; }
    void updateTrackMarks();
    void setCursor(int, unsigned long);

    void updateToc(unsigned long, unsigned long);

    sigc::signal<void(unsigned long)> markerSet;
    sigc::signal<void(unsigned long)> cursorMoved;
    sigc::signal<void(unsigned long, unsigned long)> selectionSet;
    sigc::signal<void()> selectionCleared;
    sigc::signal<void(int, int)> trackMarkSelected;
    sigc::signal<void(const Track *, int, int, unsigned long)> trackMarkMoved;
    sigc::signal<void(unsigned long, unsigned long)> viewModified;
  
protected:
    void on_resize(int width, int height);
    void on_motion(double x, double y);
    void on_pressed(int n_press, double x, double y);
    void on_released(int n_press, double x, double y);
    void on_enter(double x, double y);
    void on_leave();

    void on_draw_callback(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

    class Drawer {
    public:
        Drawer(SampleDisplay* sd, Cairo::RefPtr<Cairo::ImageSurface> surface) {
            cr_ = Cairo::Context::create(surface); sd_ = sd; }
        Drawer(SampleDisplay* sd, Cairo::RefPtr<Cairo::Context> c)
            : cr_(c), sd_(sd) {}
        
        void setColor(Gdk::RGBA c);
        void setColor(double r, double g, double b, double a = 1.0);
        void drawLine(gint, gint, gint, gint);
        void drawRectangle(gint, gint, gint, gint);
        void drawPixmap(Glib::RefPtr<Gdk::Pixbuf>, gint, gint);
        void drawText(const char* text, gint, gint);

    private:
        Cairo::RefPtr<Cairo::Context> cr_;
        SampleDisplay* sd_;
    };

    class SampleDrawer : public Drawer {
    public:
        SampleDrawer(SampleDisplay* sd) : Drawer(sd, sd->sample_layer_) {}
    };
    class MarkerDrawer : public Drawer {
    public:
        MarkerDrawer(SampleDisplay* sd) : Drawer(sd, sd->marks_layer_) {}
    };

private:
    enum DragMode { DRAG_NONE, DRAG_SAMPLE_MARKER, DRAG_TRACK_MARKER };

    Glib::RefPtr<Gtk::Adjustment> adjustment_;

    Cairo::RefPtr<Cairo::ImageSurface> sample_layer_;
    Cairo::RefPtr<Cairo::ImageSurface> marks_layer_;
  
    Glib::RefPtr<Gdk::Pixbuf> trackMarkerPixmap_;
    Glib::RefPtr<Gdk::Pixbuf> indexMarkerPixmap_;
    Glib::RefPtr<Gdk::Pixbuf> trackMarkerSelectedPixmap_;
    Glib::RefPtr<Gdk::Pixbuf> indexMarkerSelectedPixmap_;
    Glib::RefPtr<Gdk::Pixbuf> trackExtendPixmap_;
    Glib::RefPtr<Gdk::Pixbuf> indexExtendPixmap_;

    Gdk::RGBA sampleColor_{"darkslateblue"};
    Gdk::RGBA middleLineColor_{"red3"};
    Gdk::RGBA cursorColor_{"gold2"};
    Gdk::RGBA markerColor_{"red"};
    Gdk::RGBA selectionBackgroundColor_{"#ffc0e0"};
    Gdk::RGBA white_{"white"};
    Gdk::RGBA black_{"black"};

    bool draw_me_;
    bool draw_samples_;

    gint width_ = 0;
    gint height_ = 0;
    gint timeLineHeight_ = 0;
    gint timeLineY_ = 0;
    gint timeTickWidth_ = 0;
    gint timeTickSep_ = 20;
    gint sampleStartX_ = 0;
    gint sampleEndX_ = 0;
    gint sampleWidthX_ = 0;

    gint trackLineHeight_;
    gint trackLineY_;
    gint trackMarkerWidth_;
    const TrackManager::Entry *pickedTrackMarker_ = nullptr;

    gint chanSep_ = 10;
    gint chanHeight_ = 0;
    gint lcenter_ = 0;
    gint rcenter_ = 0;

    TrackManager *trackManager_ = nullptr;

    Glib::RefPtr<TocEdit> tocEdit_;
    unsigned long minSample_ = 0;
    unsigned long maxSample_ = 0;
    unsigned long resolution_ = 0;

    bool cursorDrawn_ = false;
    gint cursorX_ = 0;
    bool cursorControlExtern_ = false;

    bool markerSet_ = false;
    gint markerX_;
    unsigned long markerSample_;

    bool selectionSet_ = false;
    unsigned long selectionStartSample_;
    unsigned long selectionEndSample_;
    gint selectionStart_;
    gint selectionEnd_;
    gint selection_drag_x_;
    gint selection_drag_w_;

    bool regionSet_ = false;
    unsigned long regionStartSample_;
    unsigned long regionEndSample_;

    int selectedTrack_ = 0;
    int selectedIndex_ = 0;

    DragMode dragMode_ = DRAG_NONE;
    gint dragStart_, dragEnd_;
    gint dragStopMin_, dragStopMax_;
    gint dragLastX_;
    sigc::connection drag_timeout;
    bool drag_enabled = false;

    // Event controllers
    Glib::RefPtr<Gtk::GestureClick> click_controller_;
    Glib::RefPtr<Gtk::EventControllerMotion> motion_controller_;

    void scrollTo();
    void redraw(gint x, gint y, gint width, gint height);
    void readSamples(long startBlock, long endBlock);
    void drawSamples();
    void drawMarks(Drawer&);
    void draw_cursor(Drawer&);
    void set_cursor(gint);
    void unset_cursor();
    unsigned long pixel2sample(gint x);
    gint sample2pixel(unsigned long);
    void draw_marker(Drawer&);
    void removeMarker();
    void drawTimeTick(Drawer&, gint x, gint y, unsigned long sample);
    void drawTimeLine(Drawer&);
    void drawTrackMarker(Drawer&, gint x, int trackNr, int indexNr,
                         int selected, int extend);
    void drawTrackLine(Drawer&);
};

#endif

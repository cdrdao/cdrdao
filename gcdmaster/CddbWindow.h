#ifndef __CDDB_WINDOW_H__
#define __CDDB_WINDOW_H__

#include <gtkmm.h>

class TocEdit;
class Cddb;

class CddbWindow : public Gtk::Window
{
public:
    static Glib::RefPtr<CddbWindow> create(Glib::RefPtr<TocEdit> te);

    void setTocEdit(Glib::RefPtr<TocEdit> te);
    void doQuery();
    void getEntry(const std::string& category, const std::string& diskid);

    Cddb* cddb() { return cddb_; }

    enum Error { OK, CONNERR, QUERYERR, READERR };

    sigc::signal<void(Cddb*, Error)> signalQueryDone;
    sigc::signal<void(Cddb*, Error)> signalReadDone;

private:
    CddbWindow();
    virtual ~CddbWindow();

    Glib::RefPtr<TocEdit> tocEdit_;
    Cddb* cddb_ = nullptr;
};

#endif

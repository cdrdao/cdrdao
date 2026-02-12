

#include <gtkmm.h>

class TocEdit;
class Cddb;

class CddbWindow : public Gtk:Window
{
public:
    static Glib::RefPtr<CddbWindow> create(Glib::RefPtr<TocEdit> te);

    void doQuery();
    void getEntry(const std::string& category, const std::string& diskid);

    Cddb* cddb() { return cddb_; }

private:
    CddbWindow(Glib::RefPtr<TocEdit> te);

    sigc::signal<void(Cddb*)> signalQueryDone;
    sigc::signal<void(Cddb*)> signalReadDone;

    Glib::RefPtr<TocEdit> tocEdit_;
    Cddb* cddb_ = nullptr;
};

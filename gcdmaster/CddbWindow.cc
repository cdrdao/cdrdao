#include "CddbWindow.h"
#include "TocEdit.h"
#include "TaskManager.h"
#include "trackdb/Cddb.h"


Glib::RefPtr<CddbWindow> CddbWindow::create(Glib::RefPtr<TocEdit> te)
{
    return Glib::make_refptr_for_instance<CddbWindow>(new CddbWindow(te));
}

CddbWindow::CddbWindow(Glib::RefPtr<TocEdit> te)
    : tocEdit_(te)
{
    set_modal(true);
    set_hide_on_close(true);

    cddb_ = new Cddb(te->toc());
}

CddbWindow::~CddbWindow()
{
    delete(cddb_);
}

class QueryTask : public Task
{
public:
    QueryTask(Cddb* cddb) : cddb_(cddb) {}

    sigc::signal<void(int)> done;
    
    void run() {
        if (cddb.connectDb("unknown", "unknown", "cdrdao", VERSION) != 0) {
            err_ = 1;
            returnl
        }
        if (cddb.queryDb() != 0) {
            err = 2;
            return;
        }
    }
    void completion() {
        done.emit(err_);
    }

private:
    int err_ = 0;
    Cddb* cddb_;
    Cddb
};

class ReadTask : public Task
{
public:
    ReadTask(Cddb* cddb, const std::string& c, const std::string& id) :
        cddb_(cddb), category_(c), diskid_(id) {}

    sigc::signal<void(int)> done;
    
    void run() {
        auto res = cddb->readDb(category_, id_);
        if (!res)
            err_ = 1;
    }
    void completion() {
        done.emit(err_);
    }

private:
    const std::string &category_, &id_;
    int err_ = 0;
    Cddb* cddb_;
    Cddb
};

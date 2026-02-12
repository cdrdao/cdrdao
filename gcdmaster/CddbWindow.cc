#include <cassert>

#include "CddbWindow.h"
#include "TocEdit.h"
#include "TaskManager.h"
#include "trackdb/Cddb.h"


Glib::RefPtr<CddbWindow> CddbWindow::create(Glib::RefPtr<TocEdit> te)
{
    auto obj = Glib::make_refptr_for_instance<CddbWindow>(new CddbWindow());
    obj->setTocEdit(te);
    return obj;
}

CddbWindow::CddbWindow()
{
    set_modal(true);
    set_hide_on_close(true);
}

CddbWindow::~CddbWindow()
{
    delete(cddb_);
}

void CddbWindow::setTocEdit(Glib::RefPtr<TocEdit> te)
{
    assert(te);
    tocEdit_ = te;
    if (cddb_)
	delete cddb_;
    cddb_ = new Cddb(te->toc());
}

class QueryTask : public Task
{
public:
    QueryTask(Cddb* cddb) : cddb_(cddb) {}

    sigc::signal<void(CddbWindow::Error)> done;
    
    void run() {
	sendUpdate("Connecting to CCDB server...");
        if (cddb_->connectDb("unknown", "unknown", "cdrdao", VERSION) != 0) {
            err_ = CddbWindow::CONNERR;
            return;
        }
	sendUpdate("Connection established with CCDB server.");
        if (cddb_->queryDb() != 0) {
            err_ = CddbWindow::QUERYERR;
            return;
        }
    }
    void completed() {
        done.emit(err_);
    }

private:
    CddbWindow::Error err_ = CddbWindow::OK;
    Cddb* cddb_;
};

class ReadTask : public Task
{
public:
    ReadTask(Cddb* cddb, const std::string& c, const std::string& id) :
        cddb_(cddb), category_(c), id_(id) {}

    sigc::signal<void(CddbWindow::Error)> done;
    
    void run() {
        auto res = cddb_->readDb(category_, id_);
        if (!res)
            err_ = CddbWindow::READERR;
    }
    void completed() {
        done.emit(err_);
    }

private:
    const std::string &category_, &id_;
    CddbWindow::Error err_ = CddbWindow::OK;
    Cddb* cddb_;
};

void CddbWindow::doQuery()
{
    auto task = new QueryTask(cddb_);
    task->done.connect([this, task](CddbWindow::Error err) {
	this->signalQueryDone.emit(cddb_, err);
	delete task;
    });

    tocEdit_->taskManager().addJob(task);
}

void CddbWindow::getEntry(const std::string& category, const std::string& diskid)
{
    auto task = new ReadTask(cddb_, category, diskid);
    task->done.connect([this, task](CddbWindow::Error err) {
	this->signalReadDone.emit(cddb_, err);
	delete task;
    });

    tocEdit_->taskManager().addJob(task);
}

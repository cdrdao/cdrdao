/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 2025  Denis Leroy <denis@poolshark.org>
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

#include <glibmm.h>

#include <atomic>
#include <thread>
#include <condition_variable>
#include <queue>
#include <cassert>

#include "TaskManager.h"

class ThreadPool
{
public:
    ThreadPool(unsigned num_threads);
    ~ThreadPool();

    template<class F> void enqueue(F&& f);
    unsigned outstanding_tasks();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

ThreadPool::ThreadPool(unsigned num_threads)
{
    for (unsigned i = 0; i < num_threads; i++) {
	stop = false;
	workers.emplace_back(
	    [this] {
		while(true) {
		    std::function<void()> task;
		    {
			std::unique_lock<std::mutex> lock(queue_mutex);
			condition.wait(lock, [this] {
			    return stop || !tasks.empty();
			});

			if (stop && tasks.empty())
			    return;

			task = std::move(tasks.front());
			tasks.pop();
		    }
		    task();
		}
	    });
    }
}

ThreadPool::~ThreadPool()
{
    {
	std::unique_lock<std::mutex> lock(queue_mutex);
	stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
	if (worker.joinable())
	    worker.join();
    }    
}

template<class F>
void ThreadPool::enqueue(F&& f)
{
    {
	std::unique_lock<std::mutex> lock(queue_mutex);
	tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}

unsigned ThreadPool::outstanding_tasks()
{
    unsigned response;
    {
	std::unique_lock<std::mutex> lock(queue_mutex);
	response = tasks.size();
    }
    return response;
}

void Task::sendUpdate(const std::string& msg)
{
    if (tm_) {
	Glib::signal_idle().connect_once([this, msg](){(this->tm_)->signalJobUpdate(this, msg);});
    }
}

// ********************************************************************

TaskManager::TaskManager(int num_threads)
{
    threadpool = new ThreadPool(num_threads);
    main_thread_id = std::this_thread::get_id();
}

TaskManager::~TaskManager()
{
    delete threadpool;
}

// Runs in main thread
void TaskManager::addJob(Task* job)
{
    assert(std::this_thread::get_id() == main_thread_id);
    if (tasks.size() == 0) {
	total = 0;
	sofar = 0;
	signalQueueStarted();
    }
    tasks.push(job);
    total++;
    job->tm_ = this;

    threadpool->enqueue([this, job]() {this->runJob(job);});
}

// Runs in worker thread
void TaskManager::runJob(Task* job)
{
    assert(std::this_thread::get_id() != main_thread_id);
    Glib::signal_idle().connect_once([this, job]() {this->signalJobStarted(job);});
    job->run();
    Glib::signal_idle().connect_once([this, job]() {this->jobDone(job);});
}

// Runs in main thread.
void TaskManager::jobDone(Task* job)
{
    assert(std::this_thread::get_id() == main_thread_id);
    job->done = true;
    sofar++;

    while (!tasks.empty() && tasks.front()->done) {
	tasks.front()->completed();
	tasks.pop();
    }

    if (tasks.empty())
	signalQueueEmptied();
}

double TaskManager::completion()
{
    if (total == 0)
	return 0.0;
    else
	return (double)sofar / (double)total;
}

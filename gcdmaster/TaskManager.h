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

#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__

#include <queue>
#include <sigc++/signal.h>
#include <string>
#include <thread>

class TaskManager;
class ThreadPool;

// TaskManager. Accepts tasks that will be executed by a worker thread.
//
// A Task has two parts:
//
//   - run() : that function is called in a worker thread and can
//   block and perform potentially long operations.
//
//   - completed() : that function is called within the main thread
//   and therefore cannot block but can safely use GTK+ functions to
//   update the GUI. The completed() functions are guaranteed to be
//   called in the order in which the jobs are added.

class Task
{
  public:
    Task() : done(false)
    {
    }

    // Runs in worker thread.
    virtual void run() = 0;

    // Runs in main thread.
    virtual void completed() = 0;

    // Can be called in worker thread.
    virtual void sendUpdate(const std::string &msg);

    TaskManager *tm_;
    bool done;
};

class TaskManager
{
  public:
    TaskManager(int num_threads = 4);
    ~TaskManager();

    sigc::signal0<void> signalQueueStarted;
    sigc::signal0<void> signalQueueEmptied;
    sigc::signal1<void, Task *> signalJobStarted;
    sigc::signal2<void, Task *, const std::string &> signalJobUpdate;

    void addJob(Task *task);

    int queueSize()
    {
        return tasks.size();
    }
    double completion();
    void resumeCompletions();

  private:
    ThreadPool *threadpool;
    // We keep our own queue to guarantee completion order.
    std::queue<Task *> tasks;
    std::thread::id main_thread_id;

    void runJob(Task *);
    void jobDone(Task *);

    int total, sofar;
};

#endif

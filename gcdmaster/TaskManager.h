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

#include <sigc++/signal.h>
#include <string>

class TaskManager;
class ThreadPool;

class Task
{
public:
    // Runs in worker thread.
    virtual void run() = 0;

    // Runs in main thread.
    virtual void completed() = 0;

    // Can be called in worker thread.
    virtual void sendUpdate(const std::string& msg);

    TaskManager* tm_;
};

class TaskManager
{
public:
    TaskManager();
    ~TaskManager();

    sigc::signal0<void> signalQueueStarted;
    sigc::signal0<void> signalQueueEmptied;
    sigc::signal1<void, Task*> signalJobStarted;
    sigc::signal2<void, Task*, const std::string&> signalJobUpdate;

    void addJob(Task* task);

private:
    unsigned outstanding;
    ThreadPool* threadpool;

    void runJob(Task*);
    void jobDone(Task*);
};

#endif

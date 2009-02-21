/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002  Andreas Mueller <andreas@daneb.de>
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

#include <unistd.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ProcessMonitor.h"
#include "xcdrdao.h"

#include "util.h"
#include "log.h"


Process::Process(int pid, int commFd)
{
  pid_ = pid;
  commFd_ = commFd;

  exited_ = 0;
  exitStatus_ = 0;

  next_ = NULL;
}

Process::~Process()
{
  if (commFd_ >= 0)
    close(commFd_);

  next_ = NULL;
}

int Process::pid() const
{
  return pid_;
}

int Process::commFd() const
{
  return commFd_;
}

int Process::exited() const
{
  return exited_;
}

int Process::exitStatus() const
{
  return exitStatus_;
}


ProcessMonitor::ProcessMonitor()
{
  processes_ = NULL;
  statusChanged_ = 0;
}

ProcessMonitor::~ProcessMonitor()
{
  Process *next;

  blockProcessMonitorSignals();

  while (processes_ != NULL) {
    next = processes_->next_;
    delete processes_;
    processes_ = next;
  }

  unblockProcessMonitorSignals();
}

int ProcessMonitor::statusChanged()
{
  int s = statusChanged_;

  statusChanged_ = 0;
  return s;
}

/* Starts a child process 'prg' with arguments 'args'.
 * If 'pipeFdArgNum' is > 0 the file descriptor number will be written to
 * 'args[pipeFdArgNum]'.
 * Return: newly allocated 'Process' object or NULL on error
 */

Process *ProcessMonitor::start(const char *prg, const char **args,
			       int pipeFdArgNum)
{
  int pid;
  Process *p;
  int pipeFds[2];
  char buf[20];

  if (pipe(pipeFds) != 0) {
    log_message(-2, "Cannot create pipe: %s", strerror(errno));
    return NULL;
  }
  
  if (pipeFdArgNum > 0) {
    sprintf(buf, "%d", pipeFds[1]);
    args[pipeFdArgNum] = buf;
  }

  log_message(0, "Starting: ");
  for (int i = 0; args[i] != NULL; i++)
    log_message(0, "%s ", args[i]);
  log_message(0, "");


  blockProcessMonitorSignals();

  pid = fork();

  if (pid == 0) {
    // we are the new process

    // detach from controlling terminal
    setsid();

    // close reading end of pipe
    close(pipeFds[0]);

    execvp(prg, (char*const*)args);

    log_message(-2, "Cannot execute '%s': %s", prg, strerror(errno));
    _exit(255);
  }
  else if (pid < 0) {
    log_message(-2, "Cannot fork: %s", strerror(errno));
    unblockProcessMonitorSignals();
    return NULL;
  }


  // close writing end of pipe
  close(pipeFds[1]);

  p = new Process(pid, pipeFds[0]);
  
  p->next_ = processes_;
  processes_ = p;

  unblockProcessMonitorSignals();

  return p;
}

Process *ProcessMonitor::find(Process *p, Process **pred)
{
  Process *run;

  for (*pred = NULL, run = processes_; run != NULL;
       *pred = run, run = run->next_) {
    if (p == run) {
      return run;
    }
  }

  return NULL;
}

Process *ProcessMonitor::find(int pid)
{
  Process *run;

  for (run = processes_; run != NULL; run = run->next_) {
    if (run->pid() == pid) {
      return run;
    }
  }

  return NULL;
}

void ProcessMonitor::stop(Process *p)
{
  kill(p->pid(), SIGTERM);
}

void ProcessMonitor::remove(Process *p)
{
  Process *act, *pred;

  blockProcessMonitorSignals();

  if ((act = find(p, &pred)) != NULL) {
    if (pred == NULL)
      processes_ = act->next_;
    else
      pred->next_ = act->next_;

    delete act;
  }

  unblockProcessMonitorSignals();

}

void ProcessMonitor::handleSigChld()
{
  int pid;
  Process *p;
  int status;
  
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if ((p = find(pid)) != NULL) {
      p->exited_ = 1;
      
      if (WIFEXITED(status)) {
	p->exitStatus_ = WEXITSTATUS(status);
      }
      else if (WIFSIGNALED(status)) {
	p->exitStatus_ = 254;
      }
      else {
	p->exitStatus_ = 253;
      }

      statusChanged_ = 1;
    }
    else {
      log_message(-3, "Unknown child with pid %d exited.", pid);
    }
  }

  /*
  if (pid < 0) 
    log_message(-2, "waitpid failed: %s", strerror(errno));
    */
}


/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
/*
 * $Log: ProcessMonitor.h,v $
 * Revision 1.1  2000/02/05 01:38:46  llanero
 * Initial revision
 *
 */

#ifndef __PROCESS_MONITOR_H__
#define __PROCESS_MONITOR_H__

class Process {
public:
  Process(int pid, int commFd);
  ~Process();

  int pid() const;
  int commFd() const;

  int exited() const;
  int exitStatus() const;

private:
  friend class ProcessMonitor;

  int pid_;
  int commFd_;

  int exited_;
  int exitStatus_;

  Process *next_;
};

class ProcessMonitor {
public:
  ProcessMonitor();
  ~ProcessMonitor();

  int statusChanged();

  Process *start(const char *, char *const args[]);
  void stop(Process *);

  void remove(Process *);

  void handleSigChld();

private:
  int statusChanged_;
  Process *processes_;

  Process *find(Process *, Process **pred);
  Process *find(int pid);

};

#endif

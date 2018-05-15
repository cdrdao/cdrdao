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
 * Revision 1.4  2009/02/21 13:49:17  poolshark
 * Compile warning fixes
 *
 * Revision 1.3  2004/02/12 01:13:31  poolshark
 * Merge from gnome2 branch
 *
 * Revision 1.2.6.1  2004/01/05 00:34:02  poolshark
 * First checking of gnome2 port
 *
 * Revision 1.1.1.1  2003/12/09 05:32:28  denis
 * Fooya
 *
 * Revision 1.2  2000/10/08 16:39:41  andreasm
 * Remote progress message now always contain the track relative and total
 * progress and the total number of processed tracks.
 *
 * Revision 1.1.1.1  2000/02/05 01:38:46  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
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

  Process *start(const char *, const char **args, int pipeFdArgNum);
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

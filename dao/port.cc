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
 * $Log: port.cc,v $
 * Revision 1.1  2000/02/05 01:38:22  llanero
 * Initial revision
 *
 * Revision 1.1  1999/05/11 20:03:29  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: port.cc,v 1.1 2000/02/05 01:38:22 llanero Exp $";

#include <config.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#ifdef USE_POSIX_THREADS
#include <pthread.h>
#endif

#if defined(HAVE_USLEEP)

#else

#include <sys/time.h>
#include <sys/types.h>

#endif

#include "port.h"

#include "util.h"

void mSleep(long milliSeconds)
{
#if defined(HAVE_USLEEP)

  usleep(milliSeconds * 1000);

#else

  struct timeval tv;

  tv.tv_sec = 0;
  tv.tv_usec = 1000 * milliSeconds;
  select(0, NULL, NULL, NULL, &tv);

#endif
}

// Installs signal handler for signal 'sig' that will stay installed after
// it is called.
void installSignalHandler(int sig, SignalHandler handler)
{
  struct sigaction action;

  memset(&action, 0, sizeof(action));

  action.sa_handler = handler;
  sigemptyset(&(action.sa_mask));

  if (sigaction(sig, &action, NULL) != 0) 
    message(-2, "Cannot install signal handler: %s", strerror(errno));
}

// Blocks specified signal.
void blockSignal(int sig)
{
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, sig);

#ifdef USE_POSIX_THREADS

#ifdef HAVE_PTHREAD_SIGMASK
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

#else
  sigprocmask(SIG_BLOCK, &set, NULL);
#endif
}

// Unblocks specified signal.
void unblockSignal(int sig)
{
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, sig);

#ifdef USE_POSIX_THREADS

#ifdef HAVE_PTHREAD_SIGMASK
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
#endif

#else
  sigprocmask(SIG_UNBLOCK, &set, NULL);
#endif
}

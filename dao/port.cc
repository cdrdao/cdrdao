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
 * Revision 1.2  2000/11/12 16:50:44  andreasm
 * Fixes for compilation under Win32.
 *
 * Revision 1.1.1.1  2000/02/05 01:38:22  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/05/11 20:03:29  mueller
 * Initial revision
 *
 */

static char rcsid[] = "$Id: port.cc,v 1.2 2000/11/12 16:50:44 andreasm Exp $";

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

#ifdef _WIN32
#include <vadefs.h>
#include <Windows32/Base.h>
#include <Windows32/Defines.h>
#include <Windows32/Structures.h>
#include <Windows32/Functions.h>
#endif


/* Select POSIX scheduler interface for real time scheduling if possible */
#ifdef USE_POSIX_THREADS

#undef LINUX_QNX_SCHEDULING

#if (defined HAVE_PTHREAD_GETSCHEDPARAM) && (defined HAVE_SCHED_GET_PRIORITY_MAX) && (defined HAVE_PTHREAD_ATTR_SETSCHEDPOLICY) && (defined HAVE_PTHREAD_ATTR_SETSCHEDPARAM) && (defined HAVE_PTHREAD_SETSCHEDPARAM) && (!defined LINUX_QNX_SCHEDULING)
#define POSIX_SCHEDULING
#endif

#else

#if (defined HAVE_SCHED_GETPARAM) && (defined HAVE_SCHED_GET_PRIORITY_MAX) && (defined HAVE_SCHED_SETSCHEDULER) && (!defined LINUX_QNX_SCHEDULING)
#define POSIX_SCHEDULING
#endif

#endif


#if defined LINUX_QNX_SCHEDULING

#include <sys/types.h>

#define SCHED_OTHER     0
#define SCHED_FIFO      1
#define SCHED_RR        2

struct sched_param {
  unsigned int priority;
  int fork_penalty_threshold;
  unsigned int starvation_threshold;
  unsigned int ts_max;
  unsigned int run_q, run_q_min, run_q_max;
};
extern "C" int sched_setparam __P((pid_t __pid, const struct sched_param *__param));
extern "C" int sched_getparam __P((pid_t __pid, struct sched_param *__param));
extern "C" int sched_setscheduler __P((pid_t __pid, int __policy, const struct sched_param *__param));
extern "C" int sched_getscheduler __P((pid_t __pid));

#elif defined POSIX_SCHEDULING

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

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


/* Sets real time scheduling.
 * int priority: priority which is subtracted from the maximum priority
 * Return: 0: OK
 *         1: no permissions
 *         2: real time scheduling not available
 *         3: error occured
 */
int setRealTimeScheduling(int priority)
{
#if defined(_WIN32)
  if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
    message(-1, "Cannot set real time priority class.");
    return 3;
  }

  if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
    message(-1, "Cannot set real time priority.");
    return 3;
  }

  message(4, "Using WIN32 real time scheduling.");

#elif defined(USE_POSIX_THREADS) && defined(POSIX_SCHEDULING)
  struct sched_param schedp;
  int schedPolicy;

  if (geteuid() != 0) {
    return 1;
  }

  pthread_getschedparam(pthread_self(), &schedPolicy, &schedp);
  schedp.sched_priority = sched_get_priority_max(SCHED_RR) - priority;

  if (pthread_setschedparam(pthread_self(), SCHED_RR, &schedp) != 0) {
    message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    message(4, "Using pthread POSIX real time scheduling.");
  }

#elif defined(LINUX_QNX_SCHEDULING)
  struct sched_param schedp;

  if (geteuid() != 0) {
    return 1;
  }
  
  sched_getparam (0, &schedp);
  schedp.run_q_min = schedp.run_q_max = 2;
  if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
    message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    message(4, "Using Linux QNX real time scheduling.");
  }

#elif defined POSIX_SCHEDULING
  struct sched_param schedp;

  if (geteuid() != 0) {
    return 1;
  }

  sched_getparam (0, &schedp);
  schedp.sched_priority = sched_get_priority_max (SCHED_RR) - priority;
  if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
    message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    message(4, "Using POSIX real time scheduling.");
  }
#else
  return 2;
#endif

  return 0;
}

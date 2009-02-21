/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

#ifdef __CYGWIN__
#include <windows.h>
#endif

#ifdef UNIXWARE
#include <sys/priocntl.h>
#include <sys/fppriocntl.h>
extern uid_t geteuid();
#endif

#include "log.h"

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

#include "log.h"

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

#ifdef UNIXWARE
  action.sa_handler = (void(*)()) handler;
#else
  action.sa_handler = handler;
#endif

  sigemptyset(&(action.sa_mask));

  if (sigaction(sig, &action, NULL) != 0) 
    log_message(-2, "Cannot install signal handler: %s", strerror(errno));
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
#if defined(__CYGWIN__)
  if (!SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS)) {
    log_message(-1, "Cannot set real time priority class.");
    return 3;
  }

  if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL)) {
    log_message(-1, "Cannot set real time priority.");
    return 3;
  }

  log_message(5, "Using WIN32 real time scheduling.");

#elif defined(USE_POSIX_THREADS) && defined(POSIX_SCHEDULING)
  struct sched_param schedp;
  int schedPolicy;

  if (geteuid() != 0) {
    return 1;
  }

  pthread_getschedparam(pthread_self(), &schedPolicy, &schedp);
  schedp.sched_priority = sched_get_priority_max(SCHED_RR) - priority;

  if (pthread_setschedparam(pthread_self(), SCHED_RR, &schedp) < 0) {
    log_message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    log_message(5, "Using pthread POSIX real time scheduling.");
  }

#elif defined(LINUX_QNX_SCHEDULING)
  struct sched_param schedp;

  if (geteuid() != 0) {
    return 1;
  }
  
  sched_getparam (0, &schedp);
  schedp.run_q_min = schedp.run_q_max = 2;
  if (sched_setscheduler (0, SCHED_RR, &schedp) < 0) {
    log_message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    log_message(5, "Using Linux QNX real time scheduling.");
  }

#elif defined POSIX_SCHEDULING
  struct sched_param schedp;

  if (geteuid() != 0) {
    return 1;
  }

  sched_getparam (0, &schedp);
  schedp.sched_priority = sched_get_priority_max (SCHED_RR) - priority;
  if (sched_setscheduler (0, SCHED_RR, &schedp) < 0) {
    log_message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    return 3;
  }
  else {
    log_message(5, "Using POSIX real time scheduling.");
  }

#elif defined UNIXWARE
  /* Switch to fixed priority scheduling for this process */
  pcinfo_t        pci;
  pcparms_t       pcp;
  fpparms_t       *fp;

  if (geteuid() != 0) {
    return 1;
  }
 
  /* set fixed priority class */
  strcpy(pci.pc_clname, "FP");
  fp = (fpparms_t *) &pcp.pc_clparms;
  fp->fp_pri = FP_NOCHANGE;
  fp->fp_tqsecs = (ulong_t) FP_TQDEF;
  fp->fp_tqnsecs = FP_TQDEF;
  
  if (priocntl(P_PID, 0, PC_GETCID, (void *) &pci) < 0) {
    log_message(-1, "priocntl PC_GETCID failed");
    return 3;
  }
 
  pcp.pc_cid = pci.pc_cid;
 
  if (priocntl(P_PID, getpid(), PC_SETPARMS, (void *) &pcp) < 0) {
    log_message(-1, "priocntl PC_SETPARMS failed");
    return 3;
  }
 
  log_message(5, "Now running in fixed-priority scheduling mode.");

#else
  return 2;
#endif

  return 0;
}

// Give up root privileges. Returns true if succeeded or no action was
// taken.

bool giveUpRootPrivileges()
{
    if (geteuid() != getuid()) {
#if defined(HAVE_SETREUID)
        if (setreuid((uid_t)-1, getuid()) != 0)
            return false;
#elif defined(HAVE_SETEUID)
        if (seteuid(getuid()) != 0)
            return false;
#elif defined(HAVE_SETUID)
        if (setuid(getuid()) != 0)
            return false;
#else
        return false;
#endif
    }

    if (getegid() != getgid()) {
#if defined(HAVE_SETREGID)
        if (setregid((gid_t)-1, getgid()) != 0)
            return false;
#elif defined(HAVE_SETEGID)
        if (setegid(getgid()) != 0)
            return false;
#elif defined(HAVE_SETGID)
        if (setgid(getgid()) != 0)
            return false;
#else
        return false;
#endif
    }

    return true;
}

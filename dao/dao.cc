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
 * $Log: dao.cc,v $
 * Revision 1.1  2000/02/05 01:38:06  llanero
 * Initial revision
 *
 * Revision 1.12  1999/05/11 20:04:09  mueller
 * SYSV message queues are not used anymore. The communication between
 * reader and writer is based on a polling mechanism, now.
 * Added posix thread support.
 *
 * Revision 1.11  1999/03/27 20:57:27  mueller
 * Improved buffer code. Does not use semaphores anymore.
 * The reader is now the forked process.
 *
 * Revision 1.10  1999/01/24 16:03:57  mueller
 * Applied Radek Doulik's ring buffer patch. Added some cleanups and
 * improved behavior in error case.
 *
 * Revision 1.9  1998/10/03 14:33:46  mueller
 * Applied patch from Bjoern Fischer <bfischer@Techfak.Uni-Bielefeld.DE>:
 * Cosmetic changes and correction of real time scheduling selection.
 *
 * Revision 1.8  1998/09/22 19:14:29  mueller
 * Added locking of memory pages.
 *
 * Revision 1.7  1998/09/21 19:35:51  mueller
 * Added real time scheduling. Submitted by Bjoern Fischer <bfischer@Techfak.Uni-Bielefeld.DE>.
 *
 * Revision 1.6  1998/09/06 13:34:22  mueller
 * Use 'message()' for printing messages.
 *
 * Revision 1.5  1998/08/30 19:21:15  mueller
 * Rearranged the code.
 *
 * Revision 1.4  1998/08/15 20:47:16  mueller
 * Added support for GenericMMC driver.
 *
 */

static char rcsid[] = "$Id: dao.cc,v 1.1 2000/02/05 01:38:06 llanero Exp $";

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#ifdef linux
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/sysctl.h>
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#ifdef USE_POSIX_THREADS
#include <pthread.h>
#include <sched.h>
#else
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include "port.h"

#define DEBUG_WRITE 0

#if defined(__FreeBSD__)
#define IPC_ARG_T void
#else
#define IPC_ARG_T msgbuf
#endif


/* Select POSIX scheduler interface for real time scheduling if possible */
#if (defined HAVE_SCHED_GETPARAM) && (defined HAVE_SCHED_GET_PRIORITY_MAX) && (defined HAVE_SCHED_SETSCHEDULER) && (!defined LINUX_QNX_SCHEDULING)
#define POSIX_SCHEDULING
#endif

#ifdef LINUX_QNX_SCHEDULING

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

#include <sched.h>

#endif

#include "dao.h"
#include "util.h"
#include "remote.h"

struct ShmSegment {
  int id;
  char *buffer;
};

struct Buffer {
  long bufLen;  // number of blocks in buffer that should be written
  TrackData::Mode mode; // data mode for writing
  TrackData::Mode trackMode; // mode of track may differ from 'mode' if data
                             // blocks must be encoded in audio blocks,
                             // only used for message printing
  int trackNr; // if != 0 a new track with given number has started

  char *buffer; // address of buffer that should be written
};
  
struct BufferHeader {
  long buffersRead;    // number of blocks that are read and put to the buffer
  long buffersWritten; // number of blocks that were taken from the buffer
  int buffersFilled;   // set to 1 by reader process when buffer is filled the
                       // first time
  int readerFinished;  
  int readerTerminated;
  int terminateReader;

  long nofBuffers;     // number of available buffers
  Buffer *buffers;
};

// buffer size in blocks
int BUFFER_SIZE = 75;

static int TERMINATE = 0;




static int getSharedMemory(long nofBuffers, BufferHeader **header,
			   long *nofSegments, ShmSegment **shmSegments);
static void releaseSharedMemory(long nofSegments, ShmSegment *shmSegments);



static RETSIGTYPE terminationRequest(int sig)
{
  if (sig == SIGQUIT || sig == SIGTERM) 
    TERMINATE = 1;

#if 0
  if (sig == SIGCHLD) {
    message(0, "SIGCHLD received.");
  }
#endif
}


#ifndef USE_POSIX_THREADS
// Waits or polls for termination of a child process.
// noHang: 0: wait until child terminates, 1: just poll if child terminated
// status: filled with status information, only valid if 0 is returned
// return: 0: child exited
//         1: no child exited, can only happen if 'noHang' is 1
//         2: wait failed, 'errno' contains cause
static int waitForChild(int noHang, int *status)
{
  int ret;

  do {
    if (noHang)
      ret = wait3(status, WNOHANG, NULL);
    else
      ret = wait(status);

    if (ret > 0)
      return 0;

    if (ret < 0 && errno != EINTR 
#ifdef ERESTARTSYS
	&& errno != ERESTARTSYS
#endif
	) {
      return 2;
    }
  } while (ret < 0);

  return 1;
}

#endif

// Blocks all signals that are handled by this module.
static void blockSignals()
{
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  sigaddset(&set, SIGQUIT);
  sigaddset(&set, SIGTERM);

#ifdef USE_POSIX_THREADS

#ifdef HAVE_PTHREAD_SIGMASK
  pthread_sigmask(SIG_BLOCK, &set, NULL);
#endif

#else
  sigprocmask(SIG_BLOCK, &set, NULL);
#endif
}

// Blocks all signals that are handled by this module.
static void unblockSignals()
{
  sigset_t set;

  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  sigaddset(&set, SIGQUIT);
  sigaddset(&set, SIGTERM);

#ifdef USE_POSIX_THREADS

#ifdef HAVE_PTHREAD_SIGMASK
  pthread_sigmask(SIG_UNBLOCK, &set, NULL);
#endif

#else
  sigprocmask(SIG_UNBLOCK, &set, NULL);
#endif
}

// return: 0: OK 
//         1: child process terminated and has been collected with 'wait()'
//         2: error -> child process must be terminated
static int writer(CdrDriver *cdr, long total, BufferHeader *header, long lba,
		  long remoteMode)
{
  long cnt = 0;
  long blkCount = 0;
  long len = 0;
  long cntMb;
  long lastMb = 0;
  long buffered;
  int buffFill;
  int minFill = 100;
  int maxFill = 0;
  int actTrackNr = 0;
  long actProgress;
  TrackData::Mode dataMode;
  DaoWritingProgress remoteMsg;
  char remoteMsgSync[4];
#ifndef USE_POSIX_THREADS
  int status;
#endif

  remoteMsgSync[0] = 0xff;
  remoteMsgSync[1] = 0x00;
  remoteMsgSync[2] = 0xff;
  remoteMsgSync[3] = 0x00;

  message (3, "Waiting for reader process");

  while (header->buffersFilled == 0) {
    sleep(1);

    if (header->readerTerminated) {
      message(-2, "Reader process terminated abnormally.");
      return 1;
    }

#ifndef USE_POSIX_THREADS
  // Check if child has terminated
    switch (waitForChild(1, &status)) {
    case 0: // Child exited
      message(-2, "Reader process terminated abnormally.");
      return 1;
    case 2:
      message(-2, "wait failed: %s", strerror(errno));
      return 2;
    }
#endif
  }

#if DEBUG_WRITE
  FILE *fp = fopen("test.out", "w");
#endif

  message (3, "Awaken, will start writing");

  if (cdr != NULL) {
    if (remoteMode) {
      remoteMsg.status = 1; // writing lead-in
      remoteMsg.track = 0;
      remoteMsg.totalProgress = 0;
      remoteMsg.bufferFillRate = 100;

      if (write(3/*fileno(stdout)*/, remoteMsgSync, sizeof(remoteMsgSync)) < 0 ||
	  write(3/*fileno(stdout)*/, (const char*)&remoteMsg,
		sizeof(remoteMsg)) < 0) {
	message(-1, "Disabling remote messages.");
	remoteMode = 0;
      }
    }

    blockSignals();
    if (cdr->startDao() != 0) {
      unblockSignals();
      return 2;
    }
    unblockSignals();
  }

  do {
    //message(3, "Slave: waiting for master.");

    while (header->buffersWritten == header->buffersRead) {
      if (header->readerTerminated) {
	message(-2, "Reader process terminated abnormally.");
	return 1;
      }

#ifndef USE_POSIX_THREADS
      // Check if child has terminated
      switch (waitForChild(1, &status)) {
      case 0: // Child exited
	message(-2, "Reader process terminated abnormally.");
	return 1;
      case 2:
	message(-2, "wait failed: %s", strerror(errno));
	return 2;
      }
#endif

      mSleep(20);
    }

    Buffer &buf = header->buffers[header->buffersWritten % header->nofBuffers];
    len = buf.bufLen;
    dataMode = buf.mode;

    if (header->readerFinished) {
      buffFill = 100;
      if (maxFill == 0)
	maxFill = 100;
    }
    else {
      buffered = header->buffersRead - header->buffersWritten;

      if (buffered == header->nofBuffers ||
	  buffered == header->nofBuffers - 1) {
	buffFill = 100;
      }
      else {
	buffFill = 100 * buffered;
	buffFill /= header->nofBuffers;
      }

      if (buffFill > maxFill)
	maxFill = buffFill;
    }

    if (buffFill < minFill)
      minFill = buffFill;

    if (len == 0) {
      // all data is written
      message(1, "");
      if (cdr == NULL)
	message(1, "Read %ld blocks.", blkCount);
      else
	message(1, "Wrote %ld blocks. Buffer fill min %d%%/max %d%%.",
		blkCount, minFill, maxFill);

#if DEBUG_WRITE
      if (fp != NULL)
	fclose(fp);
#endif

      if (remoteMode) {
	remoteMsg.status = 3; // writing lead-out
	remoteMsg.track = 0xaa;
	remoteMsg.totalProgress = 1000;
	remoteMsg.bufferFillRate = 100;

	if (write(3/*fileno(stdout)*/, remoteMsgSync, sizeof(remoteMsgSync)) < 0 ||
	    write(3/*fileno(stdout)*/, (const char*)&remoteMsg,
		  sizeof(remoteMsg)) < 0) {
	  message(-1, "Disabling remote messages.");
	  remoteMode = 0;
	}
      }

      if (cdr != NULL) {
	blockSignals();
	if (cdr->finishDao() != 0) {
	  unblockSignals();
	  return 2;
	}
	unblockSignals();
      }

      return 0;
    }

    cnt += len * AUDIO_BLOCK_LEN;
    blkCount += len;

    if (buf.trackNr > 0) {
      message(1, "Writing track %02d (mode %s/%s)...", buf.trackNr,
	      TrackData::mode2String(buf.trackMode),
	      TrackData::mode2String(dataMode));

      actTrackNr = buf.trackNr;
    }

    //message(3, "Slave: writing buffer %p (%ld).", buf, len);

#if DEBUG_WRITE
    if (fp != NULL) {
      if (cdr != NULL)
	fwrite(buf.buffer, cdr->blockSize(dataMode), len, fp);
      else
	fwrite(buf.buffer, 2352, len, fp);
    }	
#endif

    if (cdr != NULL) {
      blockSignals();
      if (cdr->writeData(dataMode, lba, buf.buffer, len) != 0) {
	message(-2, "Writing failed - buffer under run?");
	unblockSignals();
	return 2;
      }
      else {
	cntMb = cnt >> 20;
	if (cntMb > lastMb) {
	  message(1, "Wrote %ld of %ld MB (Buffer %3d%%).\r", cnt >> 20,
		  total >> 20, buffFill);
	  lastMb = cntMb;
	}
      }
      unblockSignals();
    }
    else {
      message(1, "Read %ld of %ld MB.\r", cnt >> 20, total >> 20);
    }

    if (remoteMode) {
      actProgress = cnt;
      actProgress /= total / 1000;

      remoteMsg.status = 2; // writing data
      remoteMsg.track = actTrackNr;
      remoteMsg.totalProgress = actProgress;
      remoteMsg.bufferFillRate = buffFill;
      
      if (write(3/*fileno(stdout)*/, remoteMsgSync, sizeof(remoteMsgSync)) < 0 ||
	  write(3/*fileno(stdout)*/, (const char*)&remoteMsg,
		sizeof(remoteMsg)) < 0) {
	message(-1, "Disabling remote messages.");
	remoteMode = 0;
      }
    }


    header->buffersWritten += 1;

  } while (!TERMINATE);

  message(-1, "Writing/simulation/read-test aborted on user request.");

  return 2;
}

struct ReaderArgs {
  const Toc *toc;
  CdrDriver *cdr;
  int swap;
  BufferHeader *header;
  long startLba;
};

static void *reader(void *args)
{
  const Toc *toc = ((ReaderArgs*)args)->toc;
  CdrDriver *cdr = ((ReaderArgs*)args)->cdr;
  int swap = ((ReaderArgs*)args)->swap;
  BufferHeader *header = ((ReaderArgs*)args)->header;
  long lba = ((ReaderArgs*)args)->startLba + 150; // used to encode the sector
                                                  // header (MSF)

  long length = toc->length().lba();
  long n, rn;
  int first = header->nofBuffers;
  const Track *track;
  int trackNr = 1;
  Msf tstart, tend;
  TrackData::Mode dataMode;
  int encodingMode = 1;
  int newTrack;

  if (cdr != NULL) {
    if (cdr->bigEndianSamples() == 0) {
      // swap samples for little endian recorders
      swap = !swap;
    }
    encodingMode = cdr->encodingMode();
  }
  message(3, "Swap: %d", swap);

  TrackIterator itr(toc);
  TrackReader reader;

  track = itr.first(tstart, tend);
  reader.init(track);

  if (reader.openData() != 0) {
    message(-2, "Opening of track data failed.");
    goto fail;
  }
  newTrack = 1;

  dataMode = (encodingMode == 0) ? TrackData::AUDIO : track->type();

  do {
    n = (length > BUFFER_SIZE ? BUFFER_SIZE : length);

    Buffer &buf = header->buffers[header->buffersRead % header->nofBuffers];

    do {
      rn = reader.readData(encodingMode, lba, buf.buffer, n);
    
      if (rn < 0) {
	message(-2, "Reading of track data failed.");
	goto fail;
      }
      
      if (rn == 0) {
	track = itr.next(tstart, tend);
	reader.init(track);

	if (reader.openData() != 0) {
	  message(-2, "Opening of track data failed.");
	  goto fail;
	}
	trackNr++;
	if (encodingMode != 0)
	  dataMode = track->type();

	newTrack = 1;
      }
    } while (rn == 0);

    lba += rn;

    if (track->type() == TrackData::AUDIO) {
      if (swap) {
	// swap audio data
	swapSamples((Sample *)(buf.buffer), rn * SAMPLES_PER_BLOCK);
      }
    }
    else {
      if (encodingMode == 0 && cdr != NULL && cdr->bigEndianSamples() == 0) {
	// swap encoded data blocks
	swapSamples((Sample *)(buf.buffer), rn * SAMPLES_PER_BLOCK);
      }
    }

    // notify writer that it can write buffer
    buf.bufLen = rn;
    buf.mode = dataMode;
    buf.trackMode = track->type();

    if (newTrack) {
      // inform write process that it should print message about new track
      buf.trackNr = trackNr;
    }
    else {
      buf.trackNr = 0;
    }

    header->buffersRead += 1;

    length -= rn;

    if (first > 0) {
      first--;
      if (first == 0 || length == 0) {
	message (3, "Buffer filled");

	header->buffersFilled = 1;
      }
    }
    
    // wait for writing process to finish writing of previous buffer
    //message(3, "Reader: waiting for Writer.");
    while (header->buffersRead - header->buffersWritten 
	   == header->nofBuffers &&
	   header->terminateReader == 0) {
      mSleep(20);
    }


    newTrack = 0;
  } while (length > 0 && header->terminateReader == 0);

  header->readerFinished = 1;

  if (header->terminateReader == 0) {
    Buffer &buf1 = header->buffers[header->buffersRead % header->nofBuffers];
    buf1.bufLen = 0;
    buf1.trackNr = 0;
    header->buffersRead += 1;
  }

#ifndef USE_POSIX_THREADS
  // wait until we get killed
  while (1)
    sleep(1000);

  exit(0);
#endif

  return NULL;

fail:
  header->readerTerminated = 1;

#ifndef USE_POSIX_THREADS
  exit(1);
#endif

  return NULL;
}


int writeDiskAtOnce(const Toc *toc, CdrDriver *cdr, int nofBuffers, int swap,
		    int testMode, int remoteMode)
{
  long length = toc->length().lba();
  long total = length * AUDIO_BLOCK_LEN;
  int err = 0;
  BufferHeader *header = NULL;
  long nofShmSegments = 0;
  ShmSegment *shmSegments = NULL;
  long startLba = 0;

#if defined(POSIX_SCHEDULING) || defined(LINUX_QNX_SCHEDULING) || \
    defined(USE_POSIX_THREADS)
  struct sched_param schedp;
#endif

#ifdef USE_POSIX_THREADS
  pthread_t readerThread;
  pthread_attr_t readerThreadAttr;
  int threadStarted = 0;
#else
  int pid = 0;
  int status;
#endif

#if 1
  if (nofBuffers < 10) {
    nofBuffers = 10;
    message(-1, "Adjusted number of FIFO buffers to 10.");
  }
#endif

  if (remoteMode) {
    // switch stdout to non blocking IO
    int flags;

    if ((flags = fcntl(3/*fileno(stdout)*/, F_GETFL)) == -1) {
      message(-2, "Cannot get flags of stdout: %s", strerror(errno));
      return 1;
    }

    flags |= O_NONBLOCK;

    if (fcntl(3/*fileno(stdout)*/, F_SETFL, flags) < 0) {
      message(-2, "Cannot set flags of stdout: %s", strerror(errno));
      return 1;
    }
  }

  if (getSharedMemory(nofBuffers, &header, &nofShmSegments,
		      &shmSegments)  != 0) {
    releaseSharedMemory(nofShmSegments, shmSegments);
    return 1;
  }

  header->buffersRead = 0;
  header->buffersWritten = 0;
  header->buffersFilled = 0;
  header->readerFinished = 0;
  header->readerTerminated = 0;
  header->terminateReader = 0;

  TERMINATE = 0;

  installSignalHandler(SIGINT, SIG_IGN);
  installSignalHandler(SIGPIPE, SIG_IGN);
  installSignalHandler(SIGALRM, SIG_IGN);
  installSignalHandler(SIGCHLD, terminationRequest);
  installSignalHandler(SIGQUIT, terminationRequest);
  installSignalHandler(SIGTERM, terminationRequest);

  if (!testMode) {
    const DiskInfo *di;

    if (cdr->initDao(toc) != 0) {
      err = 1; goto fail;
    }

    if ((di = cdr->diskInfo()) != NULL) {
      startLba = di->thisSessionLba;
    }
  }

  // start reader process
#ifdef USE_POSIX_THREADS

  if (pthread_attr_init(&readerThreadAttr) != 0) {
    message(-2, "pthread_attr_init failed: %s", strerror(errno));
    err = 1; goto fail;
  }
  

#if defined(POSIX_SCHEDULING) && defined(HAVE_PTHREAD_ATTR_SETSCHEDPOLICY) && \
  defined(HAVE_PTHREAD_ATTR_SETSCHEDPARAM)

  if (geteuid() == 0) {
    if (pthread_attr_setschedpolicy(&readerThreadAttr, SCHED_RR) == 0) {
      struct sched_param schedParam;

      if (pthread_attr_getschedparam(&readerThreadAttr, &schedParam) == 0) {
	schedParam.sched_priority = sched_get_priority_max(SCHED_RR) - 4;
	if (pthread_attr_setschedparam(&readerThreadAttr, &schedParam) != 0) {
	  message(-1, "pthread_attr_setschedparam failed: %s",
		  strerror(errno));
	}
      }
    }
  }
#else

  message(-1, "Real time scheduling is not available for reader thread.");

#endif

  ReaderArgs rargs;

  rargs.toc = toc;
  rargs.cdr = cdr;
  rargs.swap = swap;
  rargs.header = header;
  rargs.startLba = startLba;

  if (pthread_create(&readerThread, &readerThreadAttr, reader, &rargs) != 0) {
    message(-2, "Cannot create thread: %s", strerror(errno));
    pthread_attr_destroy(&readerThreadAttr);
    err = 1; goto fail;
  }
  else {
    threadStarted = 1;

    
  }

#else /* USE_POSIX_THREADS */

  if ((pid = fork()) == 0) {
    // we are the new process

    setsid(); // detach from controlling terminal

#ifdef LINUX_QNX_SCHEDULING

    if (geteuid() == 0) {
      sched_getparam (0, &schedp);
      schedp.run_q_min = schedp.run_q_max = 1;
      if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
	message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
      }
    }

#elif defined POSIX_SCHEDULING

    if (geteuid() == 0) {
      sched_getparam (0, &schedp);
      schedp.sched_priority = sched_get_priority_max (SCHED_RR) - 4;
      if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
	message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
      }
    }

#endif

#ifdef HAVE_MLOCKALL
    if (geteuid() == 0) {
      if (mlockall(MCL_CURRENT|MCL_FUTURE) != 0) {
	message(-1, "Cannot lock memory pages: %s", strerror(errno));
      }
      message(3, "Reader process memory locked");
    }
#endif

    ReaderArgs rargs;

    rargs.toc = toc;
    rargs.cdr = cdr;
    rargs.swap = swap;
    rargs.header = header;
    rargs.startLba = startLba;

    reader(&rargs);
  }
  else if (pid < 0) {
    message(-2, "fork failed: %s", strerror(errno));
    err = 1; goto fail;
  }
#endif /* USE_POSIX_THREADS */

  // setup scheduler for writing process/thread
#if defined(USE_POSIX_THREADS)

#if defined(HAVE_PTHREAD_GETSCHEDPARAM) && defined(HAVE_PTHREAD_SETSCHEDPARAM)
  if (geteuid() == 0) {
    int schedPolicy;

    pthread_getschedparam(pthread_self(), &schedPolicy, &schedp);
    schedp.sched_priority = sched_get_priority_max(SCHED_RR) - 5;

    if (pthread_setschedparam(pthread_self(), SCHED_RR, &schedp) != 0) {
      message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    }
    else {
      message(1, "Using POSIX real time scheduling.");
    }
  }
  else {
    message(-1, "No root permissions for real time scheduling.");
  }
#else
  message(-1, "Real time scheduling is not available.");
#endif

#elif defined(LINUX_QNX_SCHEDULING)
  
  if (geteuid() == 0) {
    sched_getparam (0, &schedp);
    schedp.run_q_min = schedp.run_q_max = 2;
    if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
      message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    }
    else {
      message(1, "Using Linux QNX real time scheduling.");
    }
  }
  else {
    message(-1, "No root permissions for real time scheduling.");
  }

#elif defined POSIX_SCHEDULING
  
  if (geteuid() == 0) {
    sched_getparam (0, &schedp);
    schedp.sched_priority = sched_get_priority_max (SCHED_RR) - 5;
    if (sched_setscheduler (0, SCHED_RR, &schedp) != 0) {
      message(-1, "Cannot setup real time scheduling: %s", strerror(errno));
    }
    else {
      message(1, "Using POSIX real time scheduling.");
    }
  }
  else {
    message(-1, "No root permissions for real time scheduling.");
  }
  
#endif

#ifdef HAVE_MLOCKALL
  if (geteuid() == 0) {
    if (mlockall(MCL_CURRENT|MCL_FUTURE) != 0) {
      message(-1, "Cannot lock memory pages: %s", strerror(errno));
    }
    message(3, "Memory locked");
  }
#endif

  switch (writer(cdr, total, header, startLba, remoteMode)) {
  case 1: // error, reader process terminated abnormally
#ifndef USE_POSIX_THREADS
    pid = 0;
#endif
    err = 1;
    break;
  case 2: // error, reader process must be terminated
    err = 1;
    break;
  }

  if (err != 0 && cdr != NULL)
    cdr->abortDao(); // abort writing process

 fail:
#ifdef HAVE_MUNLOCKALL
  munlockall();
#endif

#ifdef USE_POSIX_THREADS
  if (threadStarted) {
    header->terminateReader = 1;

    if (pthread_join(readerThread, NULL) != 0) {
      message(-2, "pthread_join failed: %s", strerror(errno));
      err = 1;
    }

    pthread_attr_destroy(&readerThreadAttr);
  }

#else
  if (pid != 0) {
    if (kill(pid, SIGKILL) == 0) {
      waitForChild(0, &status);
    }
  }
#endif

  releaseSharedMemory(nofShmSegments, shmSegments);

  installSignalHandler(SIGINT, SIG_DFL);
  installSignalHandler(SIGPIPE, SIG_DFL);
  installSignalHandler(SIGALRM, SIG_DFL);
  installSignalHandler(SIGCHLD, SIG_DFL);
  installSignalHandler(SIGQUIT, SIG_DFL);
  installSignalHandler(SIGTERM, SIG_DFL);

  return err;
}


#ifdef USE_POSIX_THREADS
static int getSharedMemory(long nofBuffers,
			   BufferHeader **header, long *nofSegments,
			   ShmSegment **shmSegment)
{
  long b;
  long bufferSize = BUFFER_SIZE * AUDIO_BLOCK_LEN;

  *header = NULL;
  *nofSegments = 0;
  *shmSegment = NULL;

  if (nofBuffers <= 0) {
    return 1;
  }

  *shmSegment = new ShmSegment;
  *nofSegments = 1;

  (*shmSegment)->id = -1;
  (*shmSegment)->buffer = new char[sizeof(BufferHeader) +
				   nofBuffers * sizeof(Buffer) +
				   nofBuffers * bufferSize];

  if ( (*shmSegment)->buffer == NULL) {
    message(-2, "Cannot allocated memory for ring buffer.");
    return 1;
  }


  *header = (BufferHeader*)((*shmSegment)->buffer);
  (*header)->nofBuffers = nofBuffers;
  (*header)->buffers =
	(Buffer*)((*shmSegment)->buffer + sizeof(BufferHeader));

  char *bufferBase = (*shmSegment)->buffer + sizeof(BufferHeader) +
                      nofBuffers * sizeof(Buffer);

  for (b = 0; b < nofBuffers; b++)
    (*header)->buffers[b].buffer = bufferBase + b * bufferSize;

  return 0;
}

static void releaseSharedMemory(long nofSegments, ShmSegment *shmSegment)
{
  if (shmSegment == NULL || nofSegments == 0)
    return;

  if (shmSegment->buffer != NULL) {
    delete[] shmSegment->buffer;
    shmSegment->buffer = NULL;
  }

  delete shmSegment;
}

#else /* USE_POSIX_THREADS */

static int getSharedMemory(long nofBuffers,
			   BufferHeader **header, long *nofSegments,
			   ShmSegment **shmSegments)
{
  long i, b;
  long bufferSize = BUFFER_SIZE * AUDIO_BLOCK_LEN;
  long maxSegmentSize = 0;
  long bcnt = 0;

  *header = NULL;
  *nofSegments = 0;
  *shmSegments = NULL;

  if (nofBuffers <= 0) {
    return 1;
  }

#if defined(linux) && defined(IPC_INFO)
  struct shminfo info;

  if (shmctl(0, IPC_INFO, (struct shmid_ds*)&info) < 0) {
    message(-1, "Cannot get IPC info: %s", strerror(errno));
    maxSegmentSize = 4 * 1024 * 1024;
    message(-1, "Assuming %ld MB shared memory segment size.",
	    maxSegmentSize >> 20);
  }
  else {
    maxSegmentSize = info.shmmax;
  }

#elif defined(__FreeBSD__)
  maxSegmentSize = 4 * 1024 * 1024; // 4 MB
#else
  maxSegmentSize = 1 * 1024 * 1024; // 1 MB
#endif

  message(3, "Shm max segement size: %ld (%ld MB)", maxSegmentSize,
	  maxSegmentSize >> 20);

  if (maxSegmentSize < sizeof(BufferHeader) + nofBuffers * sizeof(Buffer)) {
    message(-2, "Shared memory segment cannot hold a single buffer.");
    return 1;
  }

  maxSegmentSize -= sizeof(BufferHeader) + nofBuffers * sizeof(Buffer);

  long buffersPerSegment = maxSegmentSize / bufferSize;

  if (buffersPerSegment == 0) {
    message(-2, "Shared memory segment cannot hold a single buffer.");
    return 1;
  }

  *nofSegments = nofBuffers / buffersPerSegment;

  if (nofBuffers % buffersPerSegment != 0)
    *nofSegments += 1;

  *shmSegments = new ShmSegment[*nofSegments];

  message(3, "Using %ld shared memory segments.", *nofSegments);

  for (i = 0; i < *nofSegments; i++) {
    (*shmSegments)[i].id = -1;
    (*shmSegments)[i].buffer = NULL;
  }

  long bufCnt = nofBuffers;
  long n;
  long segmentLength;
  char *bufferBase;

  for (i = 0; i < *nofSegments; i++) {
    n = (bufCnt > buffersPerSegment ? buffersPerSegment : bufCnt);

    segmentLength = n * bufferSize;
    if (*header == NULL) {
      // first segment contains the buffer header
      segmentLength += sizeof(BufferHeader) + nofBuffers * sizeof(Buffer);
    }

    (*shmSegments)[i].id = shmget(IPC_PRIVATE, segmentLength, 0600);
    if ((*shmSegments)[i].id < 0) {
      message(-2, "Cannot create shared memory segment: %s",
	      strerror(errno));
      message(-2, "Try to reduce the buffer count (option --buffers).");
      return 1;
    }

    (*shmSegments)[i].buffer = (char *)shmat((*shmSegments)[i].id, 0, 0);
    if (((*shmSegments)[i].buffer) == NULL ||
	((*shmSegments)[i].buffer) == (char *)-1) {
      (*shmSegments)[i].buffer = NULL;
      message(-2, "Cannot get shared memory: %s", strerror(errno));
      message(-2, "Try to reduce the buffer count (option --buffers).");
      return 1;
    }

    
    if (*header == NULL) {
      bufferBase = (*shmSegments)[i].buffer + sizeof(BufferHeader) +
	           nofBuffers * sizeof(Buffer);
      *header = (BufferHeader*)(*shmSegments)[i].buffer;
      (*header)->nofBuffers = nofBuffers;
      (*header)->buffers =
	(Buffer*)((*shmSegments)[i].buffer + sizeof(BufferHeader));
    }
    else {
      bufferBase = (*shmSegments)[i].buffer;
    }

    for (b = 0; b < n; b++)
      (*header)->buffers[bcnt++].buffer = bufferBase + b * bufferSize;

    bufCnt -= n;
  }

  assert(bcnt == nofBuffers);

  return 0;
}

static void releaseSharedMemory(long nofSegments, ShmSegment *shmSegments)
{
  long i;

  if (shmSegments == NULL || nofSegments == 0)
    return;

  for (i = 0; i < nofSegments; i++) {
    if (shmSegments[i].id >= 0) {
      if (shmSegments[i].buffer != NULL) {
	if (shmdt(shmSegments[i].buffer) != 0) {
	  message(-2, "shmdt: %s", strerror(errno));
	}
      }
      if (shmctl(shmSegments[i].id, IPC_RMID, NULL) != 0) {
	message(-2, "Cannot remove shared memory: %s", strerror(errno));
      }
    }
  }

  delete[] shmSegments;
}
#endif /* USE_POSIX_THREADS */

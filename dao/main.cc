/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2002 Andreas Mueller <andreas@daneb.de>
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <stdarg.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>

#include "util.h"
#include "Toc.h"
#include "ScsiIf.h"
#include "CdrDriver.h"
#include "dao.h"
#include "Settings.h"
#include "Cddb.h"


#ifdef UNIXWARE
extern "C" {
  extern int seteuid(uid_t);
  extern int setegid(uid_t);
};
#endif

enum Command { UNKNOWN, SHOW_TOC, SHOW_DATA, READ_TEST, SIMULATE, WRITE,
	       READ_TOC, DISK_INFO, READ_CD, TOC_INFO, TOC_SIZE, BLANK,
	       SCAN_BUS, UNLOCK, COPY_CD, READ_CDDB, MSINFO, DRIVE_INFO,
               DISCID };

static const char *PRGNAME = NULL;
static const char *TOC_FILE = NULL;
static const char *DRIVER_ID = NULL;
static const char *SOURCE_DRIVER_ID = NULL;
static const char *SOURCE_SCSI_DEVICE = NULL;
static const char *DATA_FILENAME = NULL;
static const char *CDDB_SERVER_LIST = "freedb.freedb.org freedb.freedb.org:/~cddb/cddb.cgi uk.freedb.org uk.freedb.org:/~cddb/cddb.cgi cz.freedb.org cz.freedb.org:/~cddb/cddb.cgi";
static const char *CDDB_LOCAL_DB_DIR = NULL;
static int WRITING_SPEED = -1;
static int EJECT = 0;
static int SWAP = 0;
static int MULTI_SESSION = 0;
static Command COMMAND;
static int VERBOSE = 2; // verbose level
static int SESSION = 1; // session for read-toc/read-cd
static int FAST_TOC = 0; // toc reading without sub-channel analysis
static int PAUSE = 1; // pause before writing
static int READ_RAW = 0; // read raw sectors
static int REMOTE_MODE = 0;
static int REMOTE_FD = -1;
static int RELOAD = 0;
static int FORCE = 0;
static int PARANOIA_MODE = 3;
static int ON_THE_FLY = 0;
static int WRITE_SIMULATE = 0;
static int SAVE_SETTINGS = 0;
static int CDDB_TIMEOUT = 60;
static int WITH_CDDB = 0;
static int TAO_SOURCE = 0;
static int TAO_SOURCE_ADJUST = -1;
static int KEEPIMAGE = 0;
static int OVERBURN = 0;
static int BUFFER_UNDER_RUN_PROTECTION = 1;
static int WRITE_SPEED_CONTROL = 1;
static bool PRINT_QUERY = false;

static CdrDriver::BlankingMode BLANKING_MODE = CdrDriver::BLANK_MINIMAL;
static TrackData::SubChannelMode READ_SUBCHAN_MODE = TrackData::SUBCHAN_NONE;

static Settings *SETTINGS = NULL; // settings read from $HOME/.cdrdao

#if defined(__FreeBSD__)

#  ifdef USE_SCGLIB
static const char *SCSI_DEVICE = "0,0,0";
#  else
static const char *SCSI_DEVICE = "cd0";
#  endif
static int FIFO_BUFFERS = 20;

#elif defined(__linux__)

static const char *SCSI_DEVICE = "/dev/cdrecorder";
static int FIFO_BUFFERS = 32;

#else

static const char *SCSI_DEVICE = "0,0,0";
static int FIFO_BUFFERS = 32;

#endif

void message(int level, const char *fmt, ...)
{
  long len = strlen(fmt);
  char last = len > 0 ? fmt[len - 1] : 0;

  va_list args;
  va_start(args, fmt);

  if (level < 0) {
    switch (level) {
    case -1:
      fprintf(stderr, "WARNING: ");
      break;
    case -2:
      fprintf(stderr, "ERROR: ");
      break;
    case -3:
      fprintf(stderr, "INTERNAL ERROR: ");
      break;
    default:
      fprintf(stderr, "FATAL ERROR: ");
      break;
    }
    vfprintf(stderr, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stderr, "\n");
    
    fflush(stderr);
    if (level <= -10)
      exit(1);
  }
  else if (level <= VERBOSE) {
    vfprintf(stderr, fmt, args);
    if (last != ' ' && last != '\r')
      fprintf(stderr, "\n");

    fflush(stderr);
  }

  va_end(args);
}

static void printVersion()
{
  message(1, "Cdrdao version %s - (C) Andreas Mueller <andreas@daneb.de>",
	  VERSION);

#ifdef USE_SCGLIB
  message(2, "  SCSI interface library - (C) Joerg Schilling");
#endif
  message(2, "  Paranoia DAE library - (C) Monty");
  message(2, "");
  message(2, "Check http://cdrdao.sourceforge.net/drives.html#dt for current driver tables.");
  message(1, "");
}

static void printUsage()
{
  switch (COMMAND) {

  case UNKNOWN:
    message(0, "\nUsage: %s <command> [options] [toc-file]", PRGNAME);
    message(0,
"command:\n"
"  show-toc  - prints out toc and exits\n"
"  toc-info  - prints out short toc-file summary\n"
"  toc-size  - prints total number of blocks for toc\n"
"  read-toc  - create toc file from audio CD\n"
"  read-cd   - create toc and rip audio data from CD\n"
"  read-cddb - contact CDDB server and add data as CD-TEXT to toc-file\n"
"  show-data - prints out audio data and exits\n"
"  read-test - reads all audio files and exits\n"
"  disk-info - shows information about inserted medium\n"
"  discid    - prints out CDDB information\n"
"  msinfo    - shows multi session info, output is suited for scripts\n"
"  unlock    - unlock drive after failed writing\n"
"  blank     - blank a CD-RW\n"
"  scanbus   - scan for devices\n"
"  simulate  - shortcut for 'write --simulate'\n"
"  write     - writes CD\n"
"  copy      - copies CD\n");
    
    message (0, "\n Try '%s <command> -h' to get a list of available options\n", PRGNAME);
    break;
    
  case SHOW_TOC:
    message(0, "\nUsage: %s show-toc [-v #] toc-file\n", PRGNAME);
    break;
    
  case SHOW_DATA:
    message(0, "\nUsage: %s show-data [--swap] [-v #] toc-file\n", PRGNAME);
    break;
    
  case READ_TEST:
    message(0, "\nUsage: %s read-test [-v #] toc-file\n", PRGNAME);
    break;
  
  case SIMULATE:
    message(0, "\nUsage: %s simulate [options] toc-file", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --multi                 - session will not be not closed\n"
"  --overburn              - allow to overburn a medium\n"
"  --eject                 - ejects cd after simulation\n"
"  --swap                  - swap byte order of audio files\n"
"  --buffers #             - sets fifo buffer size (min. 10)\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --force                 - force execution of operation\n"
"  -v #                    - sets verbose level\n"
"  -n                      - no pause before writing\n",
	    SCSI_DEVICE);
    break;
    
  case WRITE:
    message(0, "\nUsage: %s write [options] toc-file", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --simulate              - just perform a write simulation\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --multi                 - session will not be not closed\n"
"  --buffer-under-run-protection #\n"
"                          - 0: disable buffer under run protection\n"
"                            1: enable buffer under run protection (default)\n"
"  --write-speed-control # - 0: disable writing speed control by the drive\n"
"                            1: enable writing speed control (default)\n" 
"  --overburn              - allow to overburn a medium\n"
"  --eject                 - ejects cd after writing or simulation\n"
"  --swap                  - swap byte order of audio files\n"
"  --buffers #             - sets fifo buffer size (min. 10)\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --force                 - force execution of operation\n"
"  -v #                    - sets verbose level\n"
"  -n                      - no pause before writing\n",
	    SCSI_DEVICE);
    break;
    
  case READ_TOC:
    message(0, "\nUsage: %s read-toc [options] toc-file", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>    - force usage of specified driver for source device\n"
"  --datafile <filename>   - name of data file placed in toc-file\n"
"  --session #             - select session\n"
"  --fast-toc              - do not extract pre-gaps and index marks\n"
"  --read-raw              - select raw sectors modes for data tracks\n"
"  --read-subchan <mode>   - defines sub-channel reading mode\n"
"                            <mode> = rw | rw_raw\n"
"  --tao-source            - indicate that source CD was written in TAO mode\n"
"  --tao-source-adjust #   - # of link blocks for TAO source CDs (def. 2)\n"
"  --with-cddb             - retrieve CDDB CD-TEXT data while copying\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  --force                 - force execution of operation\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case DISK_INFO:
    message(0, "\nUsage: %s disk-info [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  -v #                    - sets verbose level\n",
	    SCSI_DEVICE);
    break;
    
  case DISCID:
    message(0, "\nUsage: %s discid [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  --query                 - prints out CDDB query only\n"
"  -v #                    - sets verbose level\n",
        SCSI_DEVICE);
    break;
   
  case READ_CD:
    message(0, "\nUsage: %s read-cd [options] toc-file", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>    - force usage of specified driver for source device\n"
"  --datafile <filename>   - name of data file placed in toc-file\n"
"  --session #             - select session\n"
"  --fast-toc              - do not extract pre-gaps and index marks\n"
"  --read-raw              - read raw data sectors (including L-EC data)\n"
"  --read-subchan <mode>   - defines sub-channel reading mode\n"
"                            <mode> = rw | rw_raw\n"
"  --tao-source            - indicate that source CD was written in TAO mode\n"
"  --tao-source-adjust #   - # of link blocks for TAO source CDs (def. 2)\n"
"  --paranoia-mode #       - DAE paranoia mode (0..3)\n"
"  --with-cddb             - retrieve CDDB CD-TEXT data while copying\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  --force                 - force execution of operation\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case TOC_INFO:
    message(0, "\nUsage: %s toc-info [-v #] toc-file\n", PRGNAME);
    break;
    
  case TOC_SIZE:
    message(0, "\nUsage: %s toc-size [-v #] toc-file\n", PRGNAME);

  case BLANK:
    message(0, "\nUsage: %s blank [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --blank-mode <mode>     - blank mode ('full', 'minimal')\n"
"  --eject                 - ejects cd after writing or simulation\n"
"  -v #                    - sets verbose level\n",
	  SCSI_DEVICE);
    break;
    
  case SCAN_BUS:
    message(0, "\nUsage: %s scan-bus [-v #]\n", PRGNAME);
    break;
    
  case UNLOCK:
    message(0, "\nUsage: %s unlock [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --eject                 - ejects cd after unlocking\n"
"  -v #                    - sets verbose level\n",
	    SCSI_DEVICE);
    break;
    
  case COPY_CD:
    message(0, "\nUsage: %s copy [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --source-device {<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>           - force usage of specified driver\n"
"  --source-driver <id>    - force usage of specified driver for source device\n"
"  --simulate              - just perform a copy simulation\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --multi                 - session will not be not closed\n"
"  --buffer-under-run-protection #\n"
"                          - 0: disable buffer under run protection\n"
"                            1: enable buffer under run protection (default)\n"
"  --write-speed-control # - 0: disable writing speed control by the drive\n"
"                            1: enable writing speed control (default)\n" 
"  --overburn              - allow to overburn a medium\n"
"  --eject                 - ejects cd after writing or simulation\n"
"  --swap                  - swap byte order of audio files\n"
"  --on-the-fly            - perform on-the-fly copy, no image file is created\n"
"  --datafile <filename>   - name of temporary data file\n"
"  --buffers #             - sets fifo buffer size (min. 10)\n"
"  --session #             - select session\n"
"  --fast-toc              - do not extract pre-gaps and index marks\n"
"  --read-subchan <mode>   - defines sub-channel reading mode\n"
"                            <mode> = rw | rw_raw\n"
"  --keepimage             - the image will not be deleted after copy\n"
"  --tao-source            - indicate that source CD was written in TAO mode\n"
"  --tao-source-adjust #   - # of link blocks for TAO source CDs (def. 2)\n"
"  --paranoia-mode #       - DAE paranoia mode (0..3)\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --force                 - force execution of operation\n"
"  --with-cddb             - retrieve CDDB CD-TEXT data while copying\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  -v #                    - sets verbose level\n"
"  -n                      - no pause before writing\n",
	  SCSI_DEVICE);
  break;
  
  case READ_CDDB:
    message(0, "\nUsage: %s read-cddb [options] toc-file", PRGNAME);
    message(0,
"options:\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case MSINFO:
    message(0, "\nUsage: %s msinfo [options]", PRGNAME);
    message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"                            (default: %s)\n"
"  --driver <id>           - force usage of specified driver\n"
"  --reload                - reload the disk if necessary for writing\n"
"  -v #                    - sets verbose level\n",
	    SCSI_DEVICE);
    break;
    
  default:
    message (0, "Sorry, no help available for command %d :-(\n", COMMAND);
    break;
  }
  
}

static void importSettings(Command cmd)
{
  const char *sval;
  const int *ival;

  if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
    if ((sval = SETTINGS->getString(SET_WRITE_DRIVER)) != NULL) {
      DRIVER_ID = strdupCC(sval);
    }

    if ((sval = SETTINGS->getString(SET_WRITE_DEVICE)) != NULL) {
      SCSI_DEVICE = strdupCC(sval);
    }
    
    if ((ival = SETTINGS->getInteger(SET_WRITE_SPEED)) != NULL &&
	*ival >= 0) {
      WRITING_SPEED = *ival;
    }

    if ((ival = SETTINGS->getInteger(SET_WRITE_BUFFERS)) != NULL &&
	*ival >= 10) {
      FIFO_BUFFERS = *ival;
    }
  }

  if (cmd == READ_CD || cmd == READ_TOC) {
    if ((sval = SETTINGS->getString(SET_READ_DRIVER)) != NULL) {
      DRIVER_ID = strdupCC(sval);
    }

    if ((sval = SETTINGS->getString(SET_READ_DEVICE)) != NULL) {
      SCSI_DEVICE = strdupCC(sval);
    }

    if ((ival = SETTINGS->getInteger(SET_READ_PARANOIA_MODE)) != NULL &&
	*ival >= 0) {
      PARANOIA_MODE = *ival;
    }
  }

  if (cmd == COPY_CD) {
    if ((sval = SETTINGS->getString(SET_READ_DRIVER)) != NULL) {
      SOURCE_DRIVER_ID = strdupCC(sval);
    }

    if ((sval = SETTINGS->getString(SET_READ_DEVICE)) != NULL) {
      SOURCE_SCSI_DEVICE = strdupCC(sval);
    }
    
    if ((ival = SETTINGS->getInteger(SET_READ_PARANOIA_MODE)) != NULL &&
	*ival >= 0) {
      PARANOIA_MODE = *ival;
    }
  }

  if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK ||
      cmd == DISCID) {
    if ((sval = SETTINGS->getString(SET_WRITE_DRIVER)) != NULL) {
      DRIVER_ID = strdupCC(sval);
    }

    if ((sval = SETTINGS->getString(SET_WRITE_DEVICE)) != NULL) {
      SCSI_DEVICE = strdupCC(sval);
    }
  }

  if (cmd == READ_CDDB || cmd == COPY_CD || cmd == READ_TOC ||
      cmd == READ_CD || cmd == DISCID) {
    if ((sval = SETTINGS->getString(SET_CDDB_SERVER_LIST)) != NULL) {
      CDDB_SERVER_LIST = strdupCC(sval);
    }

    if ((sval = SETTINGS->getString(SET_CDDB_DB_DIR)) != NULL) {
      CDDB_LOCAL_DB_DIR = strdupCC(sval);
    }

    if ((ival = SETTINGS->getInteger(SET_CDDB_TIMEOUT)) != NULL &&
	*ival > 0) {
      CDDB_TIMEOUT = *ival;
    }
  }
}

static void exportSettings(Command cmd)
{
  if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
    if (DRIVER_ID != NULL)
      SETTINGS->set(SET_WRITE_DRIVER, DRIVER_ID);
    
    if (SCSI_DEVICE != NULL)
      SETTINGS->set(SET_WRITE_DEVICE, SCSI_DEVICE);

    if (WRITING_SPEED >= 0) {
      SETTINGS->set(SET_WRITE_SPEED, WRITING_SPEED);
    }

    if (FIFO_BUFFERS > 0) {
      SETTINGS->set(SET_WRITE_BUFFERS, FIFO_BUFFERS);
    }
  }

  if (cmd == READ_CD) {
    if (DRIVER_ID != NULL)
      SETTINGS->set(SET_READ_DRIVER, DRIVER_ID);

    if (SCSI_DEVICE != NULL)
      SETTINGS->set(SET_READ_DEVICE, SCSI_DEVICE);

    SETTINGS->set(SET_READ_PARANOIA_MODE, PARANOIA_MODE);
  }

  if (cmd == COPY_CD) {
    if (SOURCE_DRIVER_ID != NULL)
      SETTINGS->set(SET_READ_DRIVER, SOURCE_DRIVER_ID);

    if (SOURCE_SCSI_DEVICE != NULL)
      SETTINGS->set(SET_READ_DEVICE, SOURCE_SCSI_DEVICE);

    SETTINGS->set(SET_READ_PARANOIA_MODE, PARANOIA_MODE);
  }

  if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK ||
      cmd == DISCID) {
    if (DRIVER_ID != NULL)
      SETTINGS->set(SET_WRITE_DRIVER, DRIVER_ID);
    
    if (SCSI_DEVICE != NULL)
      SETTINGS->set(SET_WRITE_DEVICE, SCSI_DEVICE);
  }

  if (cmd == READ_CDDB ||
      (WITH_CDDB && (cmd == COPY_CD || cmd == READ_TOC || cmd == READ_CD ||
                     cmd == DISCID))) {
    if (CDDB_SERVER_LIST != NULL) {
      SETTINGS->set(SET_CDDB_SERVER_LIST, CDDB_SERVER_LIST);
    }

    if (CDDB_LOCAL_DB_DIR != NULL) {
      SETTINGS->set(SET_CDDB_DB_DIR, CDDB_LOCAL_DB_DIR);
    }

    if (CDDB_TIMEOUT > 0) {
      SETTINGS->set(SET_CDDB_TIMEOUT, CDDB_TIMEOUT);
    }
  }
}

static int parseCmdline(int argc, char **argv)
{
  if (argc < 1) {
    return 1;
  }

  if (strcmp(*argv, "show-toc") == 0) {
    COMMAND = SHOW_TOC;
  }
  else if (strcmp(*argv, "read-toc") == 0) {
    COMMAND = READ_TOC;
  }
  else if (strcmp(*argv, "show-data") == 0) {
    COMMAND = SHOW_DATA;
  }
  else if (strcmp(*argv, "read-test") == 0) {
    COMMAND = READ_TEST;
  }
  else if (strcmp(*argv, "simulate") == 0) {
    COMMAND = SIMULATE;
  }
  else if (strcmp(*argv, "write") == 0) {
    COMMAND = WRITE;
  }
  else if (strcmp(*argv, "disk-info") == 0) {
    COMMAND = DISK_INFO;
  }
  else if (strcmp(*argv, "discid") == 0) {
    COMMAND = DISCID;
  }
  else if (strcmp(*argv, "read-cd") == 0) {
    COMMAND = READ_CD;
  }
  else if (strcmp(*argv, "toc-info") == 0) {
    COMMAND = TOC_INFO;
  }
  else if (strcmp(*argv, "toc-size") == 0) {
    COMMAND = TOC_SIZE;
  }
  else if (strcmp(*argv, "blank") == 0) {
    COMMAND = BLANK;
  }
  else if (strcmp(*argv, "scanbus") == 0) {
    COMMAND = SCAN_BUS;
  }
  else if (strcmp(*argv, "unlock") == 0) {
    COMMAND = UNLOCK;
  }
  else if (strcmp(*argv, "copy") == 0) {
    COMMAND = COPY_CD;
  }
  else if (strcmp(*argv, "read-cddb") == 0) {
    COMMAND = READ_CDDB;
  }
  else if (strcmp(*argv, "msinfo") == 0) {
    COMMAND = MSINFO;
  }
  else if (strcmp(*argv, "drive-info") == 0) {
    COMMAND = DRIVE_INFO;
  }
  else {
    COMMAND=UNKNOWN;
    message(-2, "Illegal command: %s", *argv);
    return 1;
  }

  // retrieve settings from $HOME/.cdrdao for given command
  importSettings(COMMAND);

  argc--, argv++;

  while (argc > 0 && (*argv)[0] == '-') {
    if ((*argv)[1] != '-') {
      switch ((*argv)[1]) {
      case 'h':
	return 1;
	
      case 'v':
	if ((*argv)[2] != 0) {
	  VERBOSE = atoi((*argv) + 2);
	}
	else {
	  if (argc < 2) {
	    message(-2, "Missing argument after: %s", *argv);
	    return 1;
	  }
	  else {
	    VERBOSE = atoi(argv[1]);
	    argc--, argv++;
	  }
	}
	break;

      case 'n':
	PAUSE = 0;
	break;

      default:
	message(-2, "Illegal option: %s", *argv);
	return 1;
	break;
      }
    }
    else {
      if (strcmp((*argv) + 2, "help") == 0) {
	return 1;
      }
      else if (strcmp((*argv) + 2, "device") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  SCSI_DEVICE = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "source-device") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  SOURCE_SCSI_DEVICE = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "speed") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  WRITING_SPEED = atol(argv[1]);
	  if (WRITING_SPEED < 0) {
	    message(-2, "Illegal writing speed: %s", argv[1]);
	    return 1;
	  }
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "blank-mode") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  if (strcmp(argv[1], "full") == 0) {
	    BLANKING_MODE = CdrDriver::BLANK_FULL;
	  }
	  else if (strcmp(argv[1], "minimal") == 0) {
	    BLANKING_MODE = CdrDriver::BLANK_MINIMAL;
	  }
	  else {
	    message(-2, "Illegal blank mode. Valid values: full minimal");
	    return 1;
	  }
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "paranoia-mode") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  PARANOIA_MODE= atol(argv[1]);
	  if (PARANOIA_MODE < 0) {
	    message(-2, "Illegal paranoia mode: %s", argv[1]);
	    return 1;
	  }
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "remote") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  REMOTE_FD = atol(argv[1]);
	  if (REMOTE_FD < 0) {
	    message(-2, "Invalid remote message file descriptor: %s", argv[1]);
	    return 1;
	  }
	  REMOTE_MODE = 1;
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "eject") == 0) {
	EJECT = 1;
      }
      else if (strcmp((*argv) + 2, "swap") == 0) {
	SWAP = 1;
      }
      else if (strcmp((*argv) + 2, "query") == 0) {
        PRINT_QUERY = true;
      }
      else if (strcmp((*argv) + 2, "multi") == 0) {
	MULTI_SESSION = 1;
      }
      else if (strcmp((*argv) + 2, "simulate") == 0) {
        WRITE_SIMULATE = 1;
      }
      else if (strcmp((*argv) + 2, "fast-toc") == 0) {
	FAST_TOC = 1;
      }
      else if (strcmp((*argv) + 2, "read-raw") == 0) {
	READ_RAW = 1;
      }
      else if (strcmp((*argv) + 2, "reload") == 0) {
	RELOAD = 1;
      }
      else if (strcmp((*argv) + 2, "force") == 0) {
	FORCE = 1;
      }
      else if (strcmp((*argv) + 2, "on-the-fly") == 0) {
	ON_THE_FLY = 1;
      }
      else if (strcmp((*argv) + 2, "save") == 0) {
	SAVE_SETTINGS = 1;
      }
      else if (strcmp((*argv) + 2, "tao-source") == 0) {
	TAO_SOURCE = 1;
      }
      else if (strcmp((*argv) + 2, "keepimage") == 0) {
	KEEPIMAGE = 1;
      }
      else if (strcmp((*argv) + 2, "overburn") == 0) {
	OVERBURN = 1;
      }
      else if (strcmp((*argv) + 2, "with-cddb") == 0) {
        WITH_CDDB = 1;
      }
      else if (strcmp((*argv) + 2, "driver") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  DRIVER_ID = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "source-driver") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  SOURCE_DRIVER_ID = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "datafile") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  DATA_FILENAME = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "buffers") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  FIFO_BUFFERS = atoi(argv[1]);
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "session") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  SESSION = atoi(argv[1]);
	  argc--, argv++;
	  if (SESSION < 1) {
	    message(-2, "Illegal session number: %d", SESSION);
	    return 1;
	  }
	}
      }
      else if (strcmp((*argv) + 2, "cddb-servers") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  CDDB_SERVER_LIST = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "cddb-directory") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  CDDB_LOCAL_DB_DIR = argv[1];
	  argc--, argv++;
	}
      }
      else if (strcmp((*argv) + 2, "cddb-timeout") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  CDDB_TIMEOUT = atoi(argv[1]);
	  argc--, argv++;
	  if (CDDB_TIMEOUT < 1) {
	    message(-2, "Illegal CDDB timeout: %d", CDDB_TIMEOUT);
	    return 1;
	  }
	}
      }
      else if (strcmp((*argv) + 2, "tao-source-adjust") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  TAO_SOURCE_ADJUST = atoi(argv[1]);
	  argc--, argv++;
	  if (TAO_SOURCE_ADJUST < 0 || TAO_SOURCE_ADJUST >= 100) {
	    message(-2, "Illegal number of TAO link blocks: %d",
		    TAO_SOURCE_ADJUST);
	    return 1;
	  }
	}
      }
      else if (strcmp((*argv) + 2, "buffer-under-run-protection") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  BUFFER_UNDER_RUN_PROTECTION = atoi(argv[1]);
	  argc--, argv++;
	  if (BUFFER_UNDER_RUN_PROTECTION < 0 ||
	      BUFFER_UNDER_RUN_PROTECTION > 1) {
	    message(-2, "Illegal value for option --buffer-under-run-protection: %d",
		    BUFFER_UNDER_RUN_PROTECTION);
	    return 1;
	  }
	}
      }
      else if (strcmp((*argv) + 2, "write-speed-control") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  WRITE_SPEED_CONTROL = atoi(argv[1]);
	  argc--, argv++;
	  if (WRITE_SPEED_CONTROL < 0 || WRITE_SPEED_CONTROL > 1) {
	    message(-2, "Illegal value for option --write-speed-control: %d",
		    WRITE_SPEED_CONTROL);
	    return 1;
	  }
	}
      }
      else if (strcmp((*argv) + 2, "read-subchan") == 0) {
	if (argc < 2) {
	  message(-2, "Missing argument after: %s", *argv);
	  return 1;
	}
	else {
	  if (strcmp(argv[1], "rw") == 0) {
	    READ_SUBCHAN_MODE = TrackData::SUBCHAN_RW;
	  }
	  else if (strcmp(argv[1], "rw_raw") == 0) {
	    READ_SUBCHAN_MODE = TrackData::SUBCHAN_RW_RAW;
	  }
	  else {
	    message(-2, "Invalid argument after %s: %s", argv[0], argv[1]);
	    return 1;
	  }

	  argc--, argv++;
	}
      }
      else {
	message(-2, "Illegal option: %s", *argv);
	return 1;
      }
    }

    argc--, argv++;
  }

  if (COMMAND != DISK_INFO && COMMAND != BLANK && COMMAND != SCAN_BUS &&
      COMMAND != UNLOCK && COMMAND != COPY_CD && COMMAND != MSINFO &&
      COMMAND != DISCID &&
      COMMAND != DRIVE_INFO) {
    if (argc < 1) {
      message(-2, "Missing toc-file.");
      return 1;
    }
    else if (argc > 1) {
      message(-2, "Expecting only one toc-file.");
      return 1;
    }
    TOC_FILE = *argv;
  }


  return 0;
}

// Selects driver for device of 'scsiIf'.
static CdrDriver *selectDriver(Command cmd, ScsiIf *scsiIf,
			       const char *driverId)
{
  unsigned long options = 0;
  CdrDriver *ret = NULL;

  if (driverId != NULL) {
    char *s = strdupCC(driverId);
    char *p = strchr(s, ':');

    if (p != NULL) {
      *p = 0;
      options = strtoul(p + 1, NULL, 0);
    }

    ret = CdrDriver::createDriver(s, options, scsiIf);

    if (ret == NULL) {
      message(-2, "%s: Illegal driver ID, available drivers:", s);
      CdrDriver::printDriverIds();
    }

    delete[] s;
  }
  else {
    const char *id = NULL;
    
    // for reading commands try to select a special read driver first:
    if (cmd == READ_TOC || cmd == READ_CD)
      id = CdrDriver::selectDriver(0, scsiIf->vendor(), scsiIf->product(),
				   &options);

    // try to select a write driver
    if (id == NULL)
      id = CdrDriver::selectDriver(1, scsiIf->vendor(), scsiIf->product(),
				   &options);
    // if no driver is selected, yet, try to select a read driver for
    // disk-info
    if (id == NULL && (cmd == DISK_INFO || cmd == MSINFO || cmd == DISCID))
      id = CdrDriver::selectDriver(0, scsiIf->vendor(), scsiIf->product(),
				   &options);
    // Still no driver, try to autodetect one
    if (id == NULL)
      id = CdrDriver::detectDriver(scsiIf, &options);
      
    if (id != NULL) {
      ret = CdrDriver::createDriver(id, options, scsiIf);
    }
    else {
      message(0, "");
      message(-2, "No driver found for '%s %s', available drivers:\n",
	      scsiIf->vendor(), scsiIf->product());
      CdrDriver::printDriverIds();

      message(0, "\nFor all recent recorder models either the 'generic-mmc' or");
      message(0, "the 'generic-mmc-raw' driver should work.");
      message(0, "Use option '--driver' to force usage of a driver, e.g.: --driver generic-mmc");
    }
  }

  return ret;
}

#define MAX_RETRY 10
static CdrDriver *setupDevice(Command cmd, const char *scsiDevice,
			      const char *driverId, int initDevice,
			      int checkReady, int checkEmpty, int remote,
			      int reload)
{
  ScsiIf *scsiIf = NULL;
  CdrDriver *cdr = NULL;
  DiskInfo *di = NULL;
  int inquiryFailed = 0;
  int retry = 0;
  int initTries = 2;
  int ret;

  scsiIf = new ScsiIf(scsiDevice);

  switch (scsiIf->init()) {
  case 1:
    message(-2, "Please use option '--device [proto:]bus,id,lun', e.g. "
            "--device 0,6,0 or --device ATAPI:0,0,0");
    delete scsiIf;
    return NULL;
    break;
  case 2:
    inquiryFailed = 1;
    break;
  }
  
  message(2, "%s: %s %s\tRev: %s", scsiDevice, scsiIf->vendor(),
	  scsiIf->product(), scsiIf->revision());


  if (inquiryFailed && driverId == NULL) {
    message(-2, "Inquiry failed and no driver id is specified.");
    message(-2, "Please use option --driver to specify a driver id.");
    delete scsiIf;
    return NULL;
  }

  if ((cdr = selectDriver(cmd, scsiIf, driverId)) == NULL) {
    delete scsiIf;
    return NULL;
  }

  message(2, "Using driver: %s (options 0x%04lx)\n", cdr->driverName(),
	  cdr->options());

  if (!initDevice)
    return cdr;
      
  while (initTries > 0) {
    // clear unit attention
    cdr->rezeroUnit(0);

    if (checkReady) {
      retry = 0;

      while (retry < MAX_RETRY) {
	if (retry++)
	  sleep(3);
	if (!(ret = cdr->testUnitReady(1)))
	  break;
	if (ret == 1) {
	  delete cdr;
	  delete scsiIf;
	  return NULL;
	}
	message(-1, "Unit not ready, still trying...");
      }

      if (ret != 0) {
	message(-2, "Unit not ready, giving up.");
	delete cdr;
	delete scsiIf;
	return NULL;
      }
	
      cdr->rezeroUnit(0);

      if ((di = cdr->diskInfo()) == NULL) {
	message(-2, "Cannot get disk information.");
	delete cdr;
	delete scsiIf;
	return NULL;
      }

      if (checkEmpty && initTries == 2 &&
	  di->valid.empty && !di->empty && 
	  (!di->valid.append || !di->append) &&
	  (!remote || reload)) {
	if (!reload) {
	  message(0, "Disk seems to be written - hit return to reload disk.");
	  fgetc(stdin);
	}

	message(1, "Reloading disk...");

	if (cdr->loadUnload(1) != 0) {
	  delete cdr;
	  delete scsiIf;
	  return NULL;
	}

	sleep(1);
	cdr->rezeroUnit(0); // clear unit attention

	if (cdr->loadUnload(0) != 0) {
	  message(-2, "Cannot load tray.");
	  delete cdr;
	  delete scsiIf;
	  return NULL;
	}

	initTries = 1;
      }
      else {
	initTries = 0;
      }
    }
    else {
      initTries = 0;
    }
  }

  return cdr;
}

static void showDriveInfo(const DriveInfo *i)
{
  if (i == NULL) {
    message(0, "No drive information available.");
    return;
  }

  printf("Maximum reading speed: %d kB/s\n", i->maxReadSpeed);
  printf("Current reading speed: %d kB/s\n", i->currentReadSpeed);
  printf("Maximum writing speed: %d kB/s\n", i->maxWriteSpeed);
  printf("Current writing speed: %d kB/s\n", i->currentWriteSpeed);
  printf("BurnProof supported: %s\n", i->burnProof ? "yes" : "no");
  printf("JustLink supported: %s\n", i->ricohJustLink ? "yes" : "no");
  printf("JustSpeed supported: %s\n", i->ricohJustSpeed ? "yes" : "no");
}

static void showTocInfo(const Toc *toc, const char *tocFile)
{
  long len = toc->length().lba() * AUDIO_BLOCK_LEN;
  len >>= 20;

  printf("%s: %d tracks, length %s, %ld blocks, %ld MB\n", tocFile,
	 toc->nofTracks(), toc->length().str(), toc->length().lba(), len);
}

static void showTocSize(const Toc *toc, const char *tocFile)
{
  printf("%ld\n", toc->length().lba());
}

static void showToc(const Toc *toc)
{
  const Track *t;
  Msf start, end, index;
  int i;
  int n;
  int tcount = 1;
  char buf[14];

  printf("TOC TYPE: %s\n", Toc::tocType2String(toc->tocType()));

  if (toc->catalogValid()) {
    for (i = 0; i < 13; i++) 
      buf[i] = toc->catalog(i) + '0';
    buf[13] = 0;

    printf("CATALOG NUMBER: %s\n", buf);
  }

  TrackIterator itr(toc);

  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    if (tcount > 1)
      printf("\n");

    printf("TRACK %2d  Mode %s", tcount,
	    TrackData::mode2String(t->type()));

    if (t->subChannelType() != TrackData::SUBCHAN_NONE)
      printf(" %s", TrackData::subChannelMode2String(t->subChannelType()));

    printf(":\n");

    if (t->type() == TrackData::AUDIO) {
      if (t->isrcValid()) {
	printf("          ISRC %c%c %c%c%c %c%c %c%c%c%c%c\n",
		t->isrcCountry(0), t->isrcCountry(1),
		t->isrcOwner(0), t->isrcOwner(1), t->isrcOwner(2),
		t->isrcYear(0) + '0', t->isrcYear(1) + '0',
		t->isrcSerial(0) + '0', t->isrcSerial(1) + '0',
		t->isrcSerial(2) + '0', t->isrcSerial(3) + '0',
		t->isrcSerial(4) + '0');
      }
    }
    printf("          COPY%sPERMITTED\n",
	    t->copyPermitted() ? " " : " NOT ");

    if (t->type() == TrackData::AUDIO) {
      printf("          %sPRE-EMPHASIS\n",
	      t->preEmphasis() ? "" : "NO ");
      printf("          %s CHANNEL AUDIO\n",
	      t->audioType() == 0 ? "TWO" : "FOUR");
    }

    if (t->start().lba() != 0) {
      printf("          PREGAP %s(%6ld)\n", 
	      t->start().str(), t->start().lba());
    }
    printf("          START  %s(%6ld)\n",
	    start.str(), start.lba());
    n = t->nofIndices();
    for (i = 0; i < n; i++) {
      index = start + t->getIndex(i);
      printf("          INDEX %2d %s(%6ld)\n",
	      i + 2, index.str(), index.lba());
    }

    printf("          END%c   %s(%6ld)\n",
	    t->isPadded() ? '*' : ' ', end.str(), end.lba());

    tcount++;
  }
} 

void showData(const Toc *toc, int swap)
{
  long length = toc->length().lba();
  Sample buf[SAMPLES_PER_BLOCK];
  int i;
  unsigned long sampleNr = 0;
  long lba = 150;

  TocReader reader(toc);

  if (reader.openData() != 0) {
    message(-2, "Cannot open audio data.");
    return;
  }

  while (length > 0) {
    if (reader.readSamples(buf, SAMPLES_PER_BLOCK) != SAMPLES_PER_BLOCK) {
      message(-2, "Read of audio data failed.");
      return;
    }
    lba++;

    if (swap) {
      swapSamples(buf, SAMPLES_PER_BLOCK);
    }

    for (i = 0; i < SAMPLES_PER_BLOCK; i++) {
      printf("%7lu:%6d %6d\n", sampleNr, buf[i].left(), buf[i].right());
      sampleNr++;
    }
    length -= 1;
  }
}

void showDiskInfo(DiskInfo *di)
{
  const char *s1, *s2;

  message(2, "That data below may not reflect the real status of the inserted medium");
  message(2, "if a simulation run was performed before. Reload the medium in this case.");
  message(2, "");

  printf("CD-RW                : ");
  if (di->valid.cdrw)
    printf(di->cdrw ? "yes" : "no");
  else 
    printf("n/a");

  printf("\n");

  printf("Total Capacity       : ");
  if (di->valid.capacity)
    printf("%s (%ld blocks, %ld/%ld MB)", Msf(di->capacity).str(),
	   di->capacity,
	   (di->capacity * 2) >> 10,
	   (di->capacity * AUDIO_BLOCK_LEN) >> 20);
  else
    printf("n/a");

  printf("\n");

  printf("CD-R medium          : ");
  if (di->valid.manufacturerId) {
    if (CdrDriver::cdrVendor(di->manufacturerId, &s1, &s2)) {
      printf("%s\n", s1);
      printf("                       %s\n", s2);
    }
    else {
      printf("%s: unknown vendor ID\n", di->manufacturerId.str());
    }
  }
  else {
    printf("n/a\n");
  }

  printf("Recording Speed      : ");
  if (di->valid.recSpeed)
    printf("%dX - %dX", di->recSpeedLow, di->recSpeedHigh);
  else
    printf("n/a");

  printf("\n");

  printf("CD-R empty           : ");
  if (di->valid.empty)
    printf(di->empty ? "yes" : "no");
  else 
    printf("n/a");

  printf("\n");

  if (di->valid.empty && !di->empty && di->valid.append) {
    printf("Toc Type             : ");
    switch (di->diskTocType) {
    case 0x00:
      printf("CD-DA or CD-ROM");
      break;
    case 0x10:
      printf("CD-I");
      break;
    case 0x20:
      printf("CD-ROM XA");
      break;
    case 0xff:
      printf("Undefined");
      break;
    default:
      printf("invalid: %d", di->diskTocType);
      break;
    }

    printf("\n");

    printf("Sessions             : %d\n", di->sessionCnt);
    printf("Last Track           : %d\n", di->lastTrackNr);
    printf("Appendable           : %s\n", di->append ? "yes" : "no");

    if (di->append) {
      printf("Start of last session: %ld (%s)\n", di->lastSessionLba,
	      Msf(di->lastSessionLba + 150).str());
      printf("Start of new session : %ld (%s)\n", di->thisSessionLba,
	      Msf(di->thisSessionLba + 150).str());

      if (di->valid.capacity && di->capacity > di->thisSessionLba) {
	long r = di->capacity - di->thisSessionLba;

	printf("Remaining Capacity   : %s (%ld blocks, %ld/%ld MB)\n",
		Msf(r).str(), r, (r * 2) >> 10, (r * AUDIO_BLOCK_LEN) >> 20);
      }
    }
  }
}

/*
 * Show multi session info in a format that is easy to parse with scritps.
 * Return: 0: OK
 *         1: disk is not empty and not appendable
 *         2: could not determine the requested information
 */
static int showMultiSessionInfo(DiskInfo *di)
{
  
  if (di->valid.empty) {
    if (di->empty) {
      // print nothing  to indicate empty disk
      return 0;
    }
    else if (di->valid.append) {
      if (di->append) {
	printf("%ld,%ld\n", di->lastSessionLba, di->thisSessionLba);
	return 0;
      }
      else {
	return 1;
      }
    }
  }

  return 2;
}

static void printCddbQuery(Toc *toc)
{
  Cddb cddb(toc);

  cddb.printDbQuery();
}

static int readCddb(Toc *toc, bool showEntry = false)
{
  int err = 0;
  char *servers = strdupCC(CDDB_SERVER_LIST);
  char *p;
  char *sep = " ,";
  char *user = NULL;
  char *host = NULL;
  struct passwd *pwent;
  struct utsname sinfo;
  Cddb::QueryResults *qres, *qrun, *qsel;
  Cddb::CddbEntry *dbEntry;

  Cddb cddb(toc);

  cddb.timeout(CDDB_TIMEOUT);

  if (CDDB_LOCAL_DB_DIR != NULL)
    cddb.localCddbDirectory(CDDB_LOCAL_DB_DIR);


  for (p = strtok(servers, sep); p != NULL; p = strtok(NULL, sep))
    cddb.appendServer(p);

  delete[] servers;
  servers = NULL;

  if ((pwent = getpwuid(getuid())) != NULL &&
      pwent->pw_name != NULL) {
    user = strdupCC(pwent->pw_name);
  }
  else {
    user = strdupCC("unknown");
  }

  if (uname(&sinfo) == 0) {
    host = strdupCC(sinfo.nodename);
  }
  else {
    host = strdupCC("unknown");
  }
  

  if (cddb.connectDb(user, host, "cdrdao", VERSION) != 0) {
    message(-2, "Cannot connect to any CDDB server.");
    err = 2; goto fail;
  }

	
  if (cddb.queryDb(&qres) != 0) {
    message(-2, "Querying of CDDB server failed.");
    err = 2; goto fail;
  }
  
  if (qres == NULL) {
    message(1, "No CDDB record found for this toc-file.");
    err = 1; goto fail;
  }

  if (qres->next != NULL || !(qres->exactMatch)) {
    int qcount;

    if (qres->next == NULL)
      message(0, "Found following inexact match:");
    else
      message(0, "Found following inexact matches:");
    
    message(0, "\n    DISKID   CATEGORY     TITLE\n");
    
    for (qrun = qres, qcount = 0; qrun != NULL; qrun = qrun->next, qcount++) {
      message(0, "%2d. %-8s %-12s %s", qcount + 1, qrun->diskId,
	      qrun->category,  qrun->title);
    }

    message(0, "\n");

    qsel = NULL;

    while (1) {
      char buf[20];
      int sel;

      message(0, "Select match, 0 for none [0-%d]?", qcount);

      if (fgets(buf, 20, stdin) == NULL)
	break;

      for (p = buf; *p != 0 && isspace(*p); p++) ;

      if (*p != 0 && isdigit(*p)) {
	sel = atoi(p);

	if (sel == 0) {
	  break;
	}
	else if (sel > 0 && sel <= qcount) {
	  sel--;
	  for (qsel = qres, qcount = 0; qsel != NULL && qcount != sel;
	       qsel = qsel->next, qcount++) ;

	  break;
	}
      }
    }

    if (qsel == NULL) {
      message(0, "No match selected.");
      err = 1; goto fail;
    }
  }
  else {
    qsel = qres;
  }


  message(1, "Reading CDDB record for: %s-%s-%s", qsel->diskId, qsel->category,
	  qsel->title);

  if (cddb.readDb(qsel->category, qsel->diskId, &dbEntry) != 0) {
    message(-2, "Reading of CDDB record failed.");
    err = 2; goto fail;
  }

  if (showEntry)
      cddb.printDbEntry();

  if (!cddb.addAsCdText(toc))
    err = 1;

 fail:
  delete[] user;
  delete[] host;

  return err;
}

static void scanBus()
{
  int i, len;
  ScsiIf::ScanData *sdata = ScsiIf::scan(&len);

  if (sdata) {
    for (i = 0; i < len; i++) {
      message(0, "%s : %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
              sdata[i].product, sdata[i].revision);
    }
    delete[] sdata;
  }

#ifdef SCSI_ATAPI
  sdata = ScsiIf::scan(&len, "ATAPI");
  if (sdata) {
    for (i = 0; i < len; i++) {
      message(0, "%-20s %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
              sdata[i].product, sdata[i].revision);
    }
    delete[] sdata;
  }

  sdata = ScsiIf::scan(&len, "ATA");
  if (sdata) {
    for (i = 0; i < len; i++) {
      message(0, "%-20s %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
              sdata[i].product, sdata[i].revision);
    }
    delete[] sdata;
  }
#endif
}

static int checkToc(const Toc *toc)
{
  int ret = toc->check();

  if (ret != 0) {
    if (ret == 1) { // only a warning message occured
      if (FORCE) {
	ret = 0;
      }
      else {
	message(-2, "The toc check function detected at least one warning.");
	message(-2, "If you record this toc the resulting CD might be unusable");
	message(-2, "or the recording process might abort with error.");
	message(-2, "Use option --force to ignore the warnings.");
      }
    }
  }

  return ret;
}

static int copyCd(CdrDriver *src, CdrDriver *dst, int session,
		  const char *dataFilename, int fifoBuffers, int swap,
		  int eject, int force, int keepimage)
{
  char dataFilenameBuf[50];
  long pid = getpid();
  Toc *toc;
  int ret = 0;
  DiskInfo *di = NULL;
  int isAppendable = 0;

  if (dataFilename == NULL) {
    // create a unique temporary data file name in current directory
    sprintf(dataFilenameBuf, "cddata%ld.bin", pid);
    dataFilename = dataFilenameBuf;
  }

  src->rawDataReading(1);
  src->taoSource(TAO_SOURCE);
  if (TAO_SOURCE_ADJUST >= 0)
    src->taoSourceAdjust(TAO_SOURCE_ADJUST);

  if ((toc = src->readDisk(session, dataFilename)) == NULL) {
    unlink(dataFilename);
    message(-2, "Creation of source CD image failed.");
    return 1;
  }

  if (WITH_CDDB) {
    if (readCddb(toc) == 0)
      message(2, "CD-TEXT data was added to toc-file.");
    else
      message(2, "Reading of CD-TEXT data failed - continuing without CD-TEXT data.");
  }

  if (keepimage) {
    char tocFilename[50];

    sprintf(tocFilename, "cd%ld.toc", pid);
    
    message(2, "Keeping created image file \"%s\".", dataFilename);
    message(2, "Corresponding toc-file is written to \"%s\".", tocFilename);

    toc->write(tocFilename);
  }

  if (checkToc(toc)) {
    message(-3, "Toc created from source CD image is inconsistent.");
    toc->print(std::cout);
    delete toc;
    return 1;
  }

  switch (dst->checkToc(toc)) {
  case 0: // OK
    break;
  case 1: // warning
    if (!force) {
      message(-2, "The toc extracted from the source CD is not suitable for");
      message(-2, "the recorder device and may produce undefined results.");
      message(-2, "Use option --force to use it anyway.");
      delete toc;
      return 1;
    }
    break;
  default: // error
    message(-2, "The toc extracted from the source CD is not suitable for this drive.");
    delete toc;
    return 1;
    break;
  }

  if (src == dst) {
    message(0, "Please insert a recordable medium and hit enter.");
    getc(stdin);
  }

  do {
    if ((di = dst->diskInfo()) == NULL) {
      message(-2, "Cannot get disk information from recorder device.");
      delete toc;
      return 1;
    }

    if (!di->valid.empty) {
      message(-2, "Cannot determine disk status - hit enter to try again.");
      getc(stdin);
      isAppendable = 0;
    }
    else if (!di->empty && (!di->valid.append || !di->append)) {
      message(0, "Medium in recorder device is not empty and not appendable.");
      message(0, "Please insert a recordable medium and hit enter.");
      getc(stdin);
      isAppendable = 0;
    }
    else {
      isAppendable = 1;
    }
  } while (!isAppendable);


  if (dst->preventMediumRemoval(1) != 0) {
    if (!keepimage) {
      if (unlink(dataFilename) != 0)
	message(-2, "Cannot remove CD image file \"%s\": %s", dataFilename,
		strerror(errno));
    }

    delete toc;
    return 1;
  }

  if (writeDiskAtOnce(toc, dst, fifoBuffers, swap, 0, 0) != 0) {
    if (dst->simulate())
      message(-2, "Simulation failed.");
    else
      message(-2, "Writing failed.");

    ret = 1;
  }
  else {
    if (dst->simulate())
      message(1, "Simulation finished successfully.");
    else
      message(1, "Writing finished successfully.");
  }

  if (dst->preventMediumRemoval(0) != 0)
    ret = 1;

  dst->rezeroUnit(0);

  if (ret == 0 && eject) {
    dst->loadUnload(1);
  }

  if (!keepimage) {
    if (unlink(dataFilename) != 0)
      message(-2, "Cannot remove CD image file \"%s\": %s", dataFilename,
	      strerror(errno));
  }

  delete toc;

  return ret;
}

static int copyCdOnTheFly(CdrDriver *src, CdrDriver *dst, int session,
			  int fifoBuffers, int swap,
			  int eject, int force)
{
  Toc *toc = NULL;
  int pipeFds[2];
  pid_t pid = -1;
  int ret = 0;
  int oldStdin = -1;

  if (src == dst)
    return 1;

  if (pipe(pipeFds) != 0) {
    message(-2, "Cannot create pipe: %s", strerror(errno));
    return 1;
  }
  
  src->rawDataReading(1);
  src->taoSource(TAO_SOURCE);
  if (TAO_SOURCE_ADJUST >= 0)
    src->taoSourceAdjust(TAO_SOURCE_ADJUST);

  src->onTheFly(1); // the fd is not used by 'readDiskToc', just need to
                    // indicate that on-the-fly copying is active for
                    // automatical selection if the first track's pre-gap
                    // is padded with zeros in the created Toc.

  if ((toc = src->readDiskToc(session, "-")) == NULL) {
    message(-2, "Creation of source CD toc failed.");
    ret = 1;
    goto fail;
  }

  if (WITH_CDDB) {
    if (readCddb(toc) != 0) {
      ret = 1;
      goto fail;
    }
    else {
      message(2, "CD-TEXT data was added to toc.");
    }
  }
  
  if (checkToc(toc) != 0) {
    message(-3, "Toc created from source CD image is inconsistent - please report.");
    toc->print(std::cout);
    ret = 1;
    goto fail;
  }

  switch (dst->checkToc(toc)) {
  case 0: // OK
    break;
  case 1: // warning
    if (!force) {
      message(-2, "The toc extracted from the source CD is not suitable for");
      message(-2, "the recorder device and may produce undefined results.");
      message(-2, "Use option --force to use it anyway.");
      ret = 1;
      goto fail;
    }
    break;
  default: // error
    message(-2, "The toc extracted from the source CD is not suitable for this drive.");
    ret = 1;
    goto fail;
    break;
  }

  if ((pid = fork()) < 0) {
    message(-2, "Cannot fork reader process: %s", strerror(errno));
    ret = 1;
    goto fail;
  }

  if (pid == 0) {
    // we are the reader process
    setsid();
    close(pipeFds[0]);

    src->onTheFly(pipeFds[1]);

    VERBOSE = 0;

#ifdef __CYGWIN__
    /* Close the SCSI interface and open it again. This will re-init the
     * ASPI layer which is required for the child process
     */

    delete src->scsiIf();

    src->scsiIf(new ScsiIf(SOURCE_SCSI_DEVICE));
    
    if (src->scsiIf()->init() != 0) {
      message(-2, "Re-init of SCSI interace failed.");

      // indicate end of data
      close(pipeFds[1]);

      while (1)
	sleep(10);
    }    
#endif

    if (src->readDisk(session, "-") != NULL)
      message(1, "CD image reading finished successfully.");
    else
      message(-2, "CD image reading failed.");

    // indicate end of data
    close(pipeFds[1]);
    while (1)
      sleep(10);
  }

  close(pipeFds[1]);
  pipeFds[1] = -1;

  if ((oldStdin = dup(fileno(stdin))) < 0) {
    message(-2, "Cannot duplicate stdin: %s", strerror(errno));
    ret = 1;
    goto fail;
  }

  if (dup2(pipeFds[0], fileno(stdin)) != 0) {
    message(-2, "Cannot connect pipe to stdin: %s", strerror(errno));
    close(oldStdin);
    oldStdin = -1;
    ret = 1;
    goto fail;
  }

  dst->onTheFly(fileno(stdin));

  if (dst->preventMediumRemoval(1) != 0) {
    ret = 1;
    goto fail;
  }

  if (writeDiskAtOnce(toc, dst, fifoBuffers, swap, 0, 0) != 0) {
    if (dst->simulate())
      message(-2, "Simulation failed.");
    else
      message(-2, "Writing failed.");

    ret = 1;
  }
  else {
    if (dst->simulate())
      message(1, "Simulation finished successfully.");
    else
      message(1, "Writing finished successfully.");
  }

  dst->rezeroUnit(0);

  if (dst->preventMediumRemoval(0) != 0)
    ret = 1;

  if (ret == 0 && eject) {
    dst->loadUnload(1);
  }


fail:
  if (pid > 0) {
    int status;
    kill(pid, SIGKILL);
    wait(&status);
  }

  if (oldStdin >= 0) {
    dup2(oldStdin, fileno(stdin));
    close(oldStdin);
  }

  delete toc;

  close(pipeFds[0]);

  if (pipeFds[1] >= 0)
    close(pipeFds[1]);

  return ret;
}

int main(int argc, char **argv)
{
  int exitCode = 0;
  Toc *toc = NULL;
  ScsiIf *cdrScsi = NULL;
  ScsiIf *srcCdrScsi = NULL;
  CdrDriver *cdr = NULL;
  CdrDriver *srcCdr = NULL;
  int delSrcDevice = 0;
  DiskInfo *di = NULL;
  DiskInfo *srcDi = NULL;
  const char *homeDir;
  char *settingsPath = NULL;

  PRGNAME = *argv;

  SETTINGS = new Settings;

  settingsPath = "/etc/cdrdao.conf";
  if (SETTINGS->read(settingsPath) == 0)
    message(3, "Read settings from \"%s\".", settingsPath);

  settingsPath = "/etc/defaults/cdrdao";
  if (SETTINGS->read(settingsPath) == 0)
    message(3, "Read settings from \"%s\".", settingsPath);

  settingsPath = NULL;

  if ((homeDir = getenv("HOME")) != NULL) {
    settingsPath = strdup3CC(homeDir, "/.cdrdao", NULL);

    if (SETTINGS->read(settingsPath) == 0)
      message(3, "Read settings from \"%s\".", settingsPath);
  }
  else {
    message(-1,
	    "Environment variable 'HOME' not defined - cannot read .cdrdao.");
  }

#ifdef UNIXWARE
  if (getuid() != 0) {
    message(-2, "You must be root to use cdrdao.");
    exit(1);
  }
#endif

  if (parseCmdline(argc - 1, argv + 1) != 0) {
    VERBOSE = 2;
    message(0, "");
    printVersion();
    printUsage();
    exit(1);
  }

  printVersion();

  if (SAVE_SETTINGS && settingsPath != NULL) {
    exportSettings(COMMAND);
    SETTINGS->write(settingsPath);
  }


  if (COMMAND != READ_TOC && COMMAND != DISK_INFO && COMMAND != READ_CD &&
      COMMAND != BLANK && COMMAND != SCAN_BUS && COMMAND != UNLOCK &&
      COMMAND != DISCID &&
      COMMAND != COPY_CD && COMMAND != MSINFO && COMMAND != DRIVE_INFO) {
    toc = Toc::read(TOC_FILE);

    if (REMOTE_MODE) {
      unlink(TOC_FILE);
    }

    if (toc == NULL) {
      exitCode = 1; goto fail;
    }

    if (COMMAND != SHOW_TOC && COMMAND != READ_CDDB) {
      if (checkToc(toc) != 0) {
	message(-2, "Toc file \"%s\" is inconsistent.", TOC_FILE);
	exitCode = 1; goto fail;
      }
    }
  }


  if (COMMAND == SIMULATE || COMMAND == WRITE || COMMAND == READ_TOC ||
      COMMAND == DISK_INFO || COMMAND == READ_CD || COMMAND == BLANK ||
      COMMAND == UNLOCK || COMMAND == COPY_CD || COMMAND == MSINFO ||
      COMMAND == DISCID ||
      COMMAND == DRIVE_INFO) {
    cdr = setupDevice(COMMAND, SCSI_DEVICE, DRIVER_ID, 
		      /* init device? */
		      (COMMAND == UNLOCK) ? 0 : 1,
		      /* check for ready status? */
              (COMMAND == BLANK || COMMAND == DRIVE_INFO ||
               COMMAND == DISCID) ? 0 : 1,
		      /* reset status of medium if not empty? */
		      (COMMAND == SIMULATE || COMMAND == WRITE) ? 1 : 0,
		      REMOTE_MODE, RELOAD);

    if (cdr == NULL) {
      message(-2, "Cannot setup device %s.", SCSI_DEVICE);
      exitCode = 1; goto fail;
    }

    cdrScsi = cdr->scsiIf();

    if ((di = cdr->diskInfo()) == NULL) {
      message(-2, "Cannot get disk information.");
      exitCode = 1; goto fail;
    }
  }

  if (COMMAND == COPY_CD) {
    if (SOURCE_SCSI_DEVICE != NULL) {
      delSrcDevice = 1;
      srcCdr = setupDevice(READ_CD, SOURCE_SCSI_DEVICE, SOURCE_DRIVER_ID,
			 1, 1, 0, 0, 0);

      if (srcCdr == NULL) {
	message(-2, "Cannot setup source device %s.", SOURCE_SCSI_DEVICE);
	exitCode = 1; goto fail;
      }

      srcCdrScsi = srcCdr->scsiIf();

      if ((srcDi = srcCdr->diskInfo()) == NULL) {
	message(-2, "Cannot get disk information from source device.");
	exitCode = 1; goto fail;
      }
    }
    else {
      srcCdr = cdr;
      srcDi = di;
    }
  }

  if (REMOTE_MODE)
    PAUSE = 0;

  switch (COMMAND) {
  case READ_CDDB:
    if ((exitCode = readCddb(toc)) == 0) {
      message(1, "Writing CD-TEXT populated toc-file \"%s\".", TOC_FILE);
      if (toc->write(TOC_FILE) != 0)
	exitCode = 2;
    }
    break;

  case SCAN_BUS:
    scanBus();
    break;

  case DRIVE_INFO:
    showDriveInfo(cdr->driveInfo(1));
    break;

  case SHOW_TOC:
    showToc(toc);
    if (toc->check() > 1) {
      message(-2, "Toc file \"%s\" is inconsistent.", TOC_FILE);
    }
    break;

  case TOC_INFO:
    showTocInfo(toc, TOC_FILE);
    if (toc->check() > 1) {
      message(-2, "Toc file \"%s\" is inconsistent.", TOC_FILE);
    }
    break;

  case TOC_SIZE:
    showTocSize(toc, TOC_FILE);
    if (toc->check() > 1) {
      message(-2, "Toc file \"%s\" is inconsistent.", TOC_FILE);
    }
    break;

  case SHOW_DATA:
    showData(toc, SWAP);
    break;

  case READ_TEST:
    message(1, "Starting read test...");
    message(2, "Process can be aborted with QUIT signal (usually CTRL-\\).");
    if (writeDiskAtOnce(toc, NULL, FIFO_BUFFERS, SWAP, 1, WRITING_SPEED) != 0) {
      message(-2, "Read test failed.");
      exitCode = 1; goto fail;
    }
    break;

  case DISK_INFO:
    showDiskInfo(di);
    break;

  case DISCID:
    if (di->valid.empty && di->empty) {
      message(-2, "Inserted disk is empty.");
      exitCode = 1; goto fail;
    }
    cdr->subChanReadMode(READ_SUBCHAN_MODE);
    cdr->rawDataReading(READ_RAW);
    cdr->fastTocReading(1);
    cdr->taoSource(TAO_SOURCE);
    if (TAO_SOURCE_ADJUST >= 0)
      cdr->taoSourceAdjust(TAO_SOURCE_ADJUST);

    cdr->force(FORCE);

    if ((toc = cdr->readDiskToc(SESSION,
                                (DATA_FILENAME == NULL) ?
                                "data.wav" : DATA_FILENAME)) == NULL) {
      cdr->rezeroUnit(0);
      exitCode = 1; goto fail;
    }
    else {
      cdr->rezeroUnit(0);

      if (PRINT_QUERY)
          printCddbQuery(toc);
      else
          readCddb(toc, true);
    }
    break;
   
  case MSINFO:
    switch (showMultiSessionInfo(di)) {
    case 0:
      exitCode = 0;
      break;
      
    case 1: // CD-R is not empty and not appendable
      exitCode = 2;
      break;
      
    case 2: // cannot determine state
      exitCode = 3;
      break;

    default: // everthing else is an error
      exitCode = 1;
      break;
    }
    break;

  case READ_TOC:
    if (di->valid.empty && di->empty) {
      message(-2, "Inserted disk is empty.");
      exitCode = 1; goto fail;
    }
    message(1, "Reading toc data...");

    if (access(TOC_FILE, R_OK) == 0) {
      message(-2, "File \"%s\" exists, will not overwrite.", TOC_FILE);
      exitCode = 1; goto fail;
    }

    cdr->subChanReadMode(READ_SUBCHAN_MODE);
    cdr->rawDataReading(READ_RAW);
    cdr->fastTocReading(FAST_TOC);
    cdr->taoSource(TAO_SOURCE);
    if (TAO_SOURCE_ADJUST >= 0)
      cdr->taoSourceAdjust(TAO_SOURCE_ADJUST);

    cdr->force(FORCE);

    if ((toc = cdr->readDiskToc(SESSION,
				(DATA_FILENAME == NULL) ?
				"data.wav" : DATA_FILENAME)) == NULL) {
      cdr->rezeroUnit(0);
      exitCode = 1; goto fail;
    }
    else {
      cdr->rezeroUnit(0);

#if defined(HAVE_SETEUID) && defined(HAVE_SETEGID)
      seteuid(getuid());
      setegid(getgid());
#endif

      if (WITH_CDDB) {
	if (readCddb(toc) == 0) {
	  message(2, "CD-TEXT data was added to toc-file.");
	}
      }

      std::ofstream out(TOC_FILE);
      if (!out) {
	message(-2, "Cannot open \"%s\" for writing: %s", TOC_FILE,
		strerror(errno));
	exitCode = 1; goto fail;
      }
      toc->print(out);

      message(1, "Reading of toc data finished successfully.");
    }
    break;
    
  case READ_CD:
    if (di->valid.empty && di->empty) {
      message(-2, "Inserted disk is empty.");
      exitCode = 1; goto fail;
    }
    message(1, "Reading toc and track data...");

    if (access(TOC_FILE, R_OK) == 0) {
      message(-2, "File \"%s\" exists, will not overwrite.", TOC_FILE);
      exitCode = 1; goto fail;
    }

    cdr->subChanReadMode(READ_SUBCHAN_MODE);
    cdr->rawDataReading(READ_RAW);
    cdr->taoSource(TAO_SOURCE);
    if (TAO_SOURCE_ADJUST >= 0)
      cdr->taoSourceAdjust(TAO_SOURCE_ADJUST);

    cdr->paranoiaMode(PARANOIA_MODE);
    cdr->fastTocReading(FAST_TOC);
    cdr->remote(REMOTE_MODE, REMOTE_FD);
    cdr->force(FORCE);

    toc = cdr->readDisk(SESSION,
			(DATA_FILENAME == NULL) ? "data.bin" : DATA_FILENAME);
      
    if (toc == NULL) {
      cdr->rezeroUnit(0);
      exitCode = 1; goto fail;
    }
    else {
      cdr->rezeroUnit(0);

#if defined(HAVE_SETEUID) && defined(HAVE_SETEGID)
      seteuid(getuid());
      setegid(getgid());
#endif

      if (WITH_CDDB) {
	if (readCddb(toc) == 0) {
	  message(2, "CD-TEXT data was added to toc-file.");
	}
      }

      std::ofstream out(TOC_FILE);
      if (!out) {
	message(-2, "Cannot open \"%s\" for writing: %s",
		TOC_FILE, strerror(errno));
	exitCode = 1; goto fail;
      }
      toc->print(out);

      message(1, "Reading of toc and track data finished successfully.");
    }
    break;

  case WRITE:
    if (WRITE_SIMULATE == 0)
      cdr->simulate(0);
    // fall through
    
  case SIMULATE:
    if (di->valid.empty && !di->empty && 
	(!di->valid.append || !di->append)) {
      message(-2, "Inserted disk is not empty and not appendable.");
      exitCode = 1; goto fail;
    }

    if (toc->length().lba() > di->capacity) {
      message((OVERBURN ? -1 : -2),
	      "Length of toc (%s, %ld blocks) exceeds capacity ",
	      toc->length().str(), toc->length().lba());
      message(0, "of CD-R (%s, %ld blocks).", Msf(di->capacity).str(),
	      di->capacity);

      if (OVERBURN) {
	message(-1, "Ignored because of option '--overburn'.");
	message(-1, "Some drives may fail to record this toc.");
      }
      else {
	message(-2, "Please use option '--overburn' to start recording anyway.");
	exitCode = 1; goto fail;
      }
    }

    if (MULTI_SESSION != 0) {
      if (cdr->multiSession(1) != 0) {
	message(-2, "This driver does not support multi session discs.");
	exitCode = 1; goto fail;
      }
    }

    if (WRITING_SPEED >= 0) {
      if (cdr->speed(WRITING_SPEED) != 0) {
	message(-2, "Writing speed %d not supported by device.",
		WRITING_SPEED);
	exitCode = 1; goto fail;
      }
    }

    cdr->bufferUnderRunProtection(BUFFER_UNDER_RUN_PROTECTION);
    cdr->writeSpeedControl(WRITE_SPEED_CONTROL);

    cdr->force(FORCE);
    cdr->remote(REMOTE_MODE, REMOTE_FD);

    switch (cdr->checkToc(toc)) {
    case 0: // OK
      break;
    case 1: // warning
      if (FORCE == 0 && REMOTE_MODE == 0) {
	message(-2, "Toc-file \"%s\" may create undefined results.", TOC_FILE);
	message(-2, "Use option --force to use it anyway.");
	exitCode = 1; goto fail;
      }
      break;
    default: // error
      message(-2, "Toc-file \"%s\" is not suitable for this drive.",
	      TOC_FILE);
      exitCode = 1; goto fail;
      break;
    }

    message(1, "Starting write ");
    if (cdr->simulate()) {
      message(1, "simulation ");
    }
    message(1, "at speed %d...", cdr->speed());
    if (cdr->multiSession() != 0) {
      message(1, "Using multi session mode.");
    }

    if (PAUSE) {
      message(1, "Pausing 10 seconds - hit CTRL-C to abort.");
      sleep(10);
    }

    message(2, "Process can be aborted with QUIT signal (usually CTRL-\\).");
    if (cdr->preventMediumRemoval(1) != 0) {
      exitCode = 1; goto fail;
    }

    if (writeDiskAtOnce(toc, cdr, FIFO_BUFFERS, SWAP, 0, 0) != 0) {
      if (cdr->simulate()) {
	message(-2, "Simulation failed.");
      }
      else {
	message(-2, "Writing failed.");
      }
      cdr->preventMediumRemoval(0);
      cdr->rezeroUnit(0);
      exitCode = 1; goto fail;
    }

    if (cdr->simulate()) {
      message(1, "Simulation finished successfully.");
    }
    else {
      message(1, "Writing finished successfully.");
    }

    cdr->rezeroUnit(0);

    if (cdr->preventMediumRemoval(0) != 0) {
      exitCode = 1; goto fail;
    }

    if (EJECT) {
      cdr->loadUnload(1);
    }
    break;

  case COPY_CD:
    if (cdr != srcCdr) {
      if (di->valid.empty && !di->empty && 
	  (!di->valid.append || !di->append)) {
	message(-2,
		"Medium in recorder device is not empty and not appendable.");
	exitCode = 1; goto fail;
      }
    }

    if (srcDi->valid.empty && srcDi->empty) {
      message(-2, "Medium in source device is empty.");
      exitCode = 1; goto fail;
    }
    
    cdr->simulate(WRITE_SIMULATE);
    cdr->force(FORCE);
    cdr->remote(REMOTE_MODE, REMOTE_FD);

    cdr->bufferUnderRunProtection(BUFFER_UNDER_RUN_PROTECTION);
    cdr->writeSpeedControl(WRITE_SPEED_CONTROL);
    
    if (MULTI_SESSION != 0) {
      if (cdr->multiSession(1) != 0) {
	message(-2, "This driver does not support multi session discs.");
	exitCode = 1; goto fail;
      }
    }

    if (WRITING_SPEED >= 0) {
      if (cdr->speed(WRITING_SPEED) != 0) {
	message(-2, "Writing speed %d not supported by device.",
		WRITING_SPEED);
	exitCode = 1; goto fail;
      }
    }

    srcCdr->paranoiaMode(PARANOIA_MODE);
    srcCdr->subChanReadMode(READ_SUBCHAN_MODE);
    srcCdr->fastTocReading(FAST_TOC);
    srcCdr->force(FORCE);
    
    if (ON_THE_FLY)
      message(1, "Starting on-the-fly CD copy ");
    else
      message(1, "Starting CD copy ");
    if (cdr->simulate()) {
      message(1, "simulation ");
    }
    message(1, "at speed %d...", cdr->speed());
    if (cdr->multiSession() != 0) {
      message(1, "Using multi session mode.");
    }

    if (ON_THE_FLY) {
      if (srcCdr == cdr) {
	message(-2, "Two different device are required for on-the-fly copying.");
	message(-2, "Please use option '--source-device x,y,z'.");
	exitCode = 1; goto fail;
      }

      if (copyCdOnTheFly(srcCdr, cdr, SESSION, FIFO_BUFFERS, SWAP,
			 EJECT, FORCE) == 0) {
	message(1, "On-the-fly CD copying finished successfully.");
      }
      else {
	message(-2, "On-the-fly CD copying failed.");
	exitCode = 1; goto fail;
      }
    }
    else {
      if (srcCdr != cdr)
	srcCdr->remote(REMOTE_MODE, REMOTE_FD);

      if (copyCd(srcCdr, cdr, SESSION, DATA_FILENAME, FIFO_BUFFERS, SWAP,
		 EJECT, FORCE, KEEPIMAGE) == 0) {
	message(1, "CD copying finished successfully.");
      }
      else {
	message(-2, "CD copying failed.");
	exitCode = 1; goto fail;
      }
    }
    break;

  case BLANK:
    if (WRITING_SPEED >= 0) {
      if (cdr->speed(WRITING_SPEED) != 0) {
	message(-2, "Blanking speed %d not supported by device.",
		WRITING_SPEED);
	exitCode = 1; goto fail;
      }
    }

    cdr->remote(REMOTE_MODE, REMOTE_FD);
    cdr->simulate(WRITE_SIMULATE);

    message(1, "Blanking disk...");
    if (cdr->blankDisk(BLANKING_MODE) != 0) {
      message(-2, "Blanking failed.");
      exitCode = 1; goto fail;
    }

    if (EJECT)
      cdr->loadUnload(1);
    break;

  case UNLOCK:
    message(1, "Trying to unlock drive...");

    cdr->abortDao();

    if (cdr->preventMediumRemoval(0) != 0) {
      exitCode = 1; goto fail;
    }

    if (EJECT)
      cdr->loadUnload(1);
    break;

  case UNKNOWN:
    // should not happen
    break;
  }

fail:
  delete cdr;
  delete cdrScsi;

  if (delSrcDevice) {
    delete srcCdr;
    delete srcCdrScsi;
  }

  delete toc;

  exit(exitCode);
}

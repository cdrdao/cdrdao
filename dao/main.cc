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
#include <pwd.h>
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
#include <ctype.h>
#include <list>
#include <string>
#include <assert.h>

#include "log.h"
#include "util.h"
#include "Toc.h"
#include "ScsiIf.h"
#include "CdrDriver.h"
#include "dao.h"
#include "port.h"
#include "Settings.h"
#include "Cddb.h"
#include "TempFileManager.h"
#include "FormatConverter.h"

#ifdef __CYGWIN__
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_GET_CAPABILITIES     CTL_CODE(IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_ADDRESS          CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifdef UNIXWARE
extern "C" {
  extern int seteuid(uid_t);
  extern int setegid(uid_t);
};
#endif

typedef enum {
    UNKNOWN = -1,
    SHOW_TOC,
    SHOW_DATA,
    READ_TEST,
    SIMULATE,
    WRITE,
    READ_TOC,
    DISK_INFO,
    READ_CD,
    TOC_INFO,
    TOC_SIZE,
    BLANK,
    SCAN_BUS,
    UNLOCK,
    COPY_CD,
    READ_CDDB,
    MSINFO,
    DRIVE_INFO,
    DISCID,
    SHOW_VERSION,
    LAST_CMD,
} DaoCommand;

typedef enum {
    NO_DEVICE,
    NEED_CDR_R,
    NEED_CDR_W,
    NEED_CDRW_W,
} DaoDeviceType;

// The cmdInfo[] array provides information about each of cdrdao's
// main commands, including some of the basic processing steps
// required, almost as a simplified state machine.

static struct {
    // The command code, which is also the index into the array
    DaoCommand cmd;
    // The command-line string for this command
    const char* str;
    // What type of device does the command require, for device
    // auto-detection.
    DaoDeviceType requiredDevice;
    // Does the command require a toc file
    bool needTocFile;
    // Does the command require to parse an existing toc file
    bool tocParse;
    // Does the command require to check the parsed toc file
    bool tocCheck;
} cmdInfo[LAST_CMD] = {
    { SHOW_TOC,     "show-toc",   NO_DEVICE,   1, 1, 0 },
    { SHOW_DATA,    "show-data",  NO_DEVICE,   1, 1, 1 },
    { READ_TEST,    "read-test",  NO_DEVICE,   1, 1, 1 },
    { SIMULATE,     "simulate",   NEED_CDR_W,  1, 1, 1 },
    { WRITE,        "write",      NEED_CDR_W,  1, 1, 1 },
    { READ_TOC,     "read-toc",   NEED_CDR_R,  1, 0, 1 },
    { DISK_INFO,    "disk-info",  NEED_CDR_R,  0, 0, 1 },
    { READ_CD,      "read-cd",    NEED_CDR_R,  1, 0, 1 },
    { TOC_INFO,     "toc-info",   NO_DEVICE,   1, 1, 1 },
    { TOC_SIZE,     "toc-size",   NO_DEVICE,   1, 1, 1 },
    { BLANK,        "blank",      NEED_CDRW_W, 0, 0, 1 },
    { SCAN_BUS,     "scanbus",    NO_DEVICE,   0, 0, 1 },
    { UNLOCK,       "unlock",     NEED_CDR_R,  0, 0, 1 },
    { COPY_CD,      "copy",       NEED_CDR_W,  0, 0, 1 },
    { READ_CDDB,    "read-cddb",  NO_DEVICE,   1, 1, 0 },
    { MSINFO,       "msinfo",     NEED_CDR_R,  0, 0, 1 },
    { DRIVE_INFO,   "drive-info", NEED_CDR_R,  0, 0, 1 },
    { DISCID,       "discid",     NEED_CDR_R,  0, 0, 1 },
    { SHOW_VERSION, "version",    NO_DEVICE,   0, 0, 0 },
};

typedef struct {

    DaoCommand command;

    const char* progName;
    const char* tocFile;
    const char* driverId;
    const char* sourceDriverId;
    const char* scsiDevice;
    const char* sourceScsiDevice;
    const char* dataFilename;
    const char* cddbLocalDbDir;
    const char* tmpFileDir;
    const char* cddbServerList;

    int  readingSpeed;
    int  writingSpeed;
    bool eject;
    bool swap;
    bool multiSession;
    int  verbose;
    int  session;
    int  fifoBuffers;
    bool fastToc;
    bool pause;
    bool readRaw;
    bool mode2Mixed;
    bool remoteMode;
    int  remoteFd;
    bool reload;
    bool force;
    int  paranoiaMode;
    bool onTheFly;
    bool writeSimulate;
    bool saveSettings;
    int  userCapacity;
    bool fullBurn;
    int  cddbTimeout;
    bool withCddb;
    bool taoSource;
    int  taoSourceAdjust;
    bool keepImage;
    bool overburn;
    int  bufferUnderrunProtection;
    bool writeSpeedControl;
    bool keep;
    bool printQuery;

    CdrDriver::BlankingMode blankingMode;
    TrackData::SubChannelMode readSubchanMode;

} DaoCommandLine;

static bool isNT = false;

#ifdef __CYGWIN__
/*! \brief OS handle to the device
	As obtained from CreateFile, used to apply OS level locking.
*/
static HANDLE fh = NULL;
/*! \brief Device string
	Like "\\\\.\\E:", used in CreateFile to obtain handle to device.
*/
static char devstr[10];
#endif

static void printVersion()
{
  log_message(2, "Cdrdao version %s - (C) Andreas Mueller <andreas@daneb.de>",
	  VERSION);

  std::list<std::string> list;
  int num = formatConverter.supportedExtensions(list);

  if (num) {
    std::string msg = "Format converter enabled for extensions:";
    std::list<std::string>::iterator i = list.begin();
    for (;i != list.end(); i++) {
      msg += " ";
      msg += (*i);
    }
    log_message(3, msg.c_str());
  }
}

static void setOptionDefaults(DaoCommandLine* options)
{
    memset(options, 0, sizeof(DaoCommandLine));

    options->readingSpeed = -1;
    options->writingSpeed = -1;
    options->command = UNKNOWN;
    options->verbose = 2;
    options->session = 1;
    options->pause = true;
    options->mode2Mixed = true;
    options->remoteFd = -1;
    options->paranoiaMode = 3;
    options->cddbTimeout = 60;
    options->taoSourceAdjust = -1;
    options->bufferUnderrunProtection = 1;
    options->writeSpeedControl = true;
    options->keep = false;
    options->printQuery = false;
#if defined(__FreeBSD__)
    options->fifoBuffers = 20;
#else
    options->fifoBuffers = 32;
#endif
    options->cddbServerList = "freedb.freedb.org freedb.freedb.org"
	":/~cddb/cddb.cgi uk.freedb.org uk.freedb.org:/~cddb/cddb.cgi"
	"cz.freedb.org cz.freedb.org:/~cddb/cddb.cgi";

    options->blankingMode = CdrDriver::BLANK_MINIMAL;
    options->readSubchanMode = TrackData::SUBCHAN_NONE;
}

static void printUsage(DaoCommandLine* options)
{
    switch (options->command) {

  case UNKNOWN:
    log_message(0, "\nUsage: %s <command> [options] [toc-file]",
		options->progName);
    log_message(0,
"command:\n"
"  show-toc   - prints out toc and exits\n"
"  toc-info   - prints out short toc-file summary\n"
"  toc-size   - prints total number of blocks for toc\n"
"  read-toc   - create toc file from audio CD\n"
"  read-cd    - create toc and rip audio data from CD\n"
"  read-cddb  - contact CDDB server and add data as CD-TEXT to toc-file\n"
"  show-data  - prints out audio data and exits\n"
"  read-test  - reads all audio files and exits\n"
"  disk-info  - shows information about inserted medium\n"
"  discid     - prints out CDDB information\n"
"  msinfo     - shows multi session info, output is suited for scripts\n"
"  drive-info - shows drive information\n"
"  unlock     - unlock drive after failed writing\n"
"  blank      - blank a CD-RW\n"
"  scanbus    - scan for devices\n"
"  simulate   - shortcut for 'write --simulate'\n"
"  write      - writes CD\n"
"  copy       - copies CD\n");
    
    log_message(0, "\n Try '%s <command> -h' to get a list of available "
		"options\n", options->progName);
    break;
    
  case SHOW_TOC:
    log_message(0, "\nUsage: %s show-toc [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --tmpdir <path>         - sets directory for temporary wav files\n"
"  --keep                  - keep generated temp wav files after exit\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case SHOW_DATA:
    log_message(0, "\nUsage: %s show-data [--swap] [-v #] toc-file\n",
		options->progName);
    break;
    
  case READ_TEST:
    log_message(0, "\nUsage: %s read-test [-v #] toc-file\n",
		options->progName);
    break;
  
  case SIMULATE:
    log_message(0, "\nUsage: %s simulate [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --multi                 - session will not be closed\n"
"  --overburn              - allow to overburn a medium\n"
"  --full-burn             - force burning to the outer disk edge\n"
"                            with '--driver generic-mmc-raw'\n"
"  --capacity <minutes>    - sets disk capacity for '--full-burn'\n"
"                            you must specify this when using blanks bigger\n"
"                            than 80 min. (90,99,etc.)\n"
"                            because they seems like 80 min. blanks\n"
"  --eject                 - ejects cd after simulation\n"
"  --swap                  - swap byte order of audio files\n"
"  --buffers #             - sets fifo buffer size (min. 10)\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --force                 - force execution of operation\n"
"  --tmpdir <path>         - sets directory for temporary wav files\n"
"  --keep                  - keep generated temp wav files after exit\n"
"  -v #                    - sets verbose level\n"
"  -n                      - no pause before writing\n");
    break;
    
  case WRITE:
    log_message(0, "\nUsage: %s write [options] toc-file", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --simulate              - just perform a write simulation\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --multi                 - session will not be closed\n"
"  --buffer-under-run-protection #\n"
"                          - 0: disable buffer under run protection\n"
"                            1: enable buffer under run protection (default)\n"
"  --write-speed-control # - 0: disable writing speed control by the drive\n"
"                            1: enable writing speed control (default)\n" 
"  --overburn              - allow to overburn a medium\n"
"  --full-burn             - force burning to the outer disk edge\n"
"                            with '--driver generic-mmc-raw'\n"
"  --capacity <minutes>    - sets disk capacity for '--full-burn'\n"
"                            you must specify this when using blanks bigger\n"
"                            than 80 min. (90,99,etc.)\n"
"                            because they seems like 80 min. blanks\n"
"  --eject                 - ejects cd after writing or simulation\n"
"  --swap                  - swap byte order of audio files\n"
"  --buffers #             - sets fifo buffer size (min. 10)\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --force                 - force execution of operation\n"
"  --tmpdir <path>         - sets directory for temporary wav files\n"
"  --keep                  - keep generated temp wav files after exit\n"
"  -v #                    - sets verbose level\n"
"  -n                      - no pause before writing\n");
    break;
    
  case READ_TOC:
    log_message(0, "\nUsage: %s read-toc [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>    - force usage of specified driver for source device\n"
"  --datafile <filename>   - name of data file placed in toc-file\n"
"  --session #             - select session\n"
"  --fast-toc              - do not extract pre-gaps and index marks\n"
"  --read-raw              - select raw sectors modes for data tracks\n"
"  --no-mode2-mixed        - don't switch to mode2_mixed\n"
"  --rspeed <read-speed>   - selects reading speed\n"
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
    log_message(0, "\nUsage: %s disk-info [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case DISCID:
    log_message(0, "\nUsage: %s discid [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  --query-string          - prints out CDDB query only\n"
"  -v #                    - sets verbose level\n");
    break;
   
  case READ_CD:
    log_message(0, "\nUsage: %s read-cd [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>    - force usage of specified driver for source device\n"
"  --datafile <filename>   - name of data file placed in toc-file\n"
"  --session #             - select session\n"
"  --fast-toc              - do not extract pre-gaps and index marks\n"
"  --read-raw              - read raw data sectors (including L-EC data)\n"
"  --no-mode2-mixed        - don't switch to mode2_mixed\n"
"  --rspeed <read-speed>   - selects reading speed\n"
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
      log_message(0, "\nUsage: %s toc-info [options] toc-file",
		  options->progName);
    log_message(0,
"options:\n"
"  --tmpdir <path>         - sets directory for temporary wav files\n"
"  --keep                  - keep generated temp wav files after exit\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case TOC_SIZE:
    log_message(0, "\nUsage: %s toc-size [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --tmpdir <path>         - sets directory for temporary wav files\n"
"  --keep                  - keep generated temp wav files after exit\n"
"  -v #                    - sets verbose level\n");
    break;

  case BLANK:
    log_message(0, "\nUsage: %s blank [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --blank-mode <mode>     - blank mode ('full', 'minimal')\n"
"  --eject                 - ejects cd after writing or simulation\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case SCAN_BUS:
    log_message(0, "\nUsage: %s scan-bus [-v #]\n", options->progName);
    break;
    
  case UNLOCK:
    log_message(0, "\nUsage: %s unlock [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --reload                - reload the disk if necessary for writing\n"
"  --eject                 - ejects cd after unlocking\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case DRIVE_INFO:
    log_message(0, "\nUsage: %s drive-info [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case COPY_CD:
    log_message(0, "\nUsage: %s copy [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --source-device {<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
"  --driver <id>           - force usage of specified driver\n"
"  --source-driver <id>    - force usage of specified driver for source device\n"
"  --simulate              - just perform a copy simulation\n"
"  --speed <writing-speed> - selects writing speed\n"
"  --rspeed <read-speed>   - selects reading speed\n"
"  --multi                 - session will not be closed\n"
"  --buffer-under-run-protection #\n"
"                          - 0: disable buffer under run protection\n"
"                            1: enable buffer under run protection (default)\n"
"  --write-speed-control # - 0: disable writing speed control by the drive\n"
"                            1: enable writing speed control (default)\n" 
"  --overburn              - allow to overburn a medium\n"
"  --full-burn             - force burning to the outer disk edge\n"
"                            with '--driver generic-mmc-raw'\n"
"  --capacity <minutes>    - sets disk capacity for '--full-burn'\n"
"                            you must specify this when using blanks bigger\n"
"                            than 80 min. (90,99,etc.)\n"
"                            because they seems like 80 min. blanks\n"
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
"  -n                      - no pause before writing\n");
  break;
  
  case READ_CDDB:
    log_message(0, "\nUsage: %s read-cddb [options] toc-file",
		options->progName);
    log_message(0,
"options:\n"
"  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
"  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
"  --cddb-directory <path> - path to local CDDB directory where fetched\n"
"                            CDDB records will be stored\n"
"  -v #                    - sets verbose level\n");
    break;
    
  case MSINFO:
    log_message(0, "\nUsage: %s msinfo [options]", options->progName);
    log_message(0,
"options:\n"
"  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
"  --driver <id>           - force usage of specified driver\n"
"  --reload                - reload the disk if necessary for writing\n"
"  -v #                    - sets verbose level\n");
    break;
    
  default:
    log_message(0, "Sorry, no help available for command %d :-(\n",
		options->command);
    break;
  }
  
}

static void importSettings(DaoCommandLine* opts, Settings* settings)
{
    const char *sval;
    const int *ival;

    DaoCommand cmd = opts->command;

    if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
	if ((sval = settings->getString(Settings::setWriteDriver)) != NULL) {
	    opts->driverId = strdupCC(sval);
	}

	if ((sval = settings->getString(Settings::setWriteDevice)) != NULL) {
	    opts->scsiDevice = strdupCC(sval);
	}
    
	if ((ival = settings->getInteger(Settings::setWriteSpeed)) != NULL &&
	    *ival >= 0) {
	    opts->writingSpeed = *ival;
	}

	if ((ival = settings->getInteger(Settings::setWriteBuffers)) != NULL &&
	    *ival >= 10) {
	    opts->fifoBuffers = *ival;
	}
	if ((ival = settings->getInteger(Settings::setUserCapacity)) != NULL &&
	    *ival >= 0) {
	    opts->userCapacity = *ival;
	}
	if ((ival = settings->getInteger(Settings::setFullBurn)) != NULL &&
	    *ival >= 0) {
	    opts->fullBurn = *ival;
	}
    }

    if (cmd == READ_CD || cmd == READ_TOC) {
	if ((sval = settings->getString(Settings::setReadDriver)) != NULL) {
	    opts->driverId = strdupCC(sval);
	}

	if ((sval = settings->getString(Settings::setReadDevice)) != NULL) {
	    opts->scsiDevice = strdupCC(sval);
	}

	if ((ival = settings->getInteger(Settings::setReadParanoiaMode)) != NULL &&
	    *ival >= 0) {
	    opts->paranoiaMode = *ival;
	}
    }

    if (cmd == COPY_CD) {
	if ((sval = settings->getString(Settings::setReadDriver)) != NULL) {
	    opts->sourceDriverId = strdupCC(sval);
	}

	if ((sval = settings->getString(Settings::setReadDevice)) != NULL) {
	    opts->sourceScsiDevice = strdupCC(sval);
	}
    
	if ((ival = settings->getInteger(Settings::setReadParanoiaMode)) != NULL &&
	    *ival >= 0) {
	    opts->paranoiaMode = *ival;
	}
    }

    if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK ||
	cmd == DISCID || cmd == DRIVE_INFO) {
	if ((sval = settings->getString(Settings::setWriteDriver)) != NULL) {
	    opts->driverId = strdupCC(sval);
	}

	if ((sval = settings->getString(Settings::setWriteDevice)) != NULL) {
	    opts->scsiDevice = strdupCC(sval);
	}
    }

    if (cmd == READ_CDDB || cmd == COPY_CD || cmd == READ_TOC ||
	cmd == READ_CD || cmd == DISCID) {
	if ((sval = settings->getString(Settings::setCddbServerList)) != NULL) {
	    opts->cddbServerList = strdupCC(sval);
	}

	if ((sval = settings->getString(Settings::setCddbDbDir)) != NULL) {
	    opts->cddbLocalDbDir = strdupCC(sval);
	}

	if ((ival = settings->getInteger(Settings::setCddbTimeout)) != NULL &&
	    *ival > 0) {
	    opts->cddbTimeout = *ival;
	}
	if ((sval = settings->getString(Settings::setTmpFileDir)) != NULL) {
	    opts->tmpFileDir = strdupCC(sval);
	}
    }

    if ((ival = settings->getInteger(Settings::setReadSpeed)) != NULL &&
	*ival >= 0) {
	opts->readingSpeed = *ival;
    }  
}

static void exportSettings(DaoCommandLine* opts, Settings* settings)
{
    DaoCommand cmd = opts->command;

    if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
	if (opts->driverId != NULL)
	    settings->set(Settings::setWriteDriver, opts->driverId);
    
	if (opts->scsiDevice != NULL)
	    settings->set(Settings::setWriteDevice, opts->scsiDevice);

	if (opts->writingSpeed >= 0) {
	    settings->set(Settings::setWriteSpeed, opts->writingSpeed);
    }

	if (opts->fifoBuffers > 0) {
	    settings->set(Settings::setWriteBuffers, opts->fifoBuffers);
	}

	if (opts->fullBurn > 0) {
	    settings->set(Settings::setFullBurn, opts->fullBurn);
	}

	if (opts->userCapacity > 0) {
	    settings->set(Settings::setUserCapacity, opts->userCapacity);
	}
    }

    if (cmd == READ_CD) {
	if (opts->driverId != NULL)
	    settings->set(Settings::setReadDriver, opts->driverId);

	if (opts->scsiDevice != NULL)
	    settings->set(Settings::setReadDevice, opts->scsiDevice);

	settings->set(Settings::setReadParanoiaMode, opts->paranoiaMode);
    }

    if (cmd == COPY_CD) {
	if (opts->sourceDriverId != NULL)
	    settings->set(Settings::setReadDriver, opts->sourceDriverId);

	if (opts->sourceScsiDevice != NULL)
	    settings->set(Settings::setReadDevice, opts->sourceScsiDevice);

	settings->set(Settings::setReadParanoiaMode, opts->paranoiaMode);
    }

    if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK ||
	cmd == DISCID || cmd == DRIVE_INFO) {
	if (opts->driverId != NULL)
	    settings->set(Settings::setWriteDriver, opts->driverId);
    
	if (opts->scsiDevice != NULL)
	    settings->set(Settings::setWriteDevice, opts->scsiDevice);
    }

    if (cmd == READ_CDDB ||
	(opts->withCddb && (cmd == COPY_CD || cmd == READ_TOC ||
			      cmd == READ_CD || cmd == DISCID))) {
	if (opts->cddbServerList != NULL) {
	    settings->set(Settings::setCddbServerList, opts->cddbServerList);
	}

	if (opts->cddbLocalDbDir != NULL) {
	    settings->set(Settings::setCddbDbDir, opts->cddbLocalDbDir);
	}

	if (opts->cddbTimeout > 0) {
	    settings->set(Settings::setCddbTimeout, opts->cddbTimeout);
	}
    }

    if (opts->readingSpeed >= 0) {
	settings->set(Settings::setReadSpeed, opts->readingSpeed);
    }

    if (cmd == SHOW_TOC || cmd == SIMULATE || cmd == WRITE ||
	cmd == TOC_INFO || cmd == TOC_SIZE) {
	settings->set(Settings::setTmpFileDir, opts->tmpFileDir);
    }
}

static int parseCmdline(int argc, char **argv, DaoCommandLine* opts,
			Settings* settings)
{
    int i;

    if (argc < 1) {
	return 1;
    }

    for (i = 0; i < LAST_CMD; i++) {
	if (strcmp(*argv, cmdInfo[i].str) == 0) {
	    opts->command = cmdInfo[i].cmd;
	    break;
	}
    }

    if (opts->command == UNKNOWN) {
	log_message(-2, "Illegal command: %s", *argv);
	return 1;
    }

    // retrieve settings from $HOME/.cdrdao for given command
    importSettings(opts, settings);

    argc--, argv++;

    while (argc > 0 && (*argv)[0] == '-') {

	if ((*argv)[1] != '-') {
	    switch ((*argv)[1]) {
	    case 'h':
		return 1;
	
	    case 'v':
		if ((*argv)[2] != 0) {
		    opts->verbose = atoi((*argv) + 2);
		} else {
		    if (argc < 2) {
			log_message(-2, "Missing argument after: %s", *argv);
			return 1;
		    }  else {
			opts->verbose = atoi(argv[1]);
			argc--, argv++;
		    }
		}
		break;

	    case 'n':
		opts->pause = false;
		break;

	    default:
		log_message(-2, "Illegal option: %s", *argv);
		return 1;
		break;
	    }
	} 
	else {

	    if (strcmp((*argv) + 2, "help") == 0) {
		return 1;
	    }
	    if (strcmp((*argv) + 2, "device") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->scsiDevice = argv[1];
		    argc--, argv++;
		}
	    } 
	    else if (strcmp((*argv) + 2, "source-device") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->sourceScsiDevice = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "rspeed") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->readingSpeed = atol(argv[1]);
		    if (opts->readingSpeed < 0) {
			log_message(-2, "Illegal reading speed: %s", argv[1]);
			return 1;
		    }
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "speed") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->writingSpeed = atol(argv[1]);
		    if (opts->writingSpeed < 0) {
			log_message(-2, "Illegal writing speed: %s", argv[1]);
			return 1;
		    }
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "capacity") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->userCapacity = atol(argv[1]);
		    if (opts->userCapacity < 0) {
			log_message(-2, "Illegal disk capacity: %s minutes",
				    argv[1]);
			return 1;
		    }
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "blank-mode") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    if (strcmp(argv[1], "full") == 0) {
			opts->blankingMode = CdrDriver::BLANK_FULL;
		    } else if (strcmp(argv[1], "minimal") == 0) {
			opts->blankingMode = CdrDriver::BLANK_MINIMAL;
		    } else {
			log_message(-2, "Illegal blank mode. Valid values: full minimal");
			return 1;
		    }
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "paranoia-mode") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->paranoiaMode= atol(argv[1]);
		    if (opts->paranoiaMode < 0) {
			log_message(-2, "Illegal paranoia mode: %s", argv[1]);
			return 1;
		    }
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "remote") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->remoteFd = atol(argv[1]);
		    if (opts->remoteFd < 0) {
			log_message(-2, "Invalid remote message file descriptor: %s", argv[1]);
			return 1;
		    }
		    opts->remoteMode = true;
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "eject") == 0) {
		opts->eject = true;
	    }
	    else if (strcmp((*argv) + 2, "swap") == 0) {
		opts->swap = true;
	    }
	    else if (strcmp((*argv) + 2, "query-string") == 0) {
		opts->printQuery = true;
	    }
	    else if (strcmp((*argv) + 2, "multi") == 0) {
		opts->multiSession = true;
	    }
	    else if (strcmp((*argv) + 2, "simulate") == 0) {
		opts->writeSimulate = true;
	    }
	    else if (strcmp((*argv) + 2, "fast-toc") == 0) {
		opts->fastToc = true;
	    }
	    else if (strcmp((*argv) + 2, "read-raw") == 0) {
		opts->readRaw = true;
	    }
	    else if (strcmp((*argv) + 2, "no-mode2-mixed") == 0) {
		opts->mode2Mixed = false;
	    }
	    else if (strcmp((*argv) + 2, "reload") == 0) {
		opts->reload = true;
	    }
	    else if (strcmp((*argv) + 2, "force") == 0) {
		opts->force = true;
	    }
	    else if (strcmp((*argv) + 2, "keep") == 0) {
		opts->keep = true;
	    }
	    else if (strcmp((*argv) + 2, "on-the-fly") == 0) {
		opts->onTheFly = true;
	    }
	    else if (strcmp((*argv) + 2, "save") == 0) {
		opts->saveSettings = true;
	    }
	    else if (strcmp((*argv) + 2, "tao-source") == 0) {
		opts->taoSource = true;
	    }
	    else if (strcmp((*argv) + 2, "keepimage") == 0) {
		opts->keepImage = true;
	    }
	    else if (strcmp((*argv) + 2, "overburn") == 0) {
		opts->overburn = true;
	    }
	    else if (strcmp((*argv) + 2, "full-burn") == 0) {
		opts->fullBurn = true;
	    }
	    else if (strcmp((*argv) + 2, "with-cddb") == 0) {
		opts->withCddb = true;
	    }
	    else if (strcmp((*argv) + 2, "driver") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->driverId = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "source-driver") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->sourceDriverId = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "datafile") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->dataFilename = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "buffers") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->fifoBuffers = atoi(argv[1]);
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "session") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->session = atoi(argv[1]);
		    argc--, argv++;
		    if (opts->session < 1) {
			log_message(-2, "Illegal session number: %d",
				    opts->session);
			return 1;
		    }
		}
	    }
	    else if (strcmp((*argv) + 2, "cddb-servers") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->cddbServerList = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "cddb-directory") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->cddbLocalDbDir = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "tmpdir") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->tmpFileDir = argv[1];
		    argc--, argv++;
		}
	    }
	    else if (strcmp((*argv) + 2, "cddb-timeout") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->cddbTimeout = atoi(argv[1]);
		    argc--, argv++;
		    if (opts->cddbTimeout < 1) {
			log_message(-2, "Illegal CDDB timeout: %d",
				    opts->cddbTimeout);
			return 1;
		    }
		}
	    }
	    else if (strcmp((*argv) + 2, "tao-source-adjust") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    opts->taoSourceAdjust = atoi(argv[1]);
		    argc--, argv++;
		    if (opts->taoSourceAdjust < 0 ||
			opts->taoSourceAdjust >= 100) {
			log_message(-2, "Illegal number of TAO link blocks: %d",
				    opts->taoSourceAdjust);
			return 1;
		    }
		}
	    }
	    else if (strcmp((*argv) + 2, "buffer-under-run-protection") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    int val = atoi(argv[1]);
		    argc--, argv++;
		    if (val < 0 || val > 1) {
			log_message(-2, "Illegal value for option --buffer-under-run-protection: %d", val);
			return 1;
		    }
		    opts->bufferUnderrunProtection = val;
		}
	    }
	    else if (strcmp((*argv) + 2, "write-speed-control") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    int val = atoi(argv[1]);
		    argc--, argv++;
		    if (val < 0 || val > 1) {
			log_message(-2, "Illegal value for option --write-speed-control: %d", val);
			return 1;
		    }
		    opts->writeSpeedControl = val;
		}
	    }
	    else if (strcmp((*argv) + 2, "read-subchan") == 0) {
		if (argc < 2) {
		    log_message(-2, "Missing argument after: %s", *argv);
		    return 1;
		} else {
		    if (strcmp(argv[1], "rw") == 0) {
			opts->readSubchanMode = TrackData::SUBCHAN_RW;
		    } else if (strcmp(argv[1], "rw_raw") == 0) {
			opts->readSubchanMode = TrackData::SUBCHAN_RW_RAW;
		    } else {
			log_message(-2, "Invalid argument after %s: %s",
				    argv[0], argv[1]);
			return 1;
		    }

		    argc--, argv++;
		}
	    }
	    else {
		log_message(-2, "Illegal option: %s", *argv);
		return 1;
	    }
	}

	argc--, argv++;
    }

    if (cmdInfo[opts->command].needTocFile) {
	if (argc < 1) {
	    log_message(-2, "Missing toc-file.");
	    return 1;
	} else if (argc > 1) {
	    log_message(-2, "Expecting only one toc-file.");
	    return 1;
	}
	opts->tocFile = *argv;
    }


    return 0;
}

// Commit settings to overall system. Export them.
static void commitSettings(DaoCommandLine* opts, Settings* settings,
			   const char* settingsPath)
{
    if (opts->tmpFileDir)
	tempFileManager.setTempDirectory(opts->tmpFileDir);

    tempFileManager.setKeepTemps(opts->keep);

    if (opts->saveSettings && settingsPath != NULL) {
	// If we're saving our settings, give up root privileges and
	// exit. The --save option is only compiled in if setreuid() is
	// available (because it could be used for a local root exploit).
	if (giveUpRootPrivileges()) {
	    exportSettings(opts, settings);
	    settings->write(settingsPath);
	}
	exit(0);
    }
}

// Selects driver for device of 'scsiIf'.
static CdrDriver *selectDriver(DaoCommand cmd, ScsiIf *scsiIf,
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
      log_message(-2, "%s: Illegal driver ID, available drivers:", s);
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
      log_message(0, "");
      log_message(-2, "No driver found for '%s %s', available drivers:\n",
	      scsiIf->vendor(), scsiIf->product());
      CdrDriver::printDriverIds();

      log_message(0, "\nFor all recent recorder models either the 'generic-mmc' or");
      log_message(0, "the 'generic-mmc-raw' driver should work.");
      log_message(0, "Use option '--driver' to force usage of a driver, e.g.: --driver generic-mmc");
    }
  }

  return ret;
}

const char* getDefaultDevice(DaoDeviceType req)
{
    int i, len;
    static char buf[128];

    // This function should not be called if the command issues
    // doesn't actually require a device.
    assert(req != NO_DEVICE);

    ScsiIf::ScanData* sdata = ScsiIf::scan(&len);

    if (sdata) {
	for (i = 0; i < len; i++) {

	    ScsiIf testif(sdata[i].dev.c_str());

	    if (testif.init() != 0) {
		continue;
	    }
	    bool rr, rw, rwr, rww;

	    if (!testif.checkMmc(&rr, &rw, &rwr, &rww))
		continue;

	    if (req == NEED_CDR_R && !rr)
	      continue;
	    if (req == NEED_CDR_W && !rw)
	      continue;
	    if (req == NEED_CDRW_W && !rww)
	      continue;

	    strncpy(buf, sdata[i].dev.c_str(), 128);
	    delete[] sdata;
	    return buf;
	}
	delete[] sdata;
    }

    return NULL;
}

#define MAX_RETRY 10
static CdrDriver *setupDevice(DaoCommand cmd, const char *scsiDevice,
			      const char *driverId, int initDevice,
			      int checkReady, int checkEmpty,
			      int readingSpeed,
			      bool remote, bool reload)
{
  ScsiIf *scsiIf = NULL;
  CdrDriver *cdr = NULL;
  DiskInfo *di = NULL;
  int inquiryFailed = 0;
  int retry = 0;
  int initTries = 2;
  int ret = 0;

  scsiIf = new ScsiIf(scsiDevice);

  switch (scsiIf->init()) {
  case 1:
    log_message(-2, "Please use option '--device {[proto:]bus,id,lun}|device'"
	    ", e.g. "
            "--device 0,6,0, --device ATA:0,0,0 or --device /dev/cdrom");
    delete scsiIf;
    return NULL;
    break;
  case 2:
    inquiryFailed = 1;
    break;
  }
  
  log_message(2, "%s: %s %s\tRev: %s", scsiDevice, scsiIf->vendor(),
	  scsiIf->product(), scsiIf->revision());


  if (inquiryFailed && driverId == NULL) {
    log_message(-2, "Inquiry failed and no driver id is specified.");
    log_message(-2, "Please use option --driver to specify a driver id.");
    delete scsiIf;
    return NULL;
  }

  if ((cdr = selectDriver(cmd, scsiIf, driverId)) == NULL) {
    delete scsiIf;
    return NULL;
  }

  log_message(2, "Using driver: %s (options 0x%04lx)\n", cdr->driverName(),
	  cdr->options());

  if (!initDevice)
    return cdr;
      
  while (initTries > 0) {
    // clear unit attention
    cdr->rezeroUnit(0);
    if (readingSpeed >= 0) {
      if (!cdr->rspeed(readingSpeed)) {
        log_message(-2, "Reading speed %d is not supported by device.",
                readingSpeed);
        exit(1);
      }
    }

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
	log_message(-1, "Unit not ready, still trying...");
      }

      if (ret != 0) {
	log_message(-2, "Unit not ready, giving up.");
	delete cdr;
	delete scsiIf;
	return NULL;
      }
	
      cdr->rezeroUnit(0);

      if (readingSpeed >= 0) {
        log_message(0, "Setting reading speed %d.",
                readingSpeed); 
        if (!cdr->rspeed(readingSpeed)) {
          log_message(-2, "Reading speed %d is not supported by device.",
                  readingSpeed);
          exit(1);
        }
      }

      if ((di = cdr->diskInfo()) == NULL) {
	log_message(-2, "Cannot get disk information.");
	delete cdr;
	delete scsiIf;
	return NULL;
      }

      if (checkEmpty && initTries == 2 &&
	  di->valid.empty && !di->empty && 
	  (!di->valid.append || !di->append) &&
	  (!remote || reload)) {
	if (!reload) {
	  log_message(0, "Disk seems to be written - hit return to reload disk.");
	  fgetc(stdin);
	}

	log_message(1, "Reloading disk...");

	if (cdr->loadUnload(1) != 0) {
	  delete cdr;
	  delete scsiIf;
	  return NULL;
	}

	sleep(1);
	cdr->rezeroUnit(0); // clear unit attention

	if (cdr->loadUnload(0) != 0) {
	  log_message(-2, "Cannot load tray.");
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

#ifdef __CYGWIN__
/* 	Experimental device locking code. Should work on Win2k/NT only.  */
	typedef struct _SCSI_ADDRESS {
		ULONG Length;
		UCHAR PortNumber;
		UCHAR PathId;
		UCHAR TargetId;
		UCHAR Lun;
	}SCSI_ADDRESS, *PSCSI_ADDRESS;

	OSVERSIONINFO osinfo;
	osinfo.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
	if ((GetVersionEx (&osinfo)) && (osinfo.dwPlatformId == VER_PLATFORM_WIN32_NT))
		isNT = true;
	if (isNT)	{
		char devletter;
		SCSI_ADDRESS sa;
		DWORD bytes;
		bool gotit = false;
		int ha,id,lun;

		ha = scsiIf->bus ();
		id = scsiIf->id ();
		lun = scsiIf->lun ();

		for (devletter = 'A'; devletter <= 'Z'; devletter++)	{
			sprintf (devstr, "%c:\\\0", devletter);
			if (GetDriveType (devstr) != DRIVE_CDROM)
				continue;
			sprintf (devstr, "\\\\.\\%c:", devletter);
			fh = CreateFile (devstr,
				GENERIC_READ,
				0,
				NULL,
				OPEN_EXISTING,
				FILE_FLAG_WRITE_THROUGH|FILE_FLAG_NO_BUFFERING,
				NULL);
			if (fh == INVALID_HANDLE_VALUE)	{
				//~ printf ("Error opening device %s: %d\n", devstr, GetLastError());
				fh = NULL;
				continue;
			}
			if (DeviceIoControl (fh, IOCTL_SCSI_GET_ADDRESS, NULL, 0, &sa, sizeof(SCSI_ADDRESS), &bytes, NULL))	{
				if ( (ha == sa.PortNumber) && (lun == sa.Lun) && (id == sa.TargetId) )	{
					gotit = true;
					break;
				}	else	{
					CloseHandle (fh);
					fh = NULL;
					continue;
				}
			}
		}
		if (gotit)	{
			if (!DeviceIoControl (fh, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL))	{
				log_message(-2, "Couldn't lock device %s!", devstr);
				CloseHandle (fh);
				fh = NULL;
			}
			else
				log_message(2, "OS lock on device %s. Unit won't be accessible while burning.", devstr);
		}	else	{
			log_message(-2, "Unable to determine drive letter for device %s! No OS level locking.", scsiDevice);
			if (fh) CloseHandle (fh);
			fh = NULL;
		}
	}	else
		log_message(2,"You are running Windows 9x. No OS level locking available.");
#endif

  if (readingSpeed >= 0) {
    if (!cdr->rspeed(readingSpeed)) {
      log_message(-2, "Reading speed %d is not supported by device.",
              readingSpeed);
      exit(1);
    }
  }

  return cdr;
}

static void showDriveInfo(const DriveInfo *i)
{
  if (i == NULL) {
    log_message(0, "No drive information available.");
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

void showData(const Toc *toc, bool swap)
{
  long length = toc->length().lba();
  Sample buf[SAMPLES_PER_BLOCK];
  int i;
  unsigned long sampleNr = 0;
  long lba = 150;

  TocReader reader(toc);

  if (reader.openData() != 0) {
    log_message(-2, "Cannot open audio data.");
    return;
  }

  while (length > 0) {
    if (reader.readSamples(buf, SAMPLES_PER_BLOCK) != SAMPLES_PER_BLOCK) {
      log_message(-2, "Read of audio data failed.");
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

  log_message(2, "That data below may not reflect the real status of the inserted medium");
  log_message(2, "if a simulation run was performed before. Reload the medium in this case.");
  log_message(2, "");

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

static int readCddb(DaoCommandLine* opts, Toc *toc, bool showEntry = false)
{
  int err = 0;
  char *servers = strdupCC(opts->cddbServerList);
  char *p;
  const char *sep = " ,";
  char *user = NULL;
  char *host = NULL;
  struct passwd *pwent;
  Cddb::QueryResults *qres, *qrun, *qsel;
  Cddb::CddbEntry *dbEntry;

  Cddb cddb(toc);

  cddb.timeout(opts->cddbTimeout);

  if (opts->cddbLocalDbDir != NULL)
    cddb.localCddbDirectory(opts->cddbLocalDbDir);


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

  {
    struct utsname sinfo;
    if (uname(&sinfo) == 0) {
      host = strdupCC(sinfo.nodename);
    }
    else {
      host = strdupCC("unknown");
    }
  }
  

  if (cddb.connectDb(user, host, "cdrdao", VERSION) != 0) {
    log_message(-2, "Cannot connect to any CDDB server.");
    err = 2; goto fail;
  }

	
  if (cddb.queryDb(&qres) != 0) {
    log_message(-2, "Querying of CDDB server failed.");
    err = 2; goto fail;
  }
  
  if (qres == NULL) {
    log_message(1, "No CDDB record found for this toc-file.");
    err = 1; goto fail;
  }

  if (qres->next != NULL || !(qres->exactMatch)) {
    int qcount;

    if (qres->next == NULL)
      log_message(0, "Found following inexact match:");
    else
      log_message(0, "Found following inexact matches:");
    
    log_message(0, "\n    DISKID   CATEGORY     TITLE\n");
    
    for (qrun = qres, qcount = 0; qrun != NULL; qrun = qrun->next, qcount++) {
      log_message(0, "%2d. %-8s %-12s %s", qcount + 1, qrun->diskId,
	      qrun->category,  qrun->title);
    }

    log_message(0, "\n");

    qsel = NULL;

    while (1) {
      char buf[20];
      int sel;

      log_message(0, "Select match, 0 for none [0-%d]?", qcount);

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
      log_message(0, "No match selected.");
      err = 1; goto fail;
    }
  }
  else {
    qsel = qres;
  }


  log_message(1, "Reading CDDB record for: %s-%s-%s", qsel->diskId, qsel->category,
	  qsel->title);

  if (cddb.readDb(qsel->category, qsel->diskId, &dbEntry) != 0) {
    log_message(-2, "Reading of CDDB record failed.");
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
      log_message(0, "%s : %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
              sdata[i].product, sdata[i].revision);
    }
    delete[] sdata;
  }

#ifdef SCSI_ATAPI
  sdata = ScsiIf::scan(&len, "ATA");
  if (sdata) {
    for (i = 0; i < len; i++) {
      log_message(0, "%-20s %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
              sdata[i].product, sdata[i].revision);
    }
    delete[] sdata;
  } else {
    sdata = ScsiIf::scan(&len, "ATAPI");
    if (sdata) {
      for (i = 0; i < len; i++) {
        log_message(0, "%-20s %s, %s, %s", sdata[i].dev.c_str(), sdata[i].vendor,
                sdata[i].product, sdata[i].revision);
      }
      delete[] sdata;
    }
  }
#endif
}

static int checkToc(const Toc *toc, bool force)
{
    int ret = toc->check();

    if (ret == 0 || (ret == 1 && force))
	return 0;

    log_message(-2, "The toc check function detected at least one warning.");
    log_message(-2, "If you record this toc the resulting CD might be unusable");
    log_message(-2, "or the recording process might abort with error.");
    log_message(-2, "Use option --force to ignore the warnings.");

    return ret;
}

static int copyCd(DaoCommandLine* opts, CdrDriver *src, CdrDriver *dst)
{
    char dataFilenameBuf[50];
    long pid = getpid();
    Toc *toc;
    int ret = 0;
    DiskInfo *di = NULL;
    int isAppendable = 0;

    if (opts->dataFilename == NULL) {
	// create a unique temporary data file name in current directory
	sprintf(dataFilenameBuf, "cddata%ld.bin", pid);
	opts->dataFilename = dataFilenameBuf;
    }

    src->rawDataReading(true);
    src->taoSource(opts->taoSource);
    if (opts->taoSourceAdjust >= 0)
	src->taoSourceAdjust(opts->taoSourceAdjust);

    if ((toc = src->readDisk(opts->session, opts->dataFilename)) == NULL) {
	unlink(opts->dataFilename);
	log_message(-2, "Creation of source CD image failed.");
	return 1;
    }

    if (opts->withCddb) {
	if (readCddb(opts, toc) == 0)
	    log_message(2, "CD-TEXT data was added to toc-file.");
	else
	    log_message(2, "Reading of CD-TEXT data failed - "
			"continuing without CD-TEXT data.");
    }

    if (opts->keepImage) {
	char tocFilename[50];

	sprintf(tocFilename, "cd%ld.toc", pid);
    
	log_message(2, "Keeping created image file \"%s\".",
		    opts->dataFilename);
	log_message(2, "Corresponding toc-file is written to \"%s\".",
		    tocFilename);

	toc->write(tocFilename);
    }

    if (checkToc(toc, opts->force)) {
	log_message(-3, "Toc created from source CD image is inconsistent.");
	toc->print(std::cout);
	delete toc;
	return 1;
    }

    switch (dst->checkToc(toc)) {
    case 0: // OK
	break;
    case 1: // warning
	if (!opts->force) {
	    log_message(-2, "The toc extracted from the source CD is not suitable for");
	    log_message(-2, "the recorder device and may produce undefined results.");
	    log_message(-2, "Use option --force to use it anyway.");
	    delete toc;
	    return 1;
	}
	break;
    default: // error
	log_message(-2, "The toc extracted from the source CD is not suitable for this drive.");
	delete toc;
	return 1;
	break;
    }

    if (src == dst) {
	// Unlock src to make swaping possible
	src->preventMediumRemoval(0);
	log_message(0, "Please insert a recordable medium and hit enter.");
	getc(stdin);
    }

    do {
	if ((di = dst->diskInfo()) == NULL) {
	    log_message(-2, "Cannot get disk information from recorder device.");
	    delete toc;
	    return 1;
	}

	if (!di->valid.empty) {
	    log_message(-2, "Cannot determine disk status - hit enter to try again.");
	    getc(stdin);
	    isAppendable = 0;
	} else if (!di->empty && (!di->valid.append || !di->append)) {
	    log_message(0, "Medium in recorder device is not empty and not appendable.");
	    log_message(0, "Please insert a recordable medium and hit enter.");
	    getc(stdin);
	    isAppendable = 0;
	} else {
	    isAppendable = 1;
	}
    } while (!isAppendable);


    if (dst->preventMediumRemoval(1) != 0) {
	if (!opts->keepImage) {
	    if (unlink(opts->dataFilename) != 0)
		log_message(-2, "Cannot remove CD image file \"%s\": %s",
			    opts->dataFilename,
			    strerror(errno));
	}

	delete toc;
	return 1;
    }
    
    if (writeDiskAtOnce(toc, dst, opts->fifoBuffers, opts->swap, 0, 0) != 0) {
	if (dst->simulate())
	    log_message(-2, "Simulation failed.");
	else
	    log_message(-2, "Writing failed.");

	ret = 1;
    } else {
	if (dst->simulate())
	    log_message(1, "Simulation finished successfully.");
	else
	    log_message(1, "Writing finished successfully.");
    }

    if (dst->preventMediumRemoval(0) != 0)
	ret = 1;

    dst->rezeroUnit(0);

    if (ret == 0 && opts->eject) {
	dst->loadUnload(1);
    }

    if (!opts->keepImage) {
	if (unlink(opts->dataFilename) != 0)
	    log_message(-2, "Cannot remove CD image file \"%s\": %s",
			opts->dataFilename,
			strerror(errno));
    }

    delete toc;

    return ret;
}

static int copyCdOnTheFly(DaoCommandLine* opts,CdrDriver *src, CdrDriver *dst)
{
    Toc *toc = NULL;
    int pipeFds[2];
    pid_t pid = -1;
    int ret = 0;
    int oldStdin = -1;

    if (src == dst)
	return 1;

    if (pipe(pipeFds) != 0) {
	log_message(-2, "Cannot create pipe: %s", strerror(errno));
	return 1;
    }
  
    src->rawDataReading(true);
    src->taoSource(opts->taoSource);
    if (opts->taoSourceAdjust >= 0)
	src->taoSourceAdjust(opts->taoSourceAdjust);

    src->onTheFly(1);
    // the fd is not used by 'readDiskToc', just need to
    // indicate that on-the-fly copying is active for
    // automatical selection if the first track's pre-gap
    // is padded with zeros in the created Toc.

    if ((toc = src->readDiskToc(opts->session, "-")) == NULL) {
	log_message(-2, "Creation of source CD toc failed.");
	ret = 1;
	goto fail;
    }

    if (opts->withCddb) {
	if (readCddb(opts, toc) != 0) {
	    ret = 1;
	    goto fail;
	} else {
	    log_message(2, "CD-TEXT data was added to toc.");
	}
    }
  
    if (checkToc(toc, opts->force) != 0) {
	log_message(-3, "Toc created from source CD image is inconsistent"
		    "- please report.");
	toc->print(std::cout);
	ret = 1;
	goto fail;
    }

    switch (dst->checkToc(toc)) {
    case 0: // OK
	break;
    case 1: // warning
	if (!opts->force) {
	    log_message(-2, "The toc extracted from the source CD is not suitable for");
	    log_message(-2, "the recorder device and may produce undefined results.");
	    log_message(-2, "Use option --force to use it anyway.");
	    ret = 1;
	    goto fail;
	}
	break;
    default: // error
	log_message(-2, "The toc extracted from the source CD is not suitable for this drive.");
	ret = 1;
	goto fail;
	break;
    }

    if ((pid = fork()) < 0) {
	log_message(-2, "Cannot fork reader process: %s", strerror(errno));
	ret = 1;
	goto fail;
    }

    if (pid == 0) {
	// we are the reader process
	setsid();
	close(pipeFds[0]);

	src->onTheFly(pipeFds[1]);

	opts->verbose = 0;

#ifdef __CYGWIN__
	/* Close the SCSI interface and open it again. This will re-init the
	 * ASPI layer which is required for the child process
	 */

	delete src->scsiIf();

	src->scsiIf(new ScsiIf(opts->sourceScsiDevice));
    
	if (src->scsiIf()->init() != 0) {
	    log_message(-2, "Re-init of SCSI interace failed.");

	    // indicate end of data
	    close(pipeFds[1]);

	    while (1)
		sleep(10);
	}    
#endif

	if (src->readDisk(opts->session, "-") != NULL)
	    log_message(1, "CD image reading finished successfully.");
	else
	    log_message(-2, "CD image reading failed.");

	// indicate end of data
	close(pipeFds[1]);
	while (1)
	    sleep(10);
    }

    close(pipeFds[1]);
    pipeFds[1] = -1;

    if ((oldStdin = dup(fileno(stdin))) < 0) {
	log_message(-2, "Cannot duplicate stdin: %s", strerror(errno));
	ret = 1;
	goto fail;
    }

    if (dup2(pipeFds[0], fileno(stdin)) != 0) {
	log_message(-2, "Cannot connect pipe to stdin: %s", strerror(errno));
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

    if (writeDiskAtOnce(toc, dst, opts->fifoBuffers, opts->swap, 0, 0) != 0) {
	if (dst->simulate())
	    log_message(-2, "Simulation failed.");
	else
	    log_message(-2, "Writing failed.");

	ret = 1;
    } else {
	if (dst->simulate())
	    log_message(1, "Simulation finished successfully.");
	else
	    log_message(1, "Writing finished successfully.");
    }

    dst->rezeroUnit(0);

    if (dst->preventMediumRemoval(0) != 0)
	ret = 1;

    if (ret == 0 && opts->eject) {
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
    const char *settingsPath = NULL;

#if defined(HAVE_SETEUID) && defined(HAVE_SETEGID)
    if (geteuid() == 0 && getuid() != 0) {
	uid_t uid = getuid();
	if (setuid(uid) == -1) {
	    log_message(-2, "Failed to drop privileges; exiting.");
	    exit(1);
        }
    }
#endif

    log_init();

    // Initialize command line options to default values
    DaoCommandLine options;
    setOptionDefaults(&options);
    options.progName = argv[0];

    Settings* settings = new Settings;

    settingsPath = "/etc/cdrdao.conf";
    if (settings->read(settingsPath) == 0)
	log_message(3, "Read settings from \"%s\".", settingsPath);

    settingsPath = "/etc/defaults/cdrdao";
    if (settings->read(settingsPath) == 0)
	log_message(3, "Read settings from \"%s\".", settingsPath);

    settingsPath = NULL;

    if ((homeDir = getenv("HOME")) != NULL) {
	settingsPath = strdup3CC(homeDir, "/.cdrdao", NULL);

	if (settings->read(settingsPath) == 0)
	    log_message(3, "Read settings from \"%s\".", settingsPath);
    } else {
	log_message(-1, "Environment variable 'HOME' not defined"
		    "- cannot read .cdrdao.");
    }

#ifdef UNIXWARE
    if (getuid() != 0) {
	log_message(-2, "You must be root to use cdrdao.");
	exit(1);
    }
#endif

    // Parse command line command and options.
    if (parseCmdline(argc - 1, argv + 1, &options, settings) != 0) {
	log_set_verbose(2);
	printVersion();
	printUsage(&options);
	exit(1);
    }

    log_set_verbose(options.verbose);
    commitSettings(&options, settings, settingsPath);

    printVersion();

    // Just show version ? We're done.
    if (options.command == SHOW_VERSION)
	goto fail;

    // ---------------------------------------------------------------------
    //   Parse and check the toc file
    // ---------------------------------------------------------------------
    if (cmdInfo[options.command].tocParse) {

	// Parse TOC file
	toc = Toc::read(options.tocFile);

	if (options.remoteMode) {
	    unlink(options.tocFile);
	}

	// Check and resolve input files paths
	if (!toc || !toc->resolveFilenames(options.tocFile)) {
	    exitCode = 1;
	    goto fail;
	}

	if (!toc->convertFilesToWav()) {
	    log_message(-2,
			"Could not decode audio files from toc file \"%s\".",
			options.tocFile);
	    exitCode = 1;
	    goto fail;
	}

	toc->recomputeLength();

	if (cmdInfo[options.command].tocCheck) {
	    if (checkToc(toc, options.force) != 0) {
		log_message(-2, "Toc file \"%s\" is inconsistent.",
			    options.tocFile);
		exitCode = 1;
		goto fail;
	    }
	}
    }

    // ---------------------------------------------------------------------
    //   Setup the CD device, obtain disk media information.
    // ---------------------------------------------------------------------

    if (cmdInfo[options.command].requiredDevice != NO_DEVICE) {

	if (!options.scsiDevice)
	    options.scsiDevice =
		getDefaultDevice(cmdInfo[options.command].requiredDevice);

	cdr = setupDevice(options.command,
			  options.scsiDevice,
			  options.driverId, 
			  /* init device? */
			  (options.command == UNLOCK) ? 0 : 1,
			  /* check for ready status? */
			  (options.command == BLANK ||
			   options.command == DRIVE_INFO ||
			   options.command == DISCID) ? 0 : 1,
			  /* reset status of medium if not empty? */
			  (options.command == SIMULATE ||
			   options.command == WRITE) ? 1 : 0,
			  options.readingSpeed,
			  options.remoteMode,
			  options.reload);

	if (cdr == NULL) {
	    log_message(-2, "Cannot setup device %s.", options.scsiDevice);
	    exitCode = 1; goto fail;
	}

	cdrScsi = cdr->scsiIf();

	if ((di = cdr->diskInfo()) == NULL) {
	    log_message(-2, "Cannot get disk information.");
	    exitCode = 1; goto fail;
	}
    }

    // ---------------------------------------------------------------------
    //   Process fullburn option for writing commands.
    // ---------------------------------------------------------------------
  
    if (options.command == SIMULATE ||
	options.command == WRITE ||
	options.command == COPY_CD) {
	if (options.fullBurn) {
	    if (options.driverId &&
		strcmp(options.driverId, "generic-mmc-raw") != 0) {
		log_message(-2, "You must use the generic-mmc-raw driver to use the "
			    "full-burn option.");
		exitCode = 1; goto fail;
	    } else {
		int mins = options.userCapacity ? options.userCapacity :
		    Msf(cdr->diskInfo()->capacity).min();
		log_message(2, "Burning entire %d mins disc.", mins);
	    }
	}
	cdr->fullBurn(options.fullBurn);
	cdr->userCapacity(options.userCapacity);
    }

    // ---------------------------------------------------------------------
    //   Setup secondary device for copy command.
    // ---------------------------------------------------------------------
  
    if (options.command == COPY_CD) {
	if (options.sourceScsiDevice != NULL && 
	    strcmp(options.scsiDevice, options.sourceScsiDevice) != 0) {
	    delSrcDevice = 1;
	    srcCdr = setupDevice(READ_CD, options.sourceScsiDevice,
				 options.sourceDriverId,
				 1, 1, 0, options.readingSpeed, false, false);

	    if (srcCdr == NULL) {
		log_message(-2, "Cannot setup source device %s.",
			    options.sourceScsiDevice);
		exitCode = 1; goto fail;
	    }

	    srcCdrScsi = srcCdr->scsiIf();

	    if ((srcDi = srcCdr->diskInfo()) == NULL) {
		log_message(-2,
			    "Cannot get disk information from source device.");
		exitCode = 1; goto fail;
	    }
	} else {
	    srcCdr = cdr;
	    srcDi = di;
	}
    }

    if (options.remoteMode)
	options.pause = false;

    // ---------------------------------------------------------------------
    //   Main command dispatch.
    // ---------------------------------------------------------------------

    switch (options.command) {
    case READ_CDDB:
	if ((exitCode = readCddb(&options, toc)) == 0) {
	    log_message(1, "Writing CD-TEXT populated toc-file \"%s\".",
			options.tocFile);
	    if (toc->write(options.tocFile) != 0)
		exitCode = 2;
	}
	break;

    case SCAN_BUS:
	scanBus();
	break;

    case DRIVE_INFO:
	showDriveInfo(cdr->driveInfo(true));
	break;

    case SHOW_TOC:
	showToc(toc);
	if (toc->check() > 1) {
	    log_message(-2, "Toc file \"%s\" is inconsistent.",
			options.tocFile);
	}
	break;

    case TOC_INFO:
	showTocInfo(toc, options.tocFile);
	if (toc->check() > 1) {
	    log_message(-2, "Toc file \"%s\" is inconsistent.",
			options.tocFile);
	}
	break;

    case TOC_SIZE:
	showTocSize(toc, options.tocFile);
	if (toc->check() > 1) {
	    log_message(-2, "Toc file \"%s\" is inconsistent.",
			options.tocFile);
	}
	break;

    case SHOW_DATA:
	showData(toc, options.swap);
	break;

    case READ_TEST:
	log_message(1, "Starting read test...");
	log_message(2, "Process can be aborted with QUIT signal "
		    "(usually CTRL-\\).");
	if (writeDiskAtOnce(toc, NULL, options.fifoBuffers,
			    options.swap, 1,
			    options.writingSpeed) != 0) {
	    log_message(-2, "Read test failed.");
	    exitCode = 1; goto fail;
	}
	break;

    case DISK_INFO:
	showDiskInfo(di);
	break;

    case DISCID:
	if (di->valid.empty && di->empty) {
	    log_message(-2, "Inserted disk is empty.");
	    exitCode = 1; goto fail;
	}
	cdr->subChanReadMode(options.readSubchanMode);
	cdr->rawDataReading(options.readRaw);
	cdr->mode2Mixed(options.mode2Mixed);
	cdr->fastTocReading(true);
	cdr->taoSource(options.taoSource);
	if (options.taoSourceAdjust >= 0)
	    cdr->taoSourceAdjust(options.taoSourceAdjust);

	cdr->force(options.force);

	if ((toc = cdr->readDiskToc(options.session,
				    (options.dataFilename == NULL) ?
				    "data.wav" : options.dataFilename))
	    == NULL) {
	    cdr->rezeroUnit(0);
	    exitCode = 1; goto fail;
	} else {
	    cdr->rezeroUnit(0);

	    if (options.printQuery)
		printCddbQuery(toc);
	    else
		readCddb(&options, toc, true);
	}
	break;
   
    case MSINFO:
	switch (showMultiSessionInfo(di)) {
	case 0:
	    log_message(2, "msinfo: Session is appendable");
	    exitCode = 0;
	    break;
      
	case 1: // CD-R is not empty and not appendable
	    log_message(2, "msinfo: CD-R is not empty and not appendable");
	    exitCode = 2;
	    break;
      
	case 2: // cannot determine state
	    log_message(2, "msinfo: cannot determine state");
	    exitCode = 3;
	    break;

	default: // everthing else is an error
	    log_message(2, "msinfo: command error");
	    exitCode = 1;
	    break;
	}
	break;


    case READ_TOC:
	if (di->valid.empty && di->empty) {
	    log_message(-2, "Inserted disk is empty.");
	    exitCode = 1; goto fail;
	}
	log_message(1, "Reading toc data...");

	if (access(options.tocFile, R_OK) == 0) {
	    log_message(-2, "File \"%s\" exists, will not overwrite.",
			options.tocFile);
	    exitCode = 1; goto fail;
	}

	cdr->subChanReadMode(options.readSubchanMode);
	cdr->rawDataReading(options.readRaw);
	cdr->mode2Mixed(options.mode2Mixed);
	cdr->fastTocReading(options.fastToc);
	cdr->taoSource(options.taoSource);
	if (options.taoSourceAdjust >= 0)
	    cdr->taoSourceAdjust(options.taoSourceAdjust);

	cdr->force(options.force);

	if ((toc =
	     cdr->readDiskToc(options.session,
			      (options.dataFilename == NULL) ?
			      "data.wav" : options.dataFilename)) == NULL) {
	    cdr->rezeroUnit(0);
	    exitCode = 1; goto fail;
	} else {
	    cdr->rezeroUnit(0);

	    if (options.withCddb) {
		if (readCddb(&options, toc) == 0) {
		    log_message(2, "CD-TEXT data was added to toc-file.");
		}
	    }

	    {
		std::ofstream out(options.tocFile);
		if (!out) {
		    log_message(-2, "Cannot open \"%s\" for writing: %s",
				options.tocFile,
				strerror(errno));
		    exitCode = 1; goto fail;
		}
		toc->print(out);
	    }

	    log_message(1, "Reading of toc data finished successfully.");
	}
	break;
    
    case READ_CD:
	if (di->valid.empty && di->empty) {
	    log_message(-2, "Inserted disk is empty.");
	    exitCode = 1; goto fail;
	}
	log_message(1, "Reading toc and track data...");

	if (access(options.tocFile, R_OK) == 0) {
	    log_message(-2, "File \"%s\" exists, will not overwrite.",
			options.tocFile);
	    exitCode = 1; goto fail;
	}

	cdr->subChanReadMode(options.readSubchanMode);
	cdr->rawDataReading(options.readRaw);
	cdr->mode2Mixed(options.mode2Mixed);
	cdr->taoSource(options.taoSource);
	if (options.taoSourceAdjust >= 0)
	    cdr->taoSourceAdjust(options.taoSourceAdjust);

	cdr->paranoiaMode(options.paranoiaMode);
	cdr->fastTocReading(options.fastToc);
	cdr->remote(options.remoteMode, options.remoteFd);
	cdr->force(options.force);

	toc = cdr->readDisk(options.session,
			    (options.dataFilename == NULL) ? "data.bin" :
			    options.dataFilename);
      
	if (toc == NULL) {
	    cdr->rezeroUnit(0);
	    exitCode = 1; goto fail;
	}
	cdr->rezeroUnit(0);

	if (options.withCddb) {
	    if (readCddb(&options, toc) == 0) {
		log_message(2, "CD-TEXT data was added to toc-file.");
	    }
	}

	{
	    std::ofstream out(options.tocFile);
	    if (!out) {
		log_message(-2, "Cannot open \"%s\" for writing: %s",
			    options.tocFile, strerror(errno));
		exitCode = 1; goto fail;
	    }
	    toc->print(out);
	}

	log_message(1, "Reading of toc and track data finished successfully.");
	break;

    case WRITE:
	if (!options.writeSimulate)
	    cdr->simulate(false);
	// fall through
    
    case SIMULATE:
	if (di->valid.empty && !di->empty && 
	    (!di->valid.append || !di->append)) {
	    log_message(-2, "Inserted disk is not empty and not appendable.");
	    exitCode = 1; goto fail;
	}

	if (toc->length().lba() > di->capacity) {
	    log_message((options.overburn ? -1 : -2),
			"Length of toc (%s, %ld blocks) exceeds capacity ",
			toc->length().str(), toc->length().lba());
	    log_message(0, "of CD-R (%s, %ld blocks).",
			Msf(di->capacity).str(),
			di->capacity);

	    if (options.overburn) {
		log_message(-1, "Ignored because of option '--overburn'.");
		log_message(-1, "Some drives may fail to record this toc.");
	    } else {
		log_message(-2, "Please use option '--overburn' to start"
			    "recording anyway.");
		exitCode = 1; goto fail;
	    }
	}

	if (options.multiSession) {
	    if (cdr->multiSession(1) != 0) {
		log_message(-2, "This driver does not support "
			    "multi session discs.");
		exitCode = 1; goto fail;
	    }
	}

	if (options.writingSpeed >= 0) {
	    if (cdr->speed(options.writingSpeed) != 0) {
		log_message(-2, "Writing speed %d not supported by device.",
			    options.writingSpeed);
		exitCode = 1; goto fail;
	    }
	}

	cdr->bufferUnderRunProtection(options.bufferUnderrunProtection);
	cdr->writeSpeedControl(options.writeSpeedControl);

	cdr->force(options.force);
	cdr->remote(options.remoteMode, options.remoteFd);

	switch (cdr->checkToc(toc)) {
	case 0: // OK
	    break;
	case 1: // warning
	    if (!options.force && !options.remoteMode) {
		log_message(-2, "Toc-file \"%s\" may create undefined "
			    "results.", options.tocFile);
		log_message(-2, "Use option --force to use it anyway.");
		exitCode = 1; goto fail;
	    }
	    break;
	default: // error
	    log_message(-2, "Toc-file \"%s\" is not suitable for this drive.",
			options.tocFile);
	    exitCode = 1; goto fail;
	    break;
	}

	log_message(1, "Starting write ");
	if (cdr->simulate()) {
	    log_message(1, "simulation ");
	}
	log_message(1, "at speed %d...", cdr->speed());
	if (cdr->multiSession() != 0) {
	    log_message(1, "Using multi session mode.");
	}

	if (options.pause) {
	    log_message(1, "Pausing 10 seconds - hit CTRL-C to abort.");
	    sleep(10);
	}

	log_message(2, "Process can be aborted with QUIT signal "
		    "(usually CTRL-\\).");
	if (cdr->preventMediumRemoval(1) != 0) {
	    exitCode = 1; goto fail;
	}

	if (writeDiskAtOnce(toc, cdr, options.fifoBuffers,
			    options.swap, 0, 0) != 0) {
	    if (cdr->simulate()) {
		log_message(-2, "Simulation failed.");
	    } else {
		log_message(-2, "Writing failed.");
	    }
	    cdr->preventMediumRemoval(0);
	    cdr->rezeroUnit(0);
	    exitCode = 1; goto fail;
	}

	if (cdr->simulate()) {
	    log_message(1, "Simulation finished successfully.");
	} else {
	    log_message(1, "Writing finished successfully.");
	}

	cdr->rezeroUnit(0);
	if (cdr->preventMediumRemoval(0) != 0) {
	    exitCode = 1; goto fail;
	}

	if (options.eject) {
	    cdr->loadUnload(1);
	}
	break;

    case COPY_CD:
	if (cdr != srcCdr) {
	    if (di->valid.empty && !di->empty && 
		(!di->valid.append || !di->append)) {
		log_message(-2, "Medium in recorder device is not empty"
			    "and not appendable.");
		exitCode = 1; goto fail;
	    }
	}

	if (srcDi->valid.empty && srcDi->empty) {
	    log_message(-2, "Medium in source device is empty.");
	    exitCode = 1; goto fail;
	}
    
	cdr->simulate(options.writeSimulate);
	cdr->force(options.force);
	cdr->remote(options.remoteMode, options.remoteFd);

	cdr->bufferUnderRunProtection(options.bufferUnderrunProtection);
	cdr->writeSpeedControl(options.writeSpeedControl);
    
	if (options.multiSession) {
	    if (cdr->multiSession(1) != 0) {
		log_message(-2, "This driver does not support multi"
			    "session discs.");
		exitCode = 1; goto fail;
	    }
	}

	if (options.writingSpeed >= 0) {
	    if (cdr->speed(options.writingSpeed) != 0) {
		log_message(-2, "Writing speed %d not supported by device.",
			    options.writingSpeed);
		exitCode = 1; goto fail;
	    }
	}

	srcCdr->paranoiaMode(options.paranoiaMode);
	srcCdr->subChanReadMode(options.readSubchanMode);
	srcCdr->fastTocReading(options.fastToc);
	srcCdr->force(options.force);
    
	if (options.onTheFly)
	    log_message(1, "Starting on-the-fly CD copy ");
	else
	    log_message(1, "Starting CD copy ");
	if (cdr->simulate()) {
	    log_message(1, "simulation ");
	}
	log_message(1, "at speed %d...", cdr->speed());
	if (cdr->multiSession() != 0) {
	    log_message(1, "Using multi session mode.");
	}

	if (options.onTheFly) {
	    if (srcCdr == cdr) {
		log_message(-2, "Two different device are required "
			    "for on-the-fly copying.");
		log_message(-2, "Please use option '--source-device x,y,z'.");
		exitCode = 1; goto fail;
	    }

	    if (copyCdOnTheFly(&options, srcCdr, cdr) == 0) {
		log_message(1, "On-the-fly CD copying finished successfully.");
	    } else {
		log_message(-2, "On-the-fly CD copying failed.");
		exitCode = 1; goto fail;
	    }

	} else {
	    if (srcCdr != cdr)
		srcCdr->remote(options.remoteMode, options.remoteFd);

	    if (copyCd(&options, srcCdr, cdr) == 0) {
		log_message(1, "CD copying finished successfully.");
	    } else {
		log_message(-2, "CD copying failed.");
		exitCode = 1; goto fail;
	    }
	}
	break;

    case BLANK:
	if (options.writingSpeed >= 0) {
	    if (cdr->speed(options.writingSpeed) != 0) {
		log_message(-2, "Blanking speed %d not supported by device.",
			    options.writingSpeed);
		exitCode = 1; goto fail;
	    }
	}

	cdr->remote(options.remoteMode, options.remoteFd);
	cdr->simulate(options.writeSimulate);

	log_message(1, "Blanking disk...");
	if (cdr->blankDisk(options.blankingMode) != 0) {
	    log_message(-2, "Blanking failed.");
	    exitCode = 1; goto fail;
	}

	if (options.eject)
	    cdr->loadUnload(1);
	break;

    case UNLOCK:
	log_message(1, "Trying to unlock drive...");

	cdr->abortDao();

	if (cdr->preventMediumRemoval(0) != 0) {
	    exitCode = 1; goto fail;
	}

	if (options.eject)
	    cdr->loadUnload(1);
	break;

    case UNKNOWN:
	assert(0);
	break;
    case SHOW_VERSION:
    case LAST_CMD:
	/* To avoid warning */
	break;
    }

 fail:
    delete cdr;
    if (delSrcDevice)
	delete srcCdr;
    delete cdrScsi;
    if (delSrcDevice)
	delete srcCdrScsi;

    delete toc;

#ifdef __CYGWIN__
    if (isNT) {
	DWORD bytes;
	if (fh)	{
	    if (!DeviceIoControl (fh, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL))
		log_message(-2, "Couldn't unlock device %s!", devstr);
	    else
		log_message(2, "Device %s unlocked.", devstr);
	    CloseHandle (fh);
	}
    }
#endif
    exit(exitCode);
}

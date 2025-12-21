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

#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "Cddb.h"
#include "CdrDriver.h"
#include "FormatConverter.h"
#include "ScsiIf.h"
#include "Settings.h"
#include "TempFileManager.h"
#include "Toc.h"
#include "dao.h"
#include "log.h"
#include "port.h"
#include "util.h"

#ifdef __CYGWIN__
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#define IOCTL_SCSI_BASE FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_GET_CAPABILITIES                                                                \
    CTL_CODE(IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT                                                             \
    CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_ADDRESS CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

using namespace std;

namespace
{

#ifdef UNIXWARE
extern "C" {
extern int seteuid(uid_t);
extern int setegid(uid_t);
};
#endif

PrintParams errPrintParams, filePrintParams;

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
    CDTEXT,
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

struct {
    // The command code, which is also the index into the array
    DaoCommand cmd;
    // The command-line string for this command
    const char *str;
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
    {SHOW_TOC, "show-toc", NO_DEVICE, 1, 1, 0},      {SHOW_DATA, "show-data", NO_DEVICE, 1, 1, 1},
    {READ_TEST, "read-test", NO_DEVICE, 1, 1, 1},    {SIMULATE, "simulate", NEED_CDR_W, 1, 1, 1},
    {WRITE, "write", NEED_CDR_W, 1, 1, 1},           {READ_TOC, "read-toc", NEED_CDR_R, 1, 0, 1},
    {DISK_INFO, "disk-info", NEED_CDR_R, 0, 0, 1},   {READ_CD, "read-cd", NEED_CDR_R, 1, 0, 1},
    {TOC_INFO, "toc-info", NO_DEVICE, 1, 1, 1},      {TOC_SIZE, "toc-size", NO_DEVICE, 1, 1, 1},
    {BLANK, "blank", NEED_CDRW_W, 0, 0, 1},          {SCAN_BUS, "scanbus", NO_DEVICE, 0, 0, 1},
    {UNLOCK, "unlock", NEED_CDR_R, 0, 0, 1},         {COPY_CD, "copy", NEED_CDR_W, 0, 0, 1},
    {READ_CDDB, "read-cddb", NO_DEVICE, 1, 1, 0},    {MSINFO, "msinfo", NEED_CDR_R, 0, 0, 1},
    {DRIVE_INFO, "drive-info", NEED_CDR_R, 0, 0, 1}, {DISCID, "discid", NEED_CDR_R, 0, 0, 1},
    {SHOW_VERSION, "version", NO_DEVICE, 0, 0, 0},   {CDTEXT, "cdtext", NEED_CDR_R, 0, 0, 0},
};

struct DaoCommandLine {
    DaoCommand command;

    const char *progName;
    string tocFile;
    string driverId;
    string sourceDriverId;
    string scsiDevice;
    const char *sourceScsiDevice;
    const char *dataFilename;
    string cddbLocalDbDir;
    string tmpFileDir;
    vector<string> cddbServerList;

    int readingSpeed;
    int writingSpeed;
    bool eject;
    bool swap;
    bool multiSession;
    int verbose;
    int session;
    int fifoBuffers;
    bool fastToc;
    bool pause;
    bool readRaw;
    bool mode2Mixed;
    bool remoteMode;
    int remoteFd;
    bool reload;
    bool force;
    int paranoiaMode;
    bool onTheFly;
    bool writeSimulate;
    bool saveSettings;
    int userCapacity;
    bool fullBurn;
    int cddbTimeout;
    bool withCddb;
    bool taoSource;
    int taoSourceAdjust;
    bool keepImage;
    bool overburn;
    int bufferUnderrunProtection;
    bool writeSpeedControl;
    bool keep;
    bool printQuery;
    bool no_utf8;

    CdrDriver::BlankingMode blankingMode;
    TrackData::SubChannelMode readSubchanMode;

    DaoCommandLine();

    void printUsage();
    void importSettings(Settings *settings);
    void exportSettings(Settings *settings);
    void commitSettings(Settings *settings, const char *settingsPath);
    int parseCmdLine(int argc, char **argv, Settings *settings);
};

DaoCommandLine::DaoCommandLine()
    : eject(false), swap(false), multiSession(false), fastToc(false), pause(false), readRaw(false),
      mode2Mixed(false), remoteMode(false), reload(false), force(false), onTheFly(false),
      writeSimulate(false), saveSettings(false), fullBurn(false), withCddb(false), taoSource(false),
      keepImage(false), overburn(false), writeSpeedControl(false), keep(false), printQuery(false),
      no_utf8(false)
{
    dataFilename = NULL;
    sourceScsiDevice = NULL;
    readingSpeed = -1;
    writingSpeed = -1;
    command = UNKNOWN;
    verbose = 2;
    session = 1;
    pause = true;
    mode2Mixed = true;
    remoteFd = -1;
    paranoiaMode = 3;
    cddbTimeout = 60;
    userCapacity = 0;
    taoSourceAdjust = -1;
    bufferUnderrunProtection = 1;
    writeSpeedControl = true;
    scsiDevice.clear();
#if defined(__FreeBSD__)
    fifoBuffers = 20;
#else
    fifoBuffers = 32;
#endif
    cddbServerList = {"gnudb.gnudb.org", "http://gnudb.gnudb.org:80/~cddb/cddb.cgi"};

    blankingMode = CdrDriver::BLANK_MINIMAL;
    readSubchanMode = TrackData::SUBCHAN_NONE;
}

bool isNT = false;

#ifdef __CYGWIN__
/*! \brief OS handle to the device
    As obtained from CreateFile, used to apply OS level locking.
*/
HANDLE fh = NULL;
/*! \brief Device string
    Like "\\\\.\\E:", used in CreateFile to obtain handle to device.
*/
char devstr[10];
#endif

void printVersion()
{
    log_message(2, "Cdrdao version %s - (C) Andreas Mueller <andreas@daneb.de>", VERSION);

    list<string> elist;
    int num = formatConverter.supportedExtensions(elist);

    if (num) {
        string msg = "Format converter enabled for extensions:";
        for (const auto &i : elist) {
            msg += " ";
            msg += i;
        }
        log_message(3, msg.c_str());
    }
}

void DaoCommandLine::printUsage()
{
    switch (command) {

    case UNKNOWN:
        log_message(0, "\nUsage: %s <command> [options] [toc-file]", progName);
        log_message(0, "command:\n"
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
                       "  cdtext     - shows CD CD-TEXT content\n"
                       "  unlock     - unlock drive after failed writing\n"
                       "  blank      - blank a CD-RW\n"
                       "  scanbus    - scan for devices\n"
                       "  simulate   - shortcut for 'write --simulate'\n"
                       "  write      - writes CD\n"
                       "  copy       - copies CD\n");

        log_message(0,
                    "\n Try '%s <command> -h' to get a list of available "
                    "options\n",
                    progName);
        break;

    case SHOW_TOC:
        log_message(0, "\nUsage: %s show-toc [options] toc-file", progName);
        log_message(0, "options:\n"
                       "  -v #                    - sets verbose level\n"
                       "  --no-utf8               - don't show CD-TEXT as UTF-8\n");
        break;

    case SHOW_DATA:
        log_message(0, "\nUsage: %s show-data [--swap] [-v #] toc-file\n", progName);
        break;

    case READ_TEST:
        log_message(0, "\nUsage: %s read-test [-v #] toc-file\n", progName);
        break;

    case CDTEXT:
        log_message(0, "\nUsage: %s cdtext [options]\n", progName);
        log_message(
            0, "options:\n"
               "  -v #                    - sets verbose level\n"
               "  --no-utf8               - don't show CD-TEXT as UTF-8\n"
               "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-ROM reader\n"
               "  --driver <id>           - force usage of specified driver for source device\n");
        break;

    case SIMULATE:
        log_message(0, "\nUsage: %s simulate [options] toc-file", progName);
        log_message(0,
                    "options:\n"
                    "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                    "  --driver <id>           - force usage of specified driver\n"
                    "  --speed <writing-speed> - selects writing speed\n"
                    "  --multi                 - session will not be closed\n"
                    "  --overburn              - allow one to overburn a medium\n"
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
        log_message(0, "\nUsage: %s write [options] toc-file", progName);
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
                    "  --overburn              - allow one to overburn a medium\n"
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
        log_message(0, "\nUsage: %s read-toc [options] toc-file", progName);
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
                    "  --no-utf8               - don't show CD-TEXT as UTF-8\n"
                    "  -v #                    - sets verbose level\n");
        break;

    case DISK_INFO:
        log_message(0, "\nUsage: %s disk-info [options]", progName);
        log_message(0, "options:\n"
                       "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                       "  --driver <id>           - force usage of specified driver\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case DISCID:
        log_message(0, "\nUsage: %s discid [options]", progName);
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
        log_message(0, "\nUsage: %s read-cd [options] toc-file", progName);
        log_message(0,
                    "options:\n"
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
                    "  --no-utf8               - don't show CD-TEXT as UTF-8\n"
                    "  --force                 - force execution of operation\n"
                    "  -v #                    - sets verbose level\n");
        break;

    case TOC_INFO:
        log_message(0, "\nUsage: %s toc-info [options] toc-file", progName);
        log_message(0, "options:\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case TOC_SIZE:
        log_message(0, "\nUsage: %s toc-size [options] toc-file", progName);
        log_message(0, "options:\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case BLANK:
        log_message(0, "\nUsage: %s blank [options]", progName);
        log_message(0, "options:\n"
                       "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                       "  --driver <id>           - force usage of specified driver\n"
                       "  --speed <writing-speed> - selects writing speed\n"
                       "  --blank-mode <mode>     - blank mode ('full', 'minimal')\n"
                       "  --eject                 - ejects cd after writing or simulation\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case SCAN_BUS:
        log_message(0, "\nUsage: %s scan-bus [-v #]\n", progName);
        break;

    case UNLOCK:
        log_message(0, "\nUsage: %s unlock [options]", progName);
        log_message(0, "options:\n"
                       "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                       "  --driver <id>           - force usage of specified driver\n"
                       "  --reload                - reload the disk if necessary for writing\n"
                       "  --eject                 - ejects cd after unlocking\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case DRIVE_INFO:
        log_message(0, "\nUsage: %s drive-info [options]", progName);
        log_message(0, "options:\n"
                       "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                       "  --driver <id>           - force usage of specified driver\n"
                       "  -v #                    - sets verbose level\n");
        break;

    case COPY_CD:
        log_message(0, "\nUsage: %s copy [options]", progName);
        log_message(
            0, "options:\n"
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
               "  --overburn              - allow one to overburn a medium\n"
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
        log_message(0, "\nUsage: %s read-cddb [options] toc-file", progName);
        log_message(0,
                    "options:\n"
                    "  --cddb-servers <list>   - sets space separated list of CDDB servers\n"
                    "  --cddb-timeout #        - timeout in seconds for CDDB server communication\n"
                    "  --cddb-directory <path> - path to local CDDB directory where fetched\n"
                    "                            CDDB records will be stored\n"
                    "  -v #                    - sets verbose level\n");
        break;

    case MSINFO:
        log_message(0, "\nUsage: %s msinfo [options]", progName);
        log_message(0, "options:\n"
                       "  --device [proto:]{<x,y,z>|device} - sets SCSI device of CD-writer\n"
                       "  --driver <id>           - force usage of specified driver\n"
                       "  --reload                - reload the disk if necessary for writing\n"
                       "  --no-utf8               - don't show CD-TEXT as UTF-8\n"
                       "  -v #                    - sets verbose level\n");
        break;

    default:
        log_message(0, "Sorry, no help available for command %d :-(\n", command);
        break;
    }
}

void DaoCommandLine::importSettings(Settings *settings)
{
    const char *sval;
    const int *ival;

    DaoCommand cmd = command;

    if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
        if ((sval = settings->getString(Settings::setWriteDriver)) != NULL) {
            driverId = sval;
        }

        if ((sval = settings->getString(Settings::setWriteDevice)) != NULL) {
            scsiDevice = sval;
        }

        if ((ival = settings->getInteger(Settings::setWriteSpeed)) != NULL && *ival >= 0) {
            writingSpeed = *ival;
        }

        if ((ival = settings->getInteger(Settings::setWriteBuffers)) != NULL && *ival >= 10) {
            fifoBuffers = *ival;
        }
        if ((ival = settings->getInteger(Settings::setUserCapacity)) != NULL && *ival >= 0) {
            userCapacity = *ival;
        }
        if ((ival = settings->getInteger(Settings::setFullBurn)) != NULL && *ival >= 0) {
            fullBurn = *ival;
        }
    }

    if (cmd == READ_CD || cmd == READ_TOC) {
        if ((sval = settings->getString(Settings::setReadDriver)) != NULL) {
            driverId = sval;
        }

        if ((sval = settings->getString(Settings::setReadDevice)) != NULL) {
            scsiDevice = sval;
        }

        if ((ival = settings->getInteger(Settings::setReadParanoiaMode)) != NULL && *ival >= 0) {
            paranoiaMode = *ival;
        }
    }

    if (cmd == COPY_CD) {
        if ((sval = settings->getString(Settings::setReadDriver)) != NULL) {
            sourceDriverId = sval;
        }

        if ((sval = settings->getString(Settings::setReadDevice)) != NULL) {
            sourceScsiDevice = strdupCC(sval);
        }

        if ((ival = settings->getInteger(Settings::setReadParanoiaMode)) != NULL && *ival >= 0) {
            paranoiaMode = *ival;
        }
    }

    if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK || cmd == DISCID ||
        cmd == DRIVE_INFO) {
        if ((sval = settings->getString(Settings::setWriteDriver)) != NULL) {
            driverId = sval;
        }

        if ((sval = settings->getString(Settings::setWriteDevice)) != NULL) {
            scsiDevice = sval;
        }
    }

    if (cmd == READ_CDDB || cmd == COPY_CD || cmd == READ_TOC || cmd == READ_CD || cmd == DISCID) {
        vector<string> sl;
        settings->getStrings(Settings::setCddbServerList, sl);
        if (sl.size())
            cddbServerList = sl;

        if ((sval = settings->getString(Settings::setCddbDbDir)) != NULL) {
            cddbLocalDbDir = sval;
        }

        if ((ival = settings->getInteger(Settings::setCddbTimeout)) != NULL && *ival > 0) {
            cddbTimeout = *ival;
        }
        if ((sval = settings->getString(Settings::setTmpFileDir)) != NULL) {
            tmpFileDir = sval;
        }
    }

    if ((ival = settings->getInteger(Settings::setReadSpeed)) != NULL && *ival >= 0) {
        readingSpeed = *ival;
    }
}

void DaoCommandLine::exportSettings(Settings *settings)
{
    DaoCommand cmd = command;

    if (cmd == SIMULATE || cmd == WRITE || cmd == COPY_CD) {
        if (!driverId.empty())
            settings->set(Settings::setWriteDriver, driverId.c_str());

        if (!scsiDevice.empty())
            settings->set(Settings::setWriteDevice, scsiDevice.c_str());

        if (writingSpeed >= 0) {
            settings->set(Settings::setWriteSpeed, writingSpeed);
        }

        if (fifoBuffers > 0) {
            settings->set(Settings::setWriteBuffers, fifoBuffers);
        }

        if (fullBurn > 0) {
            settings->set(Settings::setFullBurn, fullBurn);
        }

        if (userCapacity > 0) {
            settings->set(Settings::setUserCapacity, userCapacity);
        }
    }

    if (cmd == READ_CD) {
        if (!driverId.empty())
            settings->set(Settings::setReadDriver, driverId.c_str());

        if (!scsiDevice.empty())
            settings->set(Settings::setReadDevice, scsiDevice.c_str());

        settings->set(Settings::setReadParanoiaMode, paranoiaMode);
    }

    if (cmd == COPY_CD) {
        if (!sourceDriverId.empty())
            settings->set(Settings::setReadDriver, sourceDriverId.c_str());

        if (sourceScsiDevice != NULL)
            settings->set(Settings::setReadDevice, sourceScsiDevice);

        settings->set(Settings::setReadParanoiaMode, paranoiaMode);
    }

    if (cmd == BLANK || cmd == DISK_INFO || cmd == MSINFO || cmd == UNLOCK || cmd == DISCID ||
        cmd == DRIVE_INFO) {
        if (!driverId.empty())
            settings->set(Settings::setWriteDriver, driverId.c_str());

        if (!scsiDevice.empty())
            settings->set(Settings::setWriteDevice, scsiDevice.c_str());
    }

    if (cmd == READ_CDDB ||
        (withCddb && (cmd == COPY_CD || cmd == READ_TOC || cmd == READ_CD || cmd == DISCID))) {
        if (cddbServerList.size()) {
            settings->set(Settings::setCddbServerList, cddbServerList);
        }

        if (!cddbLocalDbDir.empty()) {
            settings->set(Settings::setCddbDbDir, cddbLocalDbDir.c_str());
        }

        if (cddbTimeout > 0) {
            settings->set(Settings::setCddbTimeout, cddbTimeout);
        }
    }

    if (readingSpeed >= 0) {
        settings->set(Settings::setReadSpeed, readingSpeed);
    }

    if (cmd == SHOW_TOC || cmd == SIMULATE || cmd == WRITE || cmd == TOC_INFO || cmd == TOC_SIZE) {
        if (tmpFileDir.size())
            settings->set(Settings::setTmpFileDir, tmpFileDir.c_str());
    }
}

int DaoCommandLine::parseCmdLine(int argc, char **argv, Settings *settings)
{
    int i;
    bool clear_default_servers = true;

    if (argc < 1) {
        return 1;
    }

    for (i = 0; i < LAST_CMD; i++) {
        if (strcmp(*argv, cmdInfo[i].str) == 0) {
            command = cmdInfo[i].cmd;
            break;
        }
    }

    if (command == UNKNOWN) {
        log_message(-2, "Illegal command: %s", *argv);
        return 1;
    }

    // retrieve settings from $HOME/.cdrdao for given command
    importSettings(settings);

    argc--, argv++;

    while (argc > 0 && (*argv)[0] == '-') {

        if ((*argv)[1] != '-') {
            switch ((*argv)[1]) {
            case 'h':
                return 1;

            case 'v':
                if ((*argv)[2] != 0) {
                    verbose = atoi((*argv) + 2);
                } else {
                    if (argc < 2) {
                        log_message(-2, "Missing argument after: %s", *argv);
                        return 1;
                    } else {
                        verbose = atoi(argv[1]);
                        argc--, argv++;
                    }
                }
                break;

            case 'n':
                pause = false;
                break;

            default:
                log_message(-2, "Illegal option: %s", *argv);
                return 1;
                break;
            }
        } else {

            if (strcmp((*argv) + 2, "help") == 0) {
                return 1;
            }
            if (strcmp((*argv) + 2, "device") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    scsiDevice = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "source-device") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    sourceScsiDevice = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "rspeed") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    readingSpeed = atol(argv[1]);
                    if (readingSpeed < 0) {
                        log_message(-2, "Illegal reading speed: %s", argv[1]);
                        return 1;
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "speed") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    writingSpeed = atol(argv[1]);
                    if (writingSpeed < 0) {
                        log_message(-2, "Illegal writing speed: %s", argv[1]);
                        return 1;
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "capacity") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    userCapacity = atol(argv[1]);
                    if (userCapacity < 0) {
                        log_message(-2, "Illegal disk capacity: %s minutes", argv[1]);
                        return 1;
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "blank-mode") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    if (strcmp(argv[1], "full") == 0) {
                        blankingMode = CdrDriver::BLANK_FULL;
                    } else if (strcmp(argv[1], "minimal") == 0) {
                        blankingMode = CdrDriver::BLANK_MINIMAL;
                    } else {
                        log_message(-2, "Illegal blank mode. Valid values: full minimal");
                        return 1;
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "paranoia-mode") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    paranoiaMode = atol(argv[1]);
                    if (paranoiaMode < 0) {
                        log_message(-2, "Illegal paranoia mode: %s", argv[1]);
                        return 1;
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "remote") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    remoteFd = atol(argv[1]);
                    if (remoteFd < 0) {
                        log_message(-2, "Invalid remote message file descriptor: %s", argv[1]);
                        return 1;
                    }
                    remoteMode = true;
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "eject") == 0) {
                eject = true;
            } else if (strcmp((*argv) + 2, "swap") == 0) {
                swap = true;
            } else if (strcmp((*argv) + 2, "query-string") == 0) {
                printQuery = true;
            } else if (strcmp((*argv) + 2, "multi") == 0) {
                multiSession = true;
            } else if (strcmp((*argv) + 2, "simulate") == 0) {
                writeSimulate = true;
            } else if (strcmp((*argv) + 2, "fast-toc") == 0) {
                fastToc = true;
            } else if (strcmp((*argv) + 2, "read-raw") == 0) {
                readRaw = true;
            } else if (strcmp((*argv) + 2, "no-mode2-mixed") == 0) {
                mode2Mixed = false;
            } else if (strcmp((*argv) + 2, "reload") == 0) {
                reload = true;
            } else if (strcmp((*argv) + 2, "no-utf8") == 0) {
                no_utf8 = true;
            } else if (strcmp((*argv) + 2, "force") == 0) {
                force = true;
            } else if (strcmp((*argv) + 2, "keep") == 0) {
                keep = true;
            } else if (strcmp((*argv) + 2, "on-the-fly") == 0) {
                onTheFly = true;
            } else if (strcmp((*argv) + 2, "save") == 0) {
                saveSettings = true;
            } else if (strcmp((*argv) + 2, "tao-source") == 0) {
                taoSource = true;
            } else if (strcmp((*argv) + 2, "keepimage") == 0) {
                keepImage = true;
            } else if (strcmp((*argv) + 2, "overburn") == 0) {
                overburn = true;
            } else if (strcmp((*argv) + 2, "full-burn") == 0) {
                fullBurn = true;
            } else if (strcmp((*argv) + 2, "with-cddb") == 0) {
                withCddb = true;
            } else if (strcmp((*argv) + 2, "driver") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    driverId = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "source-driver") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    sourceDriverId = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "datafile") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    dataFilename = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "buffers") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    fifoBuffers = atoi(argv[1]);
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "session") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    session = atoi(argv[1]);
                    argc--, argv++;
                    if (session < 1) {
                        log_message(-2, "Illegal session number: %d", session);
                        return 1;
                    }
                }
            } else if (strcmp((*argv) + 2, "cddb-servers") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    if (clear_default_servers) {
                        clear_default_servers = false;
                        cddbServerList.clear();
                    }
                    {
                        string parsed;
                        stringstream iss(argv[1]);
                        while (getline(iss, parsed, ' '))
                            cddbServerList.push_back(parsed);
                    }
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "cddb-directory") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    cddbLocalDbDir = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "tmpdir") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    tmpFileDir = argv[1];
                    argc--, argv++;
                }
            } else if (strcmp((*argv) + 2, "cddb-timeout") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    cddbTimeout = atoi(argv[1]);
                    argc--, argv++;
                    if (cddbTimeout < 1) {
                        log_message(-2, "Illegal CDDB timeout: %d", cddbTimeout);
                        return 1;
                    }
                }
            } else if (strcmp((*argv) + 2, "tao-source-adjust") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    taoSourceAdjust = atoi(argv[1]);
                    argc--, argv++;
                    if (taoSourceAdjust < 0 || taoSourceAdjust >= 100) {
                        log_message(-2, "Illegal number of TAO link blocks: %d", taoSourceAdjust);
                        return 1;
                    }
                }
            } else if (strcmp((*argv) + 2, "buffer-under-run-protection") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    int val = atoi(argv[1]);
                    argc--, argv++;
                    if (val < 0 || val > 1) {
                        log_message(
                            -2, "Illegal value for option --buffer-under-run-protection: %d", val);
                        return 1;
                    }
                    bufferUnderrunProtection = val;
                }
            } else if (strcmp((*argv) + 2, "write-speed-control") == 0) {
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
                    writeSpeedControl = val;
                }
            } else if (strcmp((*argv) + 2, "read-subchan") == 0) {
                if (argc < 2) {
                    log_message(-2, "Missing argument after: %s", *argv);
                    return 1;
                } else {
                    if (strcmp(argv[1], "rw") == 0) {
                        readSubchanMode = TrackData::SUBCHAN_RW;
                    } else if (strcmp(argv[1], "rw_raw") == 0) {
                        readSubchanMode = TrackData::SUBCHAN_RW_RAW;
                    } else {
                        log_message(-2, "Invalid argument after %s: %s", argv[0], argv[1]);
                        return 1;
                    }

                    argc--, argv++;
                }
            } else {
                log_message(-2, "Illegal option: %s", *argv);
                return 1;
            }
        }

        argc--, argv++;
    }

    if (cmdInfo[command].needTocFile) {
        if (argc < 1) {
            log_message(-2, "Missing toc-file.");
            return 1;
        } else if (argc > 1) {
            log_message(-2, "Expecting only one toc-file.");
            return 1;
        }
        tocFile = *argv;
    }

    return 0;
}

// Commit settings to overall system. Export them.
void DaoCommandLine::commitSettings(Settings *settings, const char *settingsPath)
{
    if (tmpFileDir.size())
        tempFileManager.setTempDirectory(tmpFileDir.c_str());

    tempFileManager.setKeepTemps(keep);

    if (saveSettings && settingsPath != NULL) {
        // If we're saving our settings, give up root privileges and
        // exit. The --save option is only compiled in if setreuid() is
        // available (because it could be used for a local root exploit).
        if (giveUpRootPrivileges()) {
            exportSettings(settings);
            settings->write(settingsPath);
        }
        exit(0);
    }
}

// Selects driver for device of 'scsiIf'.
CdrDriver *selectDriver(DaoCommand cmd, ScsiIf *scsiIf, const string &driverId)
{
    unsigned long options = 0;
    CdrDriver *ret = NULL;

    if (!driverId.empty()) {
        char *s = strdupCC(driverId.c_str());
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
    } else {
        const char *id = NULL;

        // for reading commands try to select a special read driver first:
        if (cmd == READ_TOC || cmd == READ_CD)
            id = CdrDriver::selectDriver(0, scsiIf->vendor(), scsiIf->product(), &options);

        // try to select a write driver
        if (id == NULL)
            id = CdrDriver::selectDriver(1, scsiIf->vendor(), scsiIf->product(), &options);
        // if no driver is selected, yet, try to select a read driver for
        // disk-info
        if (id == NULL && (cmd == DISK_INFO || cmd == MSINFO || cmd == DISCID))
            id = CdrDriver::selectDriver(0, scsiIf->vendor(), scsiIf->product(), &options);
        // Still no driver, try to autodetect one
        if (id == NULL)
            id = CdrDriver::detectDriver(scsiIf, &options);

        if (id != NULL) {
            ret = CdrDriver::createDriver(id, options, scsiIf);
        } else {
            log_message(0, "");
            log_message(-2, "No driver found for '%s %s', available drivers:\n", scsiIf->vendor(),
                        scsiIf->product());
            CdrDriver::printDriverIds();

            log_message(0, "\nFor all recent recorder models either the 'generic-mmc' or");
            log_message(0, "the 'generic-mmc-raw' driver should work.");
            log_message(
                0, "Use option '--driver' to force usage of a driver, e.g.: --driver generic-mmc");
        }
    }

    return ret;
}

string getDefaultDevice(DaoDeviceType req)
{
    int i, len;
    string buf;

    // This function should not be called if the command issues
    // doesn't actually require a device.
    assert(req != NO_DEVICE);

    ScsiIf::ScanData *sdata = ScsiIf::scan(&len);

    if (sdata) {
        for (i = 0; i < len; i++) {

            ScsiIf testif(sdata[i].dev.c_str());

            if (testif.init() != 0) {
                continue;
            }
            cd_page_2a *p2a;

            if (!(p2a = testif.checkMmc()))
                continue;

            if (req == NEED_CDR_R && !p2a->cd_r_read)
                continue;
            if (req == NEED_CDR_W && !p2a->cd_r_write)
                continue;
            if (req == NEED_CDRW_W && !p2a->cd_rw_write)
                continue;

            buf = sdata[i].dev;
            delete[] sdata;
            return buf;
        }
        delete[] sdata;
    }

    return buf;
}

#define MAX_RETRY 10
CdrDriver *setupDevice(DaoCommand cmd, const string &scsiDevice, const string &driverId,
                       int initDevice, int checkReady, int checkEmpty, int readingSpeed,
                       bool remote, bool reload)
{
    ScsiIf *scsiIf = NULL;
    CdrDriver *cdr = NULL;
    DiskInfo *di = NULL;
    int inquiryFailed = 0;
    int retry = 0;
    int initTries = 2;
    int ret = 0;

    scsiIf = new ScsiIf(scsiDevice.c_str());

    switch (scsiIf->init()) {
    case 1:
        log_message(-2, "Please use option '--device device'"
                        ", e.g. --device /dev/cdrom");
        delete scsiIf;
        return NULL;
        break;
    case 2:
        inquiryFailed = 1;
        break;
    }

    log_message(2, "%s: %s %s\tRev: %s", scsiDevice.c_str(), scsiIf->vendor(), scsiIf->product(),
                scsiIf->revision());

    if (inquiryFailed && driverId.empty()) {
        log_message(-2, "Inquiry failed and no driver id is specified.");
        log_message(-2, "Please use option --driver to specify a driver id.");
        delete scsiIf;
        return NULL;
    }

    if ((cdr = selectDriver(cmd, scsiIf, driverId)) == NULL) {
        delete scsiIf;
        return NULL;
    }

    log_message(2, "Using driver: %s (options 0x%04lx)\n", cdr->driverName(), cdr->options());

    if (!initDevice)
        return cdr;

    while (initTries > 0) {
        // clear unit attention
        cdr->rezeroUnit(0);
        if (readingSpeed >= 0) {
            if (!cdr->rspeed(readingSpeed)) {
                log_message(-2, "Reading speed %d is not supported by device.", readingSpeed);
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
                log_message(0, "Setting reading speed %d.", readingSpeed);
                if (!cdr->rspeed(readingSpeed)) {
                    log_message(-2, "Reading speed %d is not supported by device.", readingSpeed);
                    exit(1);
                }
            }

            if ((di = cdr->diskInfo()) == NULL) {
                log_message(-2, "Cannot get disk information.");
                delete cdr;
                delete scsiIf;
                return NULL;
            }

            if (checkEmpty && initTries == 2 && di->valid.empty && !di->empty &&
                (!di->valid.append || !di->append) && (!remote || reload)) {
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
            } else {
                initTries = 0;
            }
        } else {
            initTries = 0;
        }
    }

    if (readingSpeed >= 0) {
        if (!cdr->rspeed(readingSpeed)) {
            log_message(-2, "Reading speed %d is not supported by device.", readingSpeed);
            exit(1);
        }
    }

    return cdr;
}

void showDriveInfo(const DriveInfo *i)
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

void showTocInfo(const Toc *toc, const string &tocFile)
{
    long len = toc->length().lba() * AUDIO_BLOCK_LEN;
    len >>= 20;

    printf("%s: %d tracks, length %s, %ld blocks, %ld MB\n", tocFile.c_str(), toc->nofTracks(),
           toc->length().str(), toc->length().lba(), len);
}

void showTocSize(const Toc *toc, const string &tocFile)
{
    printf("%ld\n", toc->length().lba());
}

void showToc(Toc *toc, DaoCommandLine &options)
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

    if (options.verbose > 2) {
        for (const auto &item : toc->globalCdTextItems()) {
            PrintParams params;
            params.no_utf8 = options.no_utf8;
            cout << "          ";
            item.print(cout, params);
            cout << "\n";
        }
    }

    TrackIterator itr(toc);

    for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
        if (tcount > 1)
            printf("\n");

        printf("TRACK %2d  Mode %s", tcount, TrackData::mode2String(t->type()));

        if (t->subChannelType() != TrackData::SUBCHAN_NONE)
            printf(" %s", TrackData::subChannelMode2String(t->subChannelType()));

        printf(":\n");

        if (t->type() == TrackData::AUDIO) {
            if (t->isrcValid()) {
                printf("          ISRC %c%c %c%c%c %c%c %c%c%c%c%c\n", t->isrcCountry(0),
                       t->isrcCountry(1), t->isrcOwner(0), t->isrcOwner(1), t->isrcOwner(2),
                       t->isrcYear(0) + '0', t->isrcYear(1) + '0', t->isrcSerial(0) + '0',
                       t->isrcSerial(1) + '0', t->isrcSerial(2) + '0', t->isrcSerial(3) + '0',
                       t->isrcSerial(4) + '0');
            }
        }
        printf("          COPY%sPERMITTED\n", t->copyPermitted() ? " " : " NOT ");

        if (t->type() == TrackData::AUDIO) {
            printf("          %sPRE-EMPHASIS\n", t->preEmphasis() ? "" : "NO ");
            printf("          %s CHANNEL AUDIO\n", t->audioType() == 0 ? "TWO" : "FOUR");
        }

        if (t->start().lba() != 0) {
            printf("          PREGAP %s(%6ld)\n", t->start().str(), t->start().lba());
        }
        printf("          START  %s(%6ld)\n", start.str(), start.lba());
        n = t->nofIndices();
        for (i = 0; i < n; i++) {
            index = start + t->getIndex(i);
            printf("          INDEX %2d %s(%6ld)\n", i + 2, index.str(), index.lba());
        }

        printf("          END%c   %s(%6ld)\n", t->isPadded() ? '*' : ' ', end.str(), end.lba());

        if (options.verbose > 2) {
            for (const auto &item : t->getCdTextItems()) {
                PrintParams params;
                params.no_utf8 = options.no_utf8;
                cout << "          ";
                item.print(cout, params);
                cout << "\n";
            }
        }

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

void showCDText(CdrDriver *cdr, DaoCommandLine &options)
{
    int nofTracks = 0;
    auto cdt = cdr->getTocGeneric(&nofTracks);

    if (!cdt || nofTracks <= 1)
        return;
    nofTracks--; // remove leadout

    // Create a fake Toc placeholder.
    auto toc = new Toc();
    for (int i = 0; i < nofTracks; i++)
        toc->append(new Track(TrackData::AUDIO, TrackData::SUBCHAN_RW));

    if (cdr->readCdTextData(toc) != 0) {
        log_message(-2, "Cannot read CD-TEXT data.");
        return;
    }

    for (const auto &item : toc->globalCdTextItems()) {
        PrintParams params;
        params.no_utf8 = options.no_utf8;
        item.print(cout, params);
        cout << "\n";
    }

    // for (const auto i : items) {
    //     if (i->packType() == CdTextItem::PackType::SIZE_INFO)
    //         continue;
    //     if (i->blockNr() != n_block) {
    //         n_block = i->blockNr();
    //         if (n_block > 0) cout << "\n";
    //         cout << "Block " << n_block << ":\n";
    //         if (languages && n_block < 8 && languages[n_block] != 0) {
    //             cout << "    LANGUAGE \""
    //                  << CdTextContainer::languageName(languages[n_block])
    //                  << "\"\n";
    //         }
    //     }
    //     if (i->trackNr() >= 1 && i->trackNr() != n_track) {
    //         n_track = i->trackNr();
    //         cout << "  Track " << n_track << ":\n";
    //     }
    //     cout << "    ";
    //     if (i->dataType() == CdTextItem::DataType::SBCC) {
    //         cout << CdTextItem::packType2String(i->trackNr() > 0, i->packType())
    //              << " \"" << Util::to_utf8((u8*)i->data(), (size_t)i->dataLen(),
    //                                        encodings[i->blockNr()]) << "\"";
    //     } else {
    //         PrintParams p;
    //         p.to_utf8 = true;
    //         i->print(cout, p);
    //     }
    //     cout << "\n";
    // }
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
        printf("%s (%ld blocks, %ld/%ld MB)", Msf(di->capacity).str(), di->capacity,
               (di->capacity * 2) >> 10, (di->capacity * AUDIO_BLOCK_LEN) >> 20);
    else
        printf("n/a");

    printf("\n");

    printf("CD-R medium          : ");
    if (di->valid.manufacturerId) {
        if (CdrDriver::cdrVendor(di->manufacturerId, &s1, &s2)) {
            printf("%s\n", s1);
            printf("                       %s\n", s2);
        } else {
            printf("%s: unknown vendor ID\n", di->manufacturerId.str());
        }
    } else {
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

                printf("Remaining Capacity   : %s (%ld blocks, %ld/%ld MB)\n", Msf(r).str(), r,
                       (r * 2) >> 10, (r * AUDIO_BLOCK_LEN) >> 20);
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
int showMultiSessionInfo(DiskInfo *di)
{

    if (di->valid.empty) {
        if (di->empty) {
            // print nothing  to indicate empty disk
            return 0;
        } else if (di->valid.append) {
            if (di->append) {
                printf("%ld,%ld\n", di->lastSessionLba, di->thisSessionLba);
                return 0;
            } else {
                return 1;
            }
        }
    }

    return 2;
}

void printCddbQuery(Toc *toc)
{
    Cddb cddb(toc);

    cddb.printDbQuery();
}

int readCddb(const DaoCommandLine &opts, Toc *toc, bool showEntry = false)
{
    int err = 0;
    char *p;
    const char *sep = " ,";
    char *user = NULL;
    char *host = NULL;
    struct passwd *pwent;
    Cddb::QueryResults *qres, *qrun, *qsel;
    Cddb::CddbEntry *dbEntry;

    Cddb cddb(toc);

    cddb.timeout(opts.cddbTimeout);

    if (!opts.cddbLocalDbDir.empty())
        cddb.localCddbDirectory(opts.cddbLocalDbDir);

    for (const auto &p : opts.cddbServerList)
        cddb.appendServer(p.c_str());

    if ((pwent = getpwuid(getuid())) != NULL && pwent->pw_name != NULL) {
        user = strdupCC(pwent->pw_name);
    } else {
        user = strdupCC("unknown");
    }

    {
        struct utsname sinfo;
        if (uname(&sinfo) == 0) {
            host = strdupCC(sinfo.nodename);
        } else {
            host = strdupCC("unknown");
        }
    }

    if (cddb.connectDb(user, host, "cdrdao", VERSION) != 0) {
        log_message(-2, "Cannot connect to any CDDB server.");
        err = 2;
        goto fail;
    }

    if (cddb.queryDb(&qres) != 0) {
        log_message(-2, "Querying of CDDB server failed.");
        err = 2;
        goto fail;
    }

    if (qres == NULL) {
        log_message(1, "No CDDB record found for this toc-file.");
        err = 1;
        goto fail;
    }

    if (qres->next != NULL || !(qres->exactMatch)) {
        int qcount;

        if (qres->next == NULL)
            log_message(0, "Found following inexact match:");
        else
            log_message(0, "Found following inexact matches:");

        log_message(0, "\n    DISKID   CATEGORY     TITLE\n");

        for (qrun = qres, qcount = 0; qrun != NULL; qrun = qrun->next, qcount++) {
            log_message(0, "%2d. %-8s %-12s %s", qcount + 1, qrun->diskId, qrun->category,
                        qrun->title);
        }

        log_message(0, "\n");

        qsel = NULL;

        while (1) {
            char buf[20];
            int sel;

            log_message(0, "Select match, 0 for none [0-%d]?", qcount);

            if (fgets(buf, 20, stdin) == NULL)
                break;

            for (p = buf; *p != 0 && isspace(*p); p++)
                ;

            if (*p != 0 && isdigit(*p)) {
                sel = atoi(p);

                if (sel == 0) {
                    break;
                } else if (sel > 0 && sel <= qcount) {
                    sel--;
                    for (qsel = qres, qcount = 0; qsel != NULL && qcount != sel;
                         qsel = qsel->next, qcount++)
                        ;

                    break;
                }
            }
        }

        if (qsel == NULL) {
            log_message(0, "No match selected.");
            err = 1;
            goto fail;
        }
    } else {
        qsel = qres;
    }

    log_message(1, "Reading CDDB record for: %s-%s-%s", qsel->diskId, qsel->category, qsel->title);

    if (cddb.readDb(qsel->category, qsel->diskId, &dbEntry) != 0) {
        log_message(-2, "Reading of CDDB record failed.");
        err = 2;
        goto fail;
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

void scanBus()
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

int checkToc(const Toc *toc, bool force)
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

int copyCd(DaoCommandLine &opts, CdrDriver *src, CdrDriver *dst)
{
    char dataFilenameBuf[50];
    long pid = getpid();
    Toc *toc;
    int ret = 0;
    DiskInfo *di = NULL;
    int isAppendable = 0;

    if (opts.dataFilename == NULL) {
        // create a unique temporary data file name in current directory
        snprintf(dataFilenameBuf, sizeof(dataFilenameBuf), "cddata%ld.bin", pid);
        opts.dataFilename = dataFilenameBuf;
    }

    src->rawDataReading(true);
    src->taoSource(opts.taoSource);
    if (opts.taoSourceAdjust >= 0)
        src->taoSourceAdjust(opts.taoSourceAdjust);

    if ((toc = src->readDisk(opts.session, opts.dataFilename)) == NULL) {
        unlink(opts.dataFilename);
        log_message(-2, "Creation of source CD image failed.");
        return 1;
    }

    if (opts.withCddb) {
        if (readCddb(opts, toc) == 0)
            log_message(2, "CD-TEXT data was added to toc-file.");
        else
            log_message(2, "Reading of CD-TEXT data failed - "
                           "continuing without CD-TEXT data.");
    }

    if (opts.keepImage) {
        string tocFilename;

        tocFilename = "cd";
        tocFilename += to_string(pid);
        tocFilename += ".toc";

        log_message(2, "Keeping created image file \"%s\".", opts.dataFilename);
        log_message(2, "Corresponding toc-file is written to \"%s\".", tocFilename.c_str());

        toc->write(tocFilename);
    }

    if (checkToc(toc, opts.force)) {
        log_message(-3, "Toc created from source CD image is inconsistent.");
        toc->print(cout, errPrintParams);
        delete toc;
        return 1;
    }

    switch (dst->checkToc(toc)) {
    case 0: // OK
        break;
    case 1: // warning
        if (!opts.force) {
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
        src->loadUnload(1);
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
        if (!opts.keepImage) {
            if (unlink(opts.dataFilename) != 0)
                log_message(-2, "Cannot remove CD image file \"%s\": %s", opts.dataFilename,
                            strerror(errno));
        }

        delete toc;
        return 1;
    }

    if (writeDiskAtOnce(toc, dst, opts.fifoBuffers, opts.swap, 0, 0) != 0) {
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

    if (ret == 0 && opts.eject) {
        dst->loadUnload(1);
    }

    if (!opts.keepImage) {
        if (unlink(opts.dataFilename) != 0)
            log_message(-2, "Cannot remove CD image file \"%s\": %s", opts.dataFilename,
                        strerror(errno));
    }

    delete toc;

    return ret;
}

int copyCdOnTheFly(DaoCommandLine &opts, CdrDriver *src, CdrDriver *dst)
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
    src->taoSource(opts.taoSource);
    if (opts.taoSourceAdjust >= 0)
        src->taoSourceAdjust(opts.taoSourceAdjust);

    src->onTheFly(1);
    // the fd is not used by 'readDiskToc', just need to
    // indicate that on-the-fly copying is active for
    // automatical selection if the first track's pre-gap
    // is padded with zeros in the created Toc.

    if ((toc = src->readDiskToc(opts.session, "-")) == NULL) {
        log_message(-2, "Creation of source CD toc failed.");
        ret = 1;
        goto fail;
    }

    if (opts.withCddb) {
        if (readCddb(opts, toc) != 0) {
            ret = 1;
            goto fail;
        } else {
            log_message(2, "CD-TEXT data was added to toc.");
        }
    }

    if (checkToc(toc, opts.force) != 0) {
        log_message(-3, "Toc created from source CD image is inconsistent"
                        "- please report.");
        toc->print(cout, errPrintParams);
        ret = 1;
        goto fail;
    }

    switch (dst->checkToc(toc)) {
    case 0: // OK
        break;
    case 1: // warning
        if (!opts.force) {
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

        opts.verbose = 0;

#ifdef __CYGWIN__
        /* Close the SCSI interface and open it again. This will re-init the
         * ASPI layer which is required for the child process
         */

        delete src->scsiIf();

        src->scsiIf(new ScsiIf(opts.sourceScsiDevice));

        if (src->scsiIf()->init() != 0) {
            log_message(-2, "Re-init of SCSI interace failed.");

            // indicate end of data
            close(pipeFds[1]);

            while (1)
                sleep(10);
        }
#endif

        if (src->readDisk(opts.session, "-") != NULL)
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

    if (writeDiskAtOnce(toc, dst, opts.fifoBuffers, opts.swap, 0, 0) != 0) {
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

    if (ret == 0 && opts.eject) {
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

} // End of anonymous namespace

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
    options.progName = argv[0];

    Settings *settings = new Settings;

    settingsPath = "/etc/cdrdao.conf";
    if (settings->read(settingsPath) == 0)
        log_message(3, "Read settings from \"%s\".", settingsPath);

    settingsPath = "/etc/defaults/cdrdao";
    if (settings->read(settingsPath) == 0)
        log_message(3, "Read settings from \"%s\".", settingsPath);

    settingsPath = "/etc/default/cdrdao";
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
    if (options.parseCmdLine(argc - 1, argv + 1, settings) != 0) {
        log_set_verbose(2);
        printVersion();
        options.printUsage();
        exit(1);
    }

    log_set_verbose(options.verbose);
    options.commitSettings(settings, settingsPath);

    // Just show version ? We're done.
    if (options.command == SHOW_VERSION) {
        printVersion();
        goto fail;
    }

    errPrintParams.no_utf8 = options.no_utf8;
    filePrintParams.no_utf8 = options.no_utf8;

    // ---------------------------------------------------------------------
    //   Parse and check the toc file
    // ---------------------------------------------------------------------
    if (cmdInfo[options.command].tocParse) {

        // Parse TOC file
        toc = Toc::read(options.tocFile);

        if (options.remoteMode) {
            unlink(options.tocFile.c_str());
        }

        // Check and resolve input files paths
        if (!toc || !toc->resolveFilenames(options.tocFile.c_str())) {
            exitCode = 1;
            goto fail;
        }

        if (!toc->convertFilesToWav()) {
            log_message(-2, "Could not decode audio files from toc file \"%s\".",
                        options.tocFile.c_str());
            exitCode = 1;
            goto fail;
        }

        toc->recomputeLength();

        if (cmdInfo[options.command].tocCheck) {
            if (checkToc(toc, options.force) != 0) {
                log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile.c_str());
                exitCode = 1;
                goto fail;
            }
        }
    }

    // ---------------------------------------------------------------------
    //   Setup the CD device, obtain disk media information.
    // ---------------------------------------------------------------------

    if (cmdInfo[options.command].requiredDevice != NO_DEVICE) {

        if (options.scsiDevice.empty()) {
            options.scsiDevice = getDefaultDevice(cmdInfo[options.command].requiredDevice);
        }

        if (options.scsiDevice.empty()) {
            log_message(-2, "No device specified, no default device found.");
            exitCode = 1;
            goto fail;
        }

        cdr = setupDevice(options.command, options.scsiDevice, options.driverId,
                          /* init device? */
                          (options.command == UNLOCK) ? 0 : 1,
                          /* check for ready status? */
                          (options.command == DRIVE_INFO) ? 0 : 1,
                          /* reset status of medium if not empty? */
                          (options.command == SIMULATE || options.command == WRITE) ? 1 : 0,
                          options.readingSpeed, options.remoteMode, options.reload);

        if (cdr == NULL) {
            log_message(-2, "Cannot setup device %s.", options.scsiDevice.c_str());
            exitCode = 1;
            goto fail;
        }

        cdrScsi = cdr->scsiIf();

        if ((di = cdr->diskInfo()) == NULL) {
            log_message(-2, "Cannot get disk information.");
            exitCode = 1;
            goto fail;
        }
    }

    // ---------------------------------------------------------------------
    //   Process fullburn option for writing commands.
    // ---------------------------------------------------------------------

    if (options.command == SIMULATE || options.command == WRITE || options.command == COPY_CD) {
        if (options.fullBurn) {
            if (!options.driverId.empty() && options.driverId != "generic-mmc-raw") {
                log_message(-2, "You must use the generic-mmc-raw driver to use the "
                                "full-burn option.");
                exitCode = 1;
                goto fail;
            } else {
                int mins = options.userCapacity ? options.userCapacity
                                                : Msf(cdr->diskInfo()->capacity).min();
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
        if (options.sourceScsiDevice != NULL && options.scsiDevice != options.sourceScsiDevice) {
            delSrcDevice = 1;
            srcCdr = setupDevice(READ_CD, options.sourceScsiDevice, options.sourceDriverId, 1, 1, 0,
                                 options.readingSpeed, false, false);

            if (srcCdr == NULL) {
                log_message(-2, "Cannot setup source device %s.", options.sourceScsiDevice);
                exitCode = 1;
                goto fail;
            }

            srcCdrScsi = srcCdr->scsiIf();

            if ((srcDi = srcCdr->diskInfo()) == NULL) {
                log_message(-2, "Cannot get disk information from source device.");
                exitCode = 1;
                goto fail;
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
        if ((exitCode = readCddb(options, toc)) == 0) {
            log_message(1, "Writing CD-TEXT populated toc-file \"%s\".", options.tocFile.c_str());
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
        showToc(toc, options);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile.c_str());
        }
        break;

    case TOC_INFO:
        showTocInfo(toc, options.tocFile);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile.c_str());
        }
        break;

    case TOC_SIZE:
        showTocSize(toc, options.tocFile);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile.c_str());
        }
        break;

    case SHOW_DATA:
        showData(toc, options.swap);
        break;

    case READ_TEST:
        log_message(1, "Starting read test...");
        log_message(2, "Process can be aborted with QUIT signal "
                       "(usually CTRL-\\).");
        if (writeDiskAtOnce(toc, NULL, options.fifoBuffers, options.swap, 1,
                            options.writingSpeed) != 0) {
            log_message(-2, "Read test failed.");
            exitCode = 1;
            goto fail;
        }
        break;

    case DISK_INFO:
        showDiskInfo(di);
        break;

    case CDTEXT:
        showCDText(cdr, options);
        break;

    case DISCID:
        if (di->valid.empty && di->empty) {
            log_message(-2, "Inserted disk is empty.");
            exitCode = 1;
            goto fail;
        }
        cdr->subChanReadMode(options.readSubchanMode);
        cdr->rawDataReading(options.readRaw);
        cdr->mode2Mixed(options.mode2Mixed);
        cdr->fastTocReading(true);
        cdr->taoSource(options.taoSource);
        if (options.taoSourceAdjust >= 0)
            cdr->taoSourceAdjust(options.taoSourceAdjust);

        cdr->force(options.force);

        if ((toc = cdr->readDiskToc(options.session, (options.dataFilename == NULL)
                                                         ? "data.wav"
                                                         : options.dataFilename)) == NULL) {
            cdr->rezeroUnit(0);
            exitCode = 1;
            goto fail;
        } else {
            cdr->rezeroUnit(0);

            if (options.printQuery)
                printCddbQuery(toc);
            else
                readCddb(options, toc, true);
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
            exitCode = 1;
            goto fail;
        }
        log_message(1, "Reading toc data...");

        if (access(options.tocFile.c_str(), R_OK) == 0) {
            log_message(-2, "File \"%s\" exists, will not overwrite.", options.tocFile.c_str());
            exitCode = 1;
            goto fail;
        }

        cdr->subChanReadMode(options.readSubchanMode);
        cdr->rawDataReading(options.readRaw);
        cdr->mode2Mixed(options.mode2Mixed);
        cdr->fastTocReading(options.fastToc);
        cdr->taoSource(options.taoSource);
        if (options.taoSourceAdjust >= 0)
            cdr->taoSourceAdjust(options.taoSourceAdjust);

        cdr->force(options.force);

        if ((toc = cdr->readDiskToc(options.session, (options.dataFilename == NULL)
                                                         ? "data.wav"
                                                         : options.dataFilename)) == NULL) {
            cdr->rezeroUnit(0);
            exitCode = 1;
            goto fail;
        } else {
            cdr->rezeroUnit(0);

            if (options.withCddb) {
                if (readCddb(options, toc) == 0) {
                    log_message(2, "CD-TEXT data was added to toc-file.");
                }
            }

            {
                ofstream out(options.tocFile);
                if (!out) {
                    log_message(-2, "Cannot open \"%s\" for writing: %s", options.tocFile.c_str(),
                                strerror(errno));
                    exitCode = 1;
                    goto fail;
                }
                toc->print(out, filePrintParams);
            }

            log_message(1, "Reading of toc data finished successfully.");
        }
        break;

    case READ_CD:
        if (di->valid.empty && di->empty) {
            log_message(-2, "Inserted disk is empty.");
            exitCode = 1;
            goto fail;
        }
        log_message(1, "Reading toc and track data...");

        if (access(options.tocFile.c_str(), R_OK) == 0) {
            log_message(-2, "File \"%s\" exists, will not overwrite.", options.tocFile.c_str());
            exitCode = 1;
            goto fail;
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
                            (options.dataFilename == NULL) ? "data.bin" : options.dataFilename);

        if (toc == NULL) {
            cdr->rezeroUnit(0);
            exitCode = 1;
            goto fail;
        }
        cdr->rezeroUnit(0);

        if (options.withCddb) {
            if (readCddb(options, toc) == 0) {
                log_message(2, "CD-TEXT data was added to toc-file.");
            }
        }

        {
            ofstream out(options.tocFile);
            if (!out) {
                log_message(-2, "Cannot open \"%s\" for writing: %s", options.tocFile.c_str(),
                            strerror(errno));
                exitCode = 1;
                goto fail;
            }
            toc->print(out, filePrintParams);
        }

        log_message(1, "Reading of toc and track data finished successfully.");
        break;

    case WRITE:
        if (!options.writeSimulate)
            cdr->simulate(false);
        // fall through

    case SIMULATE:
        if (di->valid.empty && !di->empty && (!di->valid.append || !di->append)) {
            log_message(-2, "Inserted disk is not empty and not appendable.");
            exitCode = 1;
            goto fail;
        }

        if (toc->length().lba() > di->capacity) {
            log_message((options.overburn ? -1 : -2),
                        "Length of toc (%s, %ld blocks) exceeds capacity ", toc->length().str(),
                        toc->length().lba());
            log_message(0, "of CD-R (%s, %ld blocks).", Msf(di->capacity).str(), di->capacity);

            if (options.overburn) {
                log_message(-1, "Ignored because of option '--overburn'.");
                log_message(-1, "Some drives may fail to record this toc.");
            } else {
                log_message(-2, "Please use option '--overburn' to start"
                                "recording anyway.");
                exitCode = 1;
                goto fail;
            }
        }

        if (options.multiSession) {
            if (cdr->multiSession(1) != 0) {
                log_message(-2, "This driver does not support "
                                "multi session discs.");
                exitCode = 1;
                goto fail;
            }
        }

        if (options.writingSpeed >= 0) {
            if (cdr->speed(options.writingSpeed) != 0) {
                log_message(-2, "Writing speed %d not supported by device.", options.writingSpeed);
                exitCode = 1;
                goto fail;
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
                log_message(-2,
                            "Toc-file \"%s\" may create undefined "
                            "results.",
                            options.tocFile.c_str());
                log_message(-2, "Use option --force to use it anyway.");
                exitCode = 1;
                goto fail;
            }
            break;
        default: // error
            log_message(-2, "Toc-file \"%s\" is not suitable for this drive.",
                        options.tocFile.c_str());
            exitCode = 1;
            goto fail;
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
            exitCode = 1;
            goto fail;
        }

        if (writeDiskAtOnce(toc, cdr, options.fifoBuffers, options.swap, 0, 0) != 0) {
            if (cdr->simulate()) {
                log_message(-2, "Simulation failed.");
            } else {
                log_message(-2, "Writing failed.");
            }
            cdr->preventMediumRemoval(0);
            cdr->rezeroUnit(0);
            exitCode = 1;
            goto fail;
        }

        if (cdr->simulate()) {
            log_message(1, "Simulation finished successfully.");
        } else {
            log_message(1, "Writing finished successfully.");
        }

        cdr->rezeroUnit(0);
        if (cdr->preventMediumRemoval(0) != 0) {
            exitCode = 1;
            goto fail;
        }

        if (options.eject) {
            cdr->loadUnload(1);
        }
        break;

    case COPY_CD:
        if (cdr != srcCdr) {
            if (di->valid.empty && !di->empty && (!di->valid.append || !di->append)) {
                log_message(-2, "Medium in recorder device is not empty"
                                "and not appendable.");
                exitCode = 1;
                goto fail;
            }
        }

        if (srcDi->valid.empty && srcDi->empty) {
            log_message(-2, "Medium in source device is empty.");
            exitCode = 1;
            goto fail;
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
                exitCode = 1;
                goto fail;
            }
        }

        if (options.writingSpeed >= 0) {
            if (cdr->speed(options.writingSpeed) != 0) {
                log_message(-2, "Writing speed %d not supported by device.", options.writingSpeed);
                exitCode = 1;
                goto fail;
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
                exitCode = 1;
                goto fail;
            }

            if (copyCdOnTheFly(options, srcCdr, cdr) == 0) {
                log_message(1, "On-the-fly CD copying finished successfully.");
            } else {
                log_message(-2, "On-the-fly CD copying failed.");
                exitCode = 1;
                goto fail;
            }

        } else {
            if (srcCdr != cdr)
                srcCdr->remote(options.remoteMode, options.remoteFd);

            if (copyCd(options, srcCdr, cdr) == 0) {
                log_message(1, "CD copying finished successfully.");
            } else {
                log_message(-2, "CD copying failed.");
                exitCode = 1;
                goto fail;
            }
        }
        break;

    case BLANK:
        if (options.writingSpeed >= 0) {
            if (cdr->speed(options.writingSpeed) != 0) {
                log_message(-2, "Blanking speed %d not supported by device.", options.writingSpeed);
                exitCode = 1;
                goto fail;
            }
        }

        cdr->remote(options.remoteMode, options.remoteFd);
        cdr->simulate(options.writeSimulate);

        log_message(1, "Blanking disk...");
        if (cdr->blankDisk(options.blankingMode) != 0) {
            log_message(-2, "Blanking failed.");
            exitCode = 1;
            goto fail;
        }

        if (options.eject)
            cdr->loadUnload(1);
        break;

    case UNLOCK:
        log_message(1, "Trying to unlock drive...");

        cdr->abortDao();

        if (cdr->preventMediumRemoval(0) != 0) {
            exitCode = 1;
            goto fail;
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
        if (fh) {
            if (!DeviceIoControl(fh, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL))
                log_message(-2, "Couldn't unlock device %s!", devstr);
            else
                log_message(2, "Device %s unlocked.", devstr);
            CloseHandle(fh);
        }
    }
#endif
    exit(exitCode);
}

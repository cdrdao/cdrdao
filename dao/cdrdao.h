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

#ifndef __CDRDAO_H__
#define __CDRDAO_H__

#include <string>
#include <memory.h>
#include <optional>

#include "trackdb/util.h"
#include "CdrDriver.h"

class Settings;

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

#ifdef UNIXWARE
extern "C" {
extern int seteuid(uid_t);
extern int setegid(uid_t);
};
#endif

typedef enum {
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
    EJECT
} DaoCommand;

typedef enum {
    NO_DEVICE,
    NEED_CDR_R,
    NEED_CDR_W,
    NEED_CDRW_W,
} DaoDeviceType;

// The cmdInfo vector provides information about each of cdrdao's
// main commands, including some of the basic processing steps
// required, almost as a simplified state machine.

struct cmdStruct
{
    // The command code, which is also the index into the array
    DaoCommand cmd;
    // The command-line string for this command
    std::string str;
    // What type of device does the command require, for device
    // auto-detection.
    DaoDeviceType requiredDevice;
    // Does the command require a toc file
    bool needTocFile;
    // Does the command require to parse an existing toc file
    bool tocParse;
    // Does the command require to check the parsed toc file
    bool tocCheck;
    // Does the command accepts an optional toc file
    bool optionalTocFile;
};

struct DaoCommandLine {
    std::optional<DaoCommand> command;
    cmdStruct  commandInfo;

    const char *progName;
    std::optional<std::string> tocFile;
    std::string driverId;
    std::string sourceDriverId;
    std::string scsiDevice;
    const char *sourceScsiDevice;
    const char *dataFilename;
    std::string cddbLocalDbDir;
    std::string tmpFileDir;
    std::vector<std::string> cddbServerList;

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

class Cdrdao
{
public:
    Cdrdao(std::optional<DaoCommandLine> options = std::nullopt);

    static void printVersion();
    void showDriveInfo(const DriveInfo *i);
    void showTocInfo(const Toc *toc, const std::string &tocFile);
    void showTocSize(const Toc *toc, const std::string &tocFile);
    void showToc(Toc *toc, DaoCommandLine &options);
    void showData(const Toc *toc, bool swap);
    void ejectDisc(CdrDriver *cdr, DaoCommandLine &options);
    void showCDText(CdrDriver *cdr, DaoCommandLine &options);
    void showDiskInfo(DiskInfo *di);
    int showMultiSessionInfo(DiskInfo *di);
    void printCddbQuery(Toc *toc);
    int readCddb(const DaoCommandLine &opts, Toc *toc, bool showEntry = false);
    void scanBus();
    int checkToc(const Toc *toc, bool force);
    int copyCd(DaoCommandLine &opts, CdrDriver *src, CdrDriver *dst);
    int copyCdOnTheFly(DaoCommandLine &opts, CdrDriver *src, CdrDriver *dst);

    CdrDriver *selectDriver(DaoCommand, std::shared_ptr<ScsiIf>, const std::string &);
    std::string getDefaultDevice(DaoDeviceType req);
    CdrDriver *setupDevice(DaoCommand, const std::string &dev, const std::string &driver,
			   int initDevice, int checkReady, int checkEmpty, int readingSpeed,
			   bool remote, bool reload);

    PrintParams errPrintParams, filePrintParams;
    int MAX_RETRY=10;
};

#endif

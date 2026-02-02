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

#include <unistd.h>
#include <cstring>
#include <fstream>

#include "cdrdao.h"
#include "trackdb/log.h"
#include "trackdb/Toc.h"
#include "Settings.h"
#include "dao.h"

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
	Cdrdao::printVersion();
        options.printUsage();
        exit(1);
    }

    log_set_verbose(options.verbose);
    options.commitSettings(settings, settingsPath);

    Cdrdao cdrdao(options);

    // Just show version ? We're done.
    if (options.command == SHOW_VERSION) {
	Cdrdao::printVersion();
        goto fail;
    }

    // ---------------------------------------------------------------------
    //   Parse and check the toc file
    // ---------------------------------------------------------------------
    if (options.commandInfo.tocParse) {

        // Parse TOC file
        toc = Toc::read(*options.tocFile);

        if (options.remoteMode) {
            unlink(options.tocFile->c_str());
        }

        // Check and resolve input files paths
        if (!toc || !toc->resolveFilenames(options.tocFile->c_str())) {
            exitCode = 1;
            goto fail;
        }

        if (!toc->convertFilesToWav()) {
            log_message(-2, "Could not decode audio files from toc file \"%s\".",
                        options.tocFile->c_str());
            exitCode = 1;
            goto fail;
        }

        toc->recomputeLength();

        if (options.commandInfo.tocCheck) {
            if (cdrdao.checkToc(toc, options.force) != 0) {
                log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile->c_str());
                exitCode = 1;
                goto fail;
            }
        }
    }

    // ---------------------------------------------------------------------
    //   Setup the CD device, obtain disk media information.
    // ---------------------------------------------------------------------

    if (options.commandInfo.requiredDevice != NO_DEVICE) {

        if (options.scsiDevice.empty()) {
            options.scsiDevice = cdrdao.getDefaultDevice(options.commandInfo.requiredDevice);
        }

        if (options.scsiDevice.empty()) {
            log_message(-2, "No device specified, no default device found.");
            exitCode = 1;
            goto fail;
        }

        cdr = cdrdao.setupDevice(*options.command, options.scsiDevice, options.driverId,
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
            srcCdr = cdrdao.setupDevice(READ_CD, options.sourceScsiDevice, options.sourceDriverId, 1, 1, 0,
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

    switch (*options.command) {
    case READ_CDDB:
        if ((exitCode = cdrdao.readCddb(options, toc)) == 0) {
            log_message(1, "Writing CD-TEXT populated toc-file \"%s\".", options.tocFile->c_str());
            if (toc->write(*options.tocFile) != 0)
                exitCode = 2;
        }
        break;

    case SCAN_BUS:
        cdrdao.scanBus();
        break;

    case DRIVE_INFO:
        cdrdao.showDriveInfo(cdr->driveInfo(true));
        break;

    case SHOW_TOC:
        cdrdao.showToc(toc, options);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile->c_str());
        }
        break;

    case TOC_INFO:
        cdrdao.showTocInfo(toc, *options.tocFile);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile->c_str());
        }
        break;

    case TOC_SIZE:
        cdrdao.showTocSize(toc, *options.tocFile);
        if (toc->check() > 1) {
            log_message(-2, "Toc file \"%s\" is inconsistent.", options.tocFile->c_str());
        }
        break;

    case SHOW_DATA:
        cdrdao.showData(toc, options.swap);
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
        cdrdao.showDiskInfo(di);
        break;

    case CDTEXT:
        cdrdao.showCDText(cdr, options);
        break;

    case EJECT:
        cdrdao.ejectDisc(cdr, options);
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
                cdrdao.printCddbQuery(toc);
            else
                cdrdao.readCddb(options, toc, true);
        }
        break;

    case MSINFO:
        switch (cdrdao.showMultiSessionInfo(di)) {
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

	if (options.tocFile.has_value()) {
	    if (access(options.tocFile->c_str(), R_OK) == 0) {
		log_message(-2, "File \"%s\" exists, will not overwrite.", options.tocFile->c_str());
		exitCode = 1;
		goto fail;
	    }
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
                if (cdrdao.readCddb(options, toc) == 0) {
                    log_message(2, "CD-TEXT data was added to toc-file.");
                }
            }

            if (options.tocFile.has_value()) {
		std::ofstream out(*options.tocFile);
                if (!out) {
                    log_message(-2, "Cannot open \"%s\" for writing: %s", options.tocFile->c_str(),
                                strerror(errno));
                    exitCode = 1;
                    goto fail;
                }
                toc->print(out, cdrdao.filePrintParams);
            } else {
                toc->print(std::cout, cdrdao.filePrintParams);
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

        if (access(options.tocFile->c_str(), R_OK) == 0) {
            log_message(-2, "File \"%s\" exists, will not overwrite.", options.tocFile->c_str());
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
            if (cdrdao.readCddb(options, toc) == 0) {
                log_message(2, "CD-TEXT data was added to toc-file.");
            }
        }

        {
	    std::ofstream out(*options.tocFile);
            if (!out) {
                log_message(-2, "Cannot open \"%s\" for writing: %s", options.tocFile->c_str(),
                            strerror(errno));
                exitCode = 1;
                goto fail;
            }
            toc->print(out, cdrdao.filePrintParams);
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
                            options.tocFile->c_str());
                log_message(-2, "Use option --force to use it anyway.");
                exitCode = 1;
                goto fail;
            }
            break;
        default: // error
            log_message(-2, "Toc-file \"%s\" is not suitable for this drive.",
                        options.tocFile->c_str());
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

            if (cdrdao.copyCdOnTheFly(options, srcCdr, cdr) == 0) {
                log_message(1, "On-the-fly CD copying finished successfully.");
            } else {
                log_message(-2, "On-the-fly CD copying failed.");
                exitCode = 1;
                goto fail;
            }

        } else {
            if (srcCdr != cdr)
                srcCdr->remote(options.remoteMode, options.remoteFd);

            if (cdrdao.copyCd(options, srcCdr, cdr) == 0) {
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

    case SHOW_VERSION:
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
    exit(exitCode);
}

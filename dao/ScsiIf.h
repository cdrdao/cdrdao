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

// \file ScsiIf.h
//   \brief Low level SCSI interface.

#ifndef __SCSIIF_H__
#define __SCSIIF_H__

#include <stdlib.h>
#include <string>

class ScsiIfImpl;

//! \brief Base class to communicate with SCSI device

class ScsiIf
{
 public:
   
    //! \brief Constructor. Does not do SCSI initialization.
    //
    // Sets up some internal data and gets page size.

    ScsiIf(const char *dev);
    ~ScsiIf();

    //! \brief Accessor method: vendor string of the device.
    const char *vendor() const { return vendor_; }
    //! \brief Accessor method: product string of the device.
    const char *product() const { return product_; }
    //! \brief Accessor method: revision string of the device.
    const char *revision() const { return revision_; }

    //! \brief Accessor method: SCSI bus this device is connected to.
    const int bus ();
    //! \brief Accessor method: SCSI ID of the device.
    const int id ();
    /*! \brief Accessor method: SCSI LUN of the device. */
    const int lun ();

    //! \brief Opens the scsi device. Most of the code originates
    // from cdrecord's initialization function.
    // Tries to build a scglib SCSI* object using device specified in
    // ScsiIf::ScsiIf and issues an inquiry to test the
    // communication. Gets max DMA transfer length using scg_bufsize,
    // upto MAX_DATALEN_LIMIT, and builds a buffer of this size.
    //
    // \return int
    //   - 0 OK
    //   - 1 device could not be opened
    //   - 2 inquiry failed
    int init();
  
    // \brief Accessor method: returns max DMA transfer length.
    int maxDataLen() const { return maxDataLen_; }

    //! \brief Sends a SCSI command and receives data
    // \param cmd Buffer with CDB
    // \param cmdLen Length of CDB
    // \param dataOut Output buffer from the command, preallocated,
    //        will be overwritten
    // \param dataOutLen Length of preallocated output
    //        buffer. dataOutLen or dataInLen must be 0.
    // \param dataIn Input buffer to the command, containing
    //        parameters to the command
    // \param dataInLen Length of input buffer. dataOutLen or
    //        dataInLen must be 0.
    // \param showMessage If 0 makes scglib silent. If 1 verbose
    //        command execution.
    // \return int
    //   - 0 OK
    //   - 1 scsi command failed (os level, no sense data available)
    //   - 2 scsi command failed (sense data available)
    int sendCmd(const unsigned char *cmd, int cmdLen,
		const unsigned char *dataOut, int dataOutLen,
		unsigned char *dataIn, int dataInLen, int showMessage = 1);

    //! \brief Return the actual sense buffer in scglib
    // \param len will be overwritten and contain
    //        ScsiIf::impl->scgp_->scmd->sense_count (length of returned
    //        buffer).
    //	\return ScsiIf::impl->scgp_->scmd->u_sense.cmd_sense. This
    //	      buffer contains last sense data available to scglib. The
    //	      buffer is owned by scglib and must not be freed.
    const unsigned char *getSense(int &len) const;
  
    //! \brief Prints extended status information of the last SCSI command.
    // Prints the following SCSI codes:
    //  - command transport status
    //	- CDB
    //  - SCSI status byte
    //  - Sense Bytes
    //  - Decoded Sense data
    //  - DMA status
    //  - SCSI timing
    //
    // to file specified in ScsiIf::impl_->scgp_->errfile (defaults to
    // stderr)
    void printError();

    //! \brief Sets new timeout (seconds) and returns old timeout.
    int timeout(int);

    //! \brief Issues TEST UNIT READY command
    //	\return int
    //   - 0 OK, ready
    //   - 1 not ready (busy, default case when TUR command fails,
    //       except when there's no disk in drive, see below)
    //   - 2 not ready, no disk in drive
    //   - 3 scsi command failed at OS level, no sense available
    int testUnitReady();

    //! \brief Check for mmc capability. Return whether the
    //driver/drive can read and write CD-R and CD-RW disks.
    bool checkMmc(bool *cd_r_read,  bool *cd_r_write,
		  bool *cd_rw_read, bool *cd_rw_write);

    struct ScanData {
	std::string dev;
	// This is crazy, but the schily header #define vendor, product
	// and revision. Talk about namespace pollution...
	char vendor[9];
	char product[17];
	char revision[5];
    };

    //! Scans for all SCSI devices and returns a newly allocated
    // 'ScanData' array.
    static ScanData *scan(int *len, char* scsi_dev_path = NULL);

 private:
    char vendor_[9];
    char product_[17];
    char revision_[5];

    int maxDataLen_;

    // \brief Standard INQUIRY command used to fill ScsiIf::vendor_ ,
    // ScsiIf::product_ , ScsiIf::revision_
    // \return int
    //   - 0 if all right
    //   - 1 if INQUIRY command failed
    int inquiry();

    ScsiIfImpl *impl_;
};

#endif

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

#include "util.h"
#include <stdlib.h>
#include <string>

typedef struct {
    unsigned char p_len;
    unsigned cd_r_read : 1;
    unsigned cd_rw_read : 1;
    unsigned method2 : 1;
    unsigned dvd_rom_read : 1;
    unsigned dvd_r_read : 1;
    unsigned dvd_ram_read : 1;
    unsigned res_2_67 : 2;
    unsigned cd_r_write : 1;
    unsigned cd_rw_write : 1;
    unsigned test_write : 1;
    unsigned res_3_3 : 1;
    unsigned dvd_r_write : 1;
    unsigned dvd_ram_write : 1;
    unsigned res_3_67 : 2;
} cd_page_2a;

class ScsiIf
{
  public:
    virtual ~ScsiIf() {}

    // Create a SCSI interface based on its opaque string device representation.
    static ScsiIf* create(const std::string& dev);
    
    const std::string& vendor() const   { return vendor_; }
    const std::string& product() const  { return product_; }
    const std::string& revision() const { return revision_; }

    virtual int init() = 0;

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
    // \param showMessage If 0 makes it silent. If 1 verbose
    //        command execution.
    // \return int
    //   - 0 OK
    //   - 1 scsi command failed (os level, no sense data available)
    //   - 2 scsi command failed (sense data available)

    virtual int sendCmd(const u8 *cmd, int cmdLen, const u8 *dataOut, int dataOutLen, u8 *dataIn,
                        int dataInLen, int showMessage = 1) = 0;

    //! \brief Return the actual sense buffer
    // \param len will be overwritten and contain
    //        the length of returned buffer.
    //	\return buffer contains last sense data available. The
    //	      buffer is owned and must not be freed.
    virtual const u8 *getSense(int &len) const = 0;

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
    virtual void printError() = 0;

    // Sets new timeout (in seconds) and returns old timeout.
    virtual int timeout(int) = 0;

    // Check for mmc capability. Return whether the driver/drive can
    // read and write CD-R and CD-RW disks.
    //
    virtual cd_page_2a *checkMmc();

    // Standard INQUIRY command used to fill ScsiIf::vendor_ ,
    // ScsiIf::product_ , ScsiIf::revision_
    //
    // Returns int
    //   - 0 if all right
    //   - 1 if INQUIRY command failed
    //
    virtual int inquiry();

    struct ScanData {
        std::string dev;
        std::string vendor;
        std::string product;
        std::string revision;
    };

    // Scans for all SCSI devices and returns a newly allocated
    // 'ScanData' array.
    //
    static ScanData *scan(int *len, char *scsi_dev_path = NULL);

  protected:
    std::string dev_;
    std::string vendor_;
    std::string product_;
    std::string revision_;

    int maxDataLen_;
};

#endif

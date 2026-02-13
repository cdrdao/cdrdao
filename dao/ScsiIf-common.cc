/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1999  Andreas Mueller <mueller@daneb.ping.de>
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

#include "ScsiIf.h"

#include "trackdb/util.h"
#include "trackdb/log.h"

int ScsiIf::inquiry()
{
    unsigned char cmd[6] = { 0x12 /* INQUIRY */, 0, 0, 0, 0x2c, 0};
    unsigned char result[0x2c];
    int i;

    memset(result, 0, sizeof(result));

    if (sendCmd(cmd, 6, NULL, 0, result, 0x2c, 1) != 0) {
        log_message(-2, "Inquiry command failed on \"%s\"", dev_.c_str());
        return 1;
    }

    vendor_ = std::string(result + 8, result + 16);
    product_ = std::string(result + 16, result + 32);
    revision_ = std::string(result + 32, result + 36);

    Util::trimSpaces(vendor_);
    Util::trimSpaces(product_);
    Util::trimSpaces(revision_);

    return 0;
}

cd_page_2a *ScsiIf::checkMmc()
{
    static const int MODE_SENSE_G1_CMD = 0x5a;
    static const int MODE_MAX_SIZE = 256;
    static const int MODE_PAGE_HEADER_SIZE = 8;
    static const int MODE_CD_CAP_PAGE = 0x2a;

    static unsigned char mode[MODE_MAX_SIZE];
    memset(mode, 0, sizeof(mode));

    // First, read header of mode page 0x2A, to figure out its exact
    // length. For this, we issue a MODE_SENSE (10) command with a
    // data length of 8, just the size of the mode header.
    unsigned char cmd[10];
    memset(&cmd, 0, sizeof(cmd));
    cmd[0] = MODE_SENSE_G1_CMD; // MODE SENSE(10)
    cmd[2] = MODE_CD_CAP_PAGE;
    cmd[8] = MODE_PAGE_HEADER_SIZE;
    if (sendCmd((unsigned char *)&cmd, 10, NULL, 0, mode, MODE_PAGE_HEADER_SIZE) != 0) {
        return NULL;
    }

    int len = ((mode[0] << 8) + mode[1]) + 2; // +2 is for address field
    if (len > MODE_MAX_SIZE)
        len = MODE_MAX_SIZE;

    // Now we have the length of page 0x2a, read the whole page.
    memset(mode, 0, MODE_PAGE_HEADER_SIZE);
    memset(&cmd, 0, sizeof(cmd));
    cmd[0] = MODE_SENSE_G1_CMD; // MODE SENSE(10)
    cmd[2] = MODE_CD_CAP_PAGE;
    cmd[8] = len;
    if (sendCmd((unsigned char *)&cmd, 10, NULL, 0, mode, len) != 0) {
        return NULL;
    }

    return (cd_page_2a *)(mode + 9);
}

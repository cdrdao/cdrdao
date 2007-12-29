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
/*
 * $Log: ScsiIf-common.cc,v $
 * Revision 1.4  2007/12/29 12:26:33  poolshark
 * Complete rewrite of native Linux SG driver for SG 3.0 using SG_IO ioctl. Code cleanup
 *
 * Revision 1.3  2004/04/13 01:23:44  poolshark
 * Cleanup of scglib selection. Fixed without-scglib option. Default build of scsilib was problematic on older systems
 *
 * Revision 1.2  2004/03/23 18:46:07  poolshark
 * MMC autodetect mode
 *
 * Revision 1.1.1.1  2000/02/05 01:36:55  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

// checks if unit is ready
// return: 0: OK, ready
//         1: not ready (busy)
//         2: not ready, no disk in drive
//         3: scsi command failed



int ScsiIf::testUnitReady()
{
  unsigned char cmd[6];
  const unsigned char *senseData;
  int senseLen;
  int ret = 0;

  memset(cmd, 0, 6);

  switch (sendCmd(cmd, 6, NULL, 0, NULL, 0, 0)) {
  case 1:
    ret = 3;
    break;

  case 2:
    senseData = getSense(senseLen);

    switch (senseData[2] & 0x0f) {
    case 0x02: // Not ready
      switch (senseData[12]) {
      case 0x3a: // medium not present
	ret = 2;
	break;

      default:
	ret = 1;
	break;
      }
      break;

    case 0x06: // Unit attention
      ret = 0;
      break;

    default:
      ret = 3;
      break;
    }
  }

  return ret;
}

typedef struct {
    unsigned char p_len;
    unsigned cd_r_read : 1;
    unsigned cd_rw_read : 1;
    unsigned method2  : 1;
    unsigned dvd_rom_read : 1;
    unsigned dvd_r_read : 1;
    unsigned dvd_ram_read : 1;
    unsigned res_2_67 : 2;
    unsigned cd_r_write : 1;
    unsigned cd_rw_write : 1;
    unsigned test_write : 1;
    unsigned res_3_3  : 1;
    unsigned dvd_r_write : 1;
    unsigned dvd_ram_write : 1;
    unsigned res_3_67 : 2;
} cd_page_2a;

bool ScsiIf::checkMmc(bool *cd_r_read,  bool *cd_r_write,
                      bool *cd_rw_read, bool *cd_rw_write)
{
    static const int MODE_SENSE_G1_CMD = 0x5a;
    static const int MODE_MAX_SIZE = 256;
    static const int MODE_PAGE_HEADER_SIZE = 8;
    static const int MODE_CD_CAP_PAGE = 0x2a;

    unsigned char mode[MODE_MAX_SIZE];
    memset(mode, 0, sizeof(mode));

    // First, read header of mode page 0x2A, to figure out its exact
    // length. For this, we issue a MODE_SENSE (10) command with a
    // data length of 8, just the size of the mode header.
    unsigned char cmd[10];
    memset(&cmd, 0, sizeof(cmd));
    cmd[0] = MODE_SENSE_G1_CMD; // MODE SENSE(10)
    cmd[2] = MODE_CD_CAP_PAGE;
    cmd[8] = MODE_PAGE_HEADER_SIZE;
    if (sendCmd((unsigned char*)&cmd, 10, NULL, 0, mode,
		MODE_PAGE_HEADER_SIZE) != 0) {
	return false;
    }

    int len = ((mode[0] << 8) + mode[1]) + 2; // +2 is for address field
    if (len > MODE_MAX_SIZE) len = MODE_MAX_SIZE;

    // Now we have the length of page 0x2a, read the whole page.
    memset(mode, 0, MODE_PAGE_HEADER_SIZE);
    memset(&cmd, 0, sizeof(cmd));
    cmd[0] = MODE_SENSE_G1_CMD; // MODE SENSE(10)
    cmd[2] = MODE_CD_CAP_PAGE;
    cmd[8] = len;
    if (sendCmd((unsigned char*)&cmd, 10, NULL, 0, mode, len) != 0) {
	return false;
    }

  cd_page_2a *p2a = (cd_page_2a*)(mode + 9);

  *cd_r_read   = p2a->cd_r_read;
  *cd_r_write  = p2a->cd_r_write;
  *cd_rw_read  = p2a->cd_rw_read;
  *cd_rw_write = p2a->cd_rw_write;
  return true;
}

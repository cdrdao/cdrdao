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

cd_page_2a* ScsiIf::checkMmc()
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
    if (sendCmd((unsigned char*)&cmd, 10, NULL, 0, mode,
		MODE_PAGE_HEADER_SIZE) != 0) {
	return NULL;
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
	return NULL;
    }

  return (cd_page_2a*)(mode + 9);
}

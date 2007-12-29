/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: decodeSense.cc,v $
 * Revision 1.2  2007/12/29 12:26:34  poolshark
 * Complete rewrite of native Linux SG driver for SG 3.0 using SG_IO ioctl. Code cleanup
 *
 * Revision 1.1.1.1  2000/02/05 01:38:06  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.1  1999/03/14 15:34:03  mueller
 * Initial revision
 *
 */


/* Sense code printing for direct SCSI implementations */

struct StringTable {
  int code;
  char *message;
};

static StringTable SENSE_KEYS[] = {
  { 0x00, "NO SENSE" },
  { 0x01, "RECOVERED ERROR" },
  { 0x02, "NOT READY" },
  { 0x03, "MEDIUM ERROR" },
  { 0x04, "HARDWARE ERROR" },
  { 0x05, "ILLEGAL REQUEST" },
  { 0x06, "UNIT ATTENTION" },
  { 0x08, "BLANK CHECK" },
  { 0x09, "VENDOR SPECIFIC" },
  { 0x0b, "ABORTED COMMAND" },
  { 0x0d, "VOLUME OVERFLOW" },
  { 0x0e, "MISCOMPARE" },
  { 0x00, (char *)0 }
};

static const char *getFromStringTable(const StringTable *t, int code)
{
  while (t->message != NULL) {
    if (t->code == code) {
      return t->message;
    }

    t += 1;
  }

  return NULL;
}

// Prints decoded sense message, if 'ignoreUnitAttention' is != 0 and sense
// code indicates unit attention nothing will be printed and 0 will be
// returned.
// return: 0: OK, no error
//         1: sense key indicates error
static int decodeSense(const unsigned char *buf, int len)
{
  int code = buf[2] & 0x0f;
  const char *msg;

  if (code == 0) {
    return 0;
  }

  msg = getFromStringTable(SENSE_KEYS, code);

  log_message(-2, "SCSI command failed:");
  log_message(-2, "  sense key 0x%x: %s.", code, 
	  msg != NULL ? msg : "unknown code");
    
  if (len > 0x0c && buf[7] != 0) {
    log_message(-2, "  additional sense code: 0x%x", buf[0x0c]);
  }
  if (len > 0x0d && buf[7] != 0) {
    log_message(-2, "  additional sense code qualifier: 0x%x", buf[0x0d]);
  }

  return 1;
}

/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: SubChannel.cc,v $
 * Revision 1.2  2000/12/17 10:51:23  andreasm
 * Default verbose level is now 2. Adaopted message levels to have finer
 * grained control about the amount of messages printed by cdrdao.
 * Added CD-TEXT writing support to the GenericMMCraw driver.
 * Fixed CD-TEXT cue sheet creating for the GenericMMC driver.
 *
 * Revision 1.1.1.1  2000/02/05 01:37:27  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.4  1999/04/05 11:04:48  mueller
 * Added decoding of media catalog number and ISRC code.
 *
 * Revision 1.3  1999/03/27 20:58:55  mueller
 * Added various access functions.
 *
 * Revision 1.2  1998/08/30 19:10:32  mueller
 * Added handling of Catalog Number and ISRC codes.
 *
 * Revision 1.1  1998/08/29 21:31:00  mueller
 * Initial revision
 *
 *
 */

#include "SubChannel.h"

#include <ctype.h>

// lookup table for crc calculation
unsigned short SubChannel::crctab[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
  0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
  0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
  0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
  0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
  0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
  0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
  0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
  0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
  0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
  0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
  0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
  0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
  0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
  0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
  0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
  0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
  0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
  0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
  0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
  0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
  0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
  0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
  0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
  0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
  0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
  0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

SubChannel::SubChannel()
{
  crcValid_ = 1; // CRC is usually valid
}


SubChannel::~SubChannel()
{
}

void SubChannel::crcInvalid()
{
  crcValid_ = 0;
}

SubChannel::Type SubChannel::type() const
{
  return type_;
}

// returns bcd value for given binary value if it is >= 0 and <= 99, otherwise
// the given value is returned
unsigned char SubChannel::bcd(int d)
{
  if (d >= 0 && d <= 99)
    return ((d / 10) << 4) | (d % 10);
  else 
    return d;
}

int SubChannel::bcd2int(unsigned char d)
{
  unsigned char d1 = d & 0x0f;
  unsigned char d2 = d >> 4;

  if (d1 <= 9 && d2 <= 9) {
    return d2 * 10 + d1;
  }
  else {
    return d;
  }
}

int SubChannel::isBcd(unsigned char d)
{
  if ((d & 0x0f) <= 9 && (d >> 4) <= 9)
    return 1;

  return 0;
}

// Encodes the given catalog number digits 'n1' - 'n13' (ascii) into
// provided buffer 'buf' of 7 bytes length
void SubChannel::encodeCatalogNumber(unsigned char *buf, char n1, char n2,
				     char n3, char n4, char n5, char n6,
				     char n7, char n8, char n9, char n10,
				     char n11, char n12, char n13)
{
  buf[0] = ((n1 - '0') << 4)  | ((n2 - '0') & 0x0f);
  buf[1] = ((n3 - '0') << 4)  | ((n4 - '0') & 0x0f);
  buf[2] = ((n5 - '0') << 4)  | ((n6 - '0') & 0x0f);
  buf[3] = ((n7 - '0') << 4)  | ((n8 - '0') & 0x0f);
  buf[4] = ((n9 - '0') << 4)  | ((n10 - '0') & 0x0f);
  buf[5] = ((n11 - '0') << 4) | ((n12 - '0') & 0x0f);
  buf[6] = ((n13 - '0') << 4);
}

// Decodes given 7 byte buffer to ascii media catalog numbers.
void SubChannel::decodeCatalogNumber(const unsigned char *in, char *n1,
				     char *n2, char *n3,  char *n4, char *n5,
				     char *n6, char *n7, char *n8, char *n9,
				     char *n10, char *n11, char *n12,
				     char *n13)
{
  *n1  = ((in[0] >> 4) & 0x0f) + '0';
  *n2  = (in[0] & 0x0f) + '0';
  *n3  = ((in[1] >> 4) & 0x0f) + '0';
  *n4  = (in[1] & 0x0f) + '0';
  *n5  = ((in[2] >> 4) & 0x0f) + '0';
  *n6  = (in[2] & 0x0f) + '0';
  *n7  = ((in[3] >> 4) & 0x0f) + '0';
  *n8  = (in[3] & 0x0f) + '0';
  *n9  = ((in[4] >> 4) & 0x0f) + '0';
  *n10 = (in[4] & 0x0f) + '0';
  *n11 = ((in[5] >> 4) & 0x0f) + '0';
  *n12 = (in[5] & 0x0f) + '0';
  *n13 = ((in[6] >> 4) & 0x0f) + '0';
}

// Encodes given ISRC code into provided buffer of 8 bytes length.
// c1 - c2: country code (ascii digits or letters)
// o1 - o3: country code (ascii digits or letters)
// y1 - y2: year of recording (ascii digits)
// s1 - s2: serial number (ascii digits)
void SubChannel::encodeIsrcCode(unsigned char *buf, char c1, char c2,
				char o1, char o2, char o3, char y1, char y2,
				char s1, char s2, char s3, char s4, char s5)
{
  unsigned char d;

  buf[0] = ascii2Isrc(c1) << 2;

  d = ascii2Isrc(c2);
  buf[0] |= d >> 4;
  buf[1] = d << 4;

  d = ascii2Isrc(o1);
  buf[1] |= d >> 2;
  buf[2] = d << 6;

  buf[2] |= ascii2Isrc(o2);

  buf[3] =  ascii2Isrc(o3) << 2;

  buf[4] = ((y1 - '0') << 4)  | ((y2 - '0') & 0x0f);
  buf[5] = ((s1 - '0') << 4)  | ((s2 - '0') & 0x0f);
  buf[6] = ((s3 - '0') << 4)  | ((s4 - '0') & 0x0f);
  buf[7] = ((s5 - '0') << 4);
}

// Decodes ISRC code from given 8 byte length buffer to ascii characters.
void SubChannel::decodeIsrcCode(const unsigned char *in, char *c1, char *c2,
				char *o1, char *o2, char *o3, char *y1,
				char *y2, char *s1, char *s2, char *s3,
				char *s4, char *s5)
{
  unsigned char d;

  d = (in[0] >> 2) & 0x3f;
  *c1 = isrc2Ascii(d);

  d = ((in[0] & 0x03) << 4) | ((in[1] >> 4) & 0x0f);
  *c2 = isrc2Ascii(d);

  d = ((in[1] & 0x0f) << 2) | ((in[2] >> 6) & 0x03);
  *o1 = isrc2Ascii(d);

  d = in[2] & 0x3f;
  *o2 = isrc2Ascii(d);

  d = (in[3] >> 2) & 0x3f;
  *o3 = isrc2Ascii(d);

  *y1 = ((in[4] >> 4) & 0x0f) + '0';
  *y2 = (in[4] & 0x0f) + '0';
  *s1 = ((in[5] >> 4) & 0x0f) + '0';
  *s2 = (in[5] & 0x0f) + '0';
  *s3 = ((in[6] >> 4) & 0x0f) + '0';
  *s4 = (in[6] & 0x0f) + '0';
  *s5 = ((in[7] >> 4) & 0x0f) + '0';
}

unsigned char SubChannel::ascii2Isrc(char c)
{
  if (isdigit(c))
    return (c - '0') & 0x3f;

  if (isupper(c))
    return (c - 'A' + 17) & 0x3f;

  if (islower(c))
    return (c - 'a' + 17) & 0x3f;

  return 0;
}

char SubChannel::isrc2Ascii(unsigned char c)
{
  if (c <= 9)
    return '0' + c;

  if (c >= 17 && c <= 42)
    return 'A' + (c - 17);

  return 0;
}

int SubChannel::checkConsistency()
{
  const char *buf;
  int i;

  switch (type()) {
  case QMODE1TOC:
    // Not complete, yet.
    if (min() > 99 || sec() > 59 || frame() > 74)
      return 0;
    break;

  case QMODE5TOC:
    // not required right now
    break;

  case QMODE1DATA:
    if (trackNr() < 1 || trackNr() > 99)
      return 0;

    if (indexNr() > 99)
      return 0;

    if (min() > 99 || sec() > 59 || frame() > 74)
      return 0;

    if (amin() > 99 || asec() > 59 || aframe() > 74)
      return 0;
    break;

  case QMODE2:
    buf = catalog();
    for (i = 0; i < 13; i++) {
      if (!isdigit(buf[i])) 
	return 0;
    }
    break;

  case QMODE3:
    buf = isrc();
    for (i = 0; i < 5; i++) {
      if (!isdigit(buf[i]) && !isupper(buf[i])) 
	return 0;
    }
    for (i = 5; i < 12; i++) {
      if (!isdigit(buf[i]))
	return 0;
    }
    break;

  case QMODE_ILLEGAL:
    return 0;
    break;
  }

  return 1;
}

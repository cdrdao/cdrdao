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

#include "PWSubChannel96.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "log.h"

PWSubChannel96::PWSubChannel96()
{
  memset(data_, 0, 96);
  type_ = QMODE1DATA;
}

PWSubChannel96::PWSubChannel96(unsigned char *buf)
{
  init(buf);
}

void PWSubChannel96::init(unsigned char *buf)
{
  memcpy(data_, buf, 96);

  switch (getChannelByte(Q_CHAN, 0) & 0x0f) {
  case 1:
    type_ = QMODE1DATA;
    break;

  case 2:
    type_ = QMODE2;
    break;

  case 3:
    type_ = QMODE3;
    break;

  case 5:
    type_ = QMODE5TOC;
    break;

  default:
    type_ = QMODE_ILLEGAL;
    break;
  }
}

PWSubChannel96::~PWSubChannel96()
{
}

SubChannel *PWSubChannel96::makeSubChannel(Type t)
{
  PWSubChannel96 *chan = new PWSubChannel96;

  chan->type_ = t;

  switch (t) {
  case QMODE1TOC:
  case QMODE1DATA:
    chan->setChannelByte(Q_CHAN, 0, 0x01);
    break;
    
  case QMODE2:
    chan->setChannelByte(Q_CHAN, 0, 0x02);
    break;

  case QMODE3:
    chan->setChannelByte(Q_CHAN, 0, 0x03);
    break;

  case QMODE5TOC:
    chan->setChannelByte(Q_CHAN, 0, 0x05);
    break;

  case QMODE_ILLEGAL:
    chan->setChannelByte(Q_CHAN, 0, 0x00);
    break;
  }

  return chan;
}

SubChannel *PWSubChannel96::makeSubChannel(unsigned char *buf)
{
  PWSubChannel96 *chan = new PWSubChannel96(buf);

  return chan;
}

void PWSubChannel96::setChannelByte(Channel chan, int byteNr,
				    unsigned char value)
{
  assert(byteNr >= 0 && byteNr < 12);

  register unsigned char setMask = 1 << chan;
  register unsigned char clearMask = ~setMask;

  register unsigned char *p = data_ + (byteNr * 8);
  register int i;

  for (i = 0; i < 8; i++) {
    if (value & 0x80)
      *p |= setMask;
    else
      *p &= clearMask;

    p++;
    value <<= 1;
  }
}

unsigned char PWSubChannel96::getChannelByte(Channel chan, int byteNr) const
{
  assert(byteNr >= 0 && byteNr < 12);

  register unsigned char testMask = 1 << chan;
  register const unsigned char *p = data_ + (byteNr * 8);
  register unsigned char val = 0;
  register int i;
  
  for (i = 0; i < 8; i++) {
    val <<= 1;
    if ((*p) & testMask)
      val |= 0x01;

    p++;
  }

  return val;
}

const unsigned char *PWSubChannel96::data() const
{
  return data_;
}

long PWSubChannel96::dataLength() const
{
  return 96;
}

// calculate the crc over Q sub channel bytes 0-9 and stores it in byte 10,11
void PWSubChannel96::calcCrc()
{
  register unsigned short crc = 0;
  register int i;

  for (i = 0; i < 10; i++) {
    register unsigned char data = getChannelByte(Q_CHAN, i);
    crc = crctab[(crc >> 8) ^ data] ^ (crc << 8);
  }

  crc = ~crc;

  setChannelByte(Q_CHAN, 10, crc >> 8);
  setChannelByte(Q_CHAN, 11, crc);
}

int PWSubChannel96::checkCrc() const
{
  register unsigned short crc = 0;
  register int i;

  if (!crcValid_) {
    return 1;
  }

  for (i = 0; i < 10; i++) {
    register unsigned char data = getChannelByte(Q_CHAN, i);
    crc = crctab[(crc >> 8) ^ data] ^ (crc << 8);
  }

  crc = ~crc;

  if (getChannelByte(Q_CHAN, 10) == (crc >> 8) &&
      getChannelByte(Q_CHAN, 11) == (crc & 0xff))
    return 1;
  else
    return 0;
}


// sets P channel bit
void PWSubChannel96::pChannel(int f)
{
  register int i;

  if (f != 0) {
    for (i = 0; i < 96; i++)
      data_[i] |= 0x80;
  }
  else {
    for (i = 0; i < 96; i++)
      data_[i] &= 0x7f;
  }    
}

// returns Q type
SubChannel::Type PWSubChannel96::type() const
{
  return type_;
}


// set Q type
void PWSubChannel96::type(unsigned char type)
{
  switch (type & 0x0f) {
  case 1:
    type_ = QMODE1DATA;
    break;

  case 2:
    type_ = QMODE2;
    break;

  case 3:
    type_ = QMODE3;
    break;

  case 5:
    type_ = QMODE5TOC;
    break;

  default:
    type_ = QMODE_ILLEGAL;
    break;
  }

  register unsigned char val = getChannelByte(Q_CHAN, 0);

  val &= 0xf0;
  val |= type & 0x0f;

  setChannelByte(Q_CHAN, 0, val);
}

// function for setting various Q sub channel fields

void PWSubChannel96::ctl(int c)
{
  assert((c & 0x0f) == 0);

  register unsigned char val = getChannelByte(Q_CHAN, 0);

  val &= 0x0f;
  val |= c & 0xf0;

  setChannelByte(Q_CHAN, 0, val);
}

unsigned char PWSubChannel96::ctl() const
{
  register unsigned char val = getChannelByte(Q_CHAN, 0);

  return val >> 4;
}

void PWSubChannel96::trackNr(int t)
{
  assert(type_ == QMODE1DATA);
  setChannelByte(Q_CHAN, 1, bcd(t));
}

int PWSubChannel96::trackNr() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(getChannelByte(Q_CHAN, 1));
}

void PWSubChannel96::indexNr(int i)
{
  assert(type_ == QMODE1DATA);
  setChannelByte(Q_CHAN, 2, bcd(i));
}

int PWSubChannel96::indexNr() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(getChannelByte(Q_CHAN, 2));
}

void PWSubChannel96::point(int p)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 2, bcd(p));
}

void PWSubChannel96::min(int m)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 3, bcd(m));
}

int PWSubChannel96::min() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(getChannelByte(Q_CHAN, 3));
}

void PWSubChannel96::sec(int s)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 4, bcd(s));
}

int PWSubChannel96::sec() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(getChannelByte(Q_CHAN, 4));
}

void PWSubChannel96::frame(int f)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 5, bcd(f));
}

int PWSubChannel96::frame() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(getChannelByte(Q_CHAN, 5));
}

void PWSubChannel96::zero(int z)
{
  assert(type_ == QMODE5TOC);

  setChannelByte(Q_CHAN, 6, bcd(z));
}

void PWSubChannel96::amin(int am)
{
  assert(type_ == QMODE1DATA);
  setChannelByte(Q_CHAN, 7, bcd(am));
}

int PWSubChannel96::amin() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(getChannelByte(Q_CHAN, 7));
}

void PWSubChannel96::asec(int as)
{
  assert(type_ == QMODE1DATA);
  setChannelByte(Q_CHAN, 8, bcd(as));
}

int PWSubChannel96::asec() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(getChannelByte(Q_CHAN, 8));
}

void PWSubChannel96::aframe(int af)
{
  assert(type_ == QMODE1DATA || type_ == QMODE2 || type_ == QMODE3);
  setChannelByte(Q_CHAN, 9, bcd(af));
}

int PWSubChannel96::aframe() const
{
  assert(type_ == QMODE1DATA || type_ == QMODE2 || type_ == QMODE3);
  return bcd2int(getChannelByte(Q_CHAN, 9));
}

void PWSubChannel96::pmin(int pm)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 7, bcd(pm));
}

void PWSubChannel96::psec(int ps)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 8, bcd(ps));
}

void PWSubChannel96::pframe(int pf)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  setChannelByte(Q_CHAN, 9, bcd(pf));
}

void PWSubChannel96::catalog(char n1, char n2, char n3, char n4, char n5,
			     char n6, char n7, char n8, char n9, char n10,
			     char n11, char n12, char n13)
{
  assert(type_ == QMODE2);

  unsigned char buf[8];
  int i;
  
  encodeCatalogNumber(buf, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10, n11, n12,
		      n13);
  buf[7] = 0;

  for (i = 0; i < 8; i++)
    setChannelByte(Q_CHAN, i + 1, buf[i]);
}

const char *PWSubChannel96::catalog() const
{
  static char buf[14];
  unsigned char in[7];
  int i;

  for (i = 0; i < 7; i++)
    in[i] = getChannelByte(Q_CHAN, i + 1);

  decodeCatalogNumber(in, &buf[0], &buf[1], &buf[2], &buf[3], &buf[4],
		      &buf[5], &buf[6], &buf[7], &buf[8], &buf[9], &buf[10],
		      &buf[11], &buf[12]);

  buf[13] = 0;

  return buf;
}

void PWSubChannel96::isrc(char c1, char c2, char o1, char o2, char o3,
			  char y1, char y2, char s1, char s2, char s3,
			  char s4, char s5)
{
  assert(type_ == QMODE3);

  unsigned char buf[8];
  int i;

  encodeIsrcCode(buf, c1, c2, o1, o2, o3, y1, y2, s1, s2, s3, s4, s5);

  for (i = 0; i < 8; i++)
    setChannelByte(Q_CHAN, i + 1, buf[i]);
}

const char *PWSubChannel96::isrc() const
{
  static char buf[13];
  unsigned char in[8];
  int i;

  assert(type_ == QMODE3);

  for (i = 0; i < 8; i++)
    in[i] = getChannelByte(Q_CHAN, i + 1);

  decodeIsrcCode(in, &buf[0], &buf[1], &buf[2], &buf[3], &buf[4],
		 &buf[5], &buf[6], &buf[7], &buf[8], &buf[9], &buf[10], 
		 &buf[11]);
  buf[12] = 0;

  return buf;
}


void PWSubChannel96::print() const
{
  if (type_ != QMODE_ILLEGAL) 
    log_message(0, "P:%02x ", getChannelByte(P_CHAN, 0));

  switch (type_) {
  case QMODE1TOC:
  case QMODE1DATA:
  case QMODE5TOC:
    log_message(0, "Q: (%02x) %02x,%02x %02x:%02x:%02x %02x %02x:%02x:%02x ", 
	   getChannelByte(Q_CHAN, 0), getChannelByte(Q_CHAN, 1),
	   getChannelByte(Q_CHAN, 2), getChannelByte(Q_CHAN, 3),
	   getChannelByte(Q_CHAN, 4), getChannelByte(Q_CHAN, 5),
	   getChannelByte(Q_CHAN, 6), getChannelByte(Q_CHAN, 7),
	   getChannelByte(Q_CHAN, 8), getChannelByte(Q_CHAN, 9));
    break;
  case QMODE2:
    log_message(0, "Q: (%02x) MCN: %s      %02x ", getChannelByte(Q_CHAN, 0),
	    catalog(), getChannelByte(Q_CHAN, 9));
    break;
  case QMODE3:
    log_message(0, "Q: (%02x) ISRC: %s      %02x ", getChannelByte(Q_CHAN, 0),
	    isrc(), getChannelByte(Q_CHAN, 9));
    break;
  case QMODE_ILLEGAL:
    log_message(0, "INVALID QMODE: %02x", getChannelByte(Q_CHAN, 0));
    break;
  }

  if (type_ != QMODE_ILLEGAL) 
    log_message(0, "%04x %d", 
	    (getChannelByte(Q_CHAN, 10) << 8) | getChannelByte(Q_CHAN, 11),
	    checkCrc());
}

// sets R-W channel bits from 72 byte data buffer, used for CD-TEXT
void PWSubChannel96::setRawRWdata(const unsigned char *data)
{
  int i;

  for (i = 0; i < 96; i += 4) {
    data_[i]     |= (data[0] >> 2) & 0x3f;
    data_[i + 1] |= ((data[0] << 4) & 0x30) | ((data[1] >> 4) & 0x0f);
    data_[i + 2] |= ((data[1] << 2) & 0x3c) | ((data[2] >> 6) & 0x03);
    data_[i + 3] |= data[2] & 0x3f;
    
    data += 3;
  }
}

// concatenates the R-W channel bits and writes them to provided 72 byte buffer
void PWSubChannel96::getRawRWdata(unsigned char *data) const
{
  int i;

  for (i = 0; i < 96; i += 4) {
    data[0] = ((data_[i] << 2) & 0xfc)     | ((data_[i + 1] >> 4) & 0x03);
    data[1] = ((data_[i + 1] << 4) & 0xf0) | ((data_[i + 2] >> 2) & 0x0f);
    data[2] = ((data_[i + 2] << 6) & 0xc0) | (data_[i + 3] & 0x3f);
    
    data += 3;
  }
}

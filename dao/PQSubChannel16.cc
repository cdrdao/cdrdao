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

#include "PQSubChannel16.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "log.h"

PQSubChannel16::PQSubChannel16()
{
  memset(data_, 0, 16);
  type_ = QMODE1DATA;
}


PQSubChannel16::~PQSubChannel16()
{
}

SubChannel *PQSubChannel16::makeSubChannel(Type t)
{
  PQSubChannel16 *chan = new PQSubChannel16;

  chan->type_ = t;

  switch (t) {
  case QMODE1TOC:
  case QMODE1DATA:
    chan->data_[0] = 0x01;
    break;
    
  case QMODE2:
    chan->data_[0] = 0x02;
    break;

  case QMODE3:
    chan->data_[0] = 0x03;
    break;

  case QMODE5TOC:
    chan->data_[0] = 0x05;
    break;

  case QMODE_ILLEGAL:
    chan->data_[0] = 0x00;
    break;
  }

  return chan;
}

void PQSubChannel16::init(unsigned char *buf)
{
  memcpy(data_, buf, 16);

  switch (data_[0] & 0x0f) {
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

SubChannel *PQSubChannel16::makeSubChannel(unsigned char *buf)
{
  PQSubChannel16 *chan = new PQSubChannel16;

  chan->init(buf);

  return chan;
}


const unsigned char *PQSubChannel16::data() const
{
  return data_;
}

long PQSubChannel16::dataLength() const
{
  return 16;
}

// calculate the crc over Q sub channel bytes 0-9 and stores it in byte 10,11
void PQSubChannel16::calcCrc()
{
  register unsigned short crc = 0;
  register int i;

  for (i = 0; i < 10; i++) {
    crc = crctab[(crc >> 8) ^ data_[i]] ^ (crc << 8);
  }

  data_[10] = crc >> 8;
  data_[11] = crc;
}

int PQSubChannel16::checkCrc() const
{
  register unsigned short crc = 0;
  register int i;

  if (!crcValid_) {
    return 1;
  }

  for (i = 0; i < 10; i++) {
    crc = crctab[(crc >> 8) ^ data_[i]] ^ (crc << 8);
  }

  if (data_[10] == (crc >> 8) && data_[11] == (crc & 0xff))
    return 1;
  else
    return 0;
}

// returns Q type
SubChannel::Type PQSubChannel16::type() const
{
  return type_;
}

// set Q type
void PQSubChannel16::type(unsigned char type)
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
  
  data_[0] &= 0xf0;
  data_[0] |= type & 0x0f;
}

// sets P channel bit
void PQSubChannel16::pChannel(int f)
{
  data_[15] = f != 0 ? 0x80 : 0;
}


// function for setting various Q sub channel fields

void PQSubChannel16::ctl(int c)
{
  assert((c & 0x0f) == 0);

  data_[0] &= 0x0f;
  data_[0] |= c & 0xf0;
}

unsigned char PQSubChannel16::ctl() const
{
  return data_[0] >> 4;
}

void PQSubChannel16::trackNr(int t)
{
  assert(type_ == QMODE1DATA);
  data_[1] = bcd(t);
}

int PQSubChannel16::trackNr() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(data_[1]);
}

void PQSubChannel16::indexNr(int i)
{
  assert(type_ == QMODE1DATA);
  data_[2] = bcd(i);
}

int PQSubChannel16::indexNr() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(data_[2]);
}

void PQSubChannel16::point(int p)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  data_[2] = bcd(p);
}

void PQSubChannel16::min(int m)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  data_[3] = bcd(m);
}

int PQSubChannel16::min() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(data_[3]);
}

void PQSubChannel16::sec(int s)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  data_[4] = bcd(s);
}

int PQSubChannel16::sec() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(data_[4]);
}

void PQSubChannel16::frame(int f)
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  data_[5] = bcd(f);
}

int PQSubChannel16::frame() const
{
  assert(type_ == QMODE1TOC || type_ == QMODE1DATA || type_ == QMODE5TOC);
  return bcd2int(data_[5]);
}

void PQSubChannel16::zero(int z)
{
  assert(type_ == QMODE5TOC);
  data_[6] = bcd(z);
}

void PQSubChannel16::amin(int am)
{
  assert(type_ == QMODE1DATA);
  data_[7] = bcd(am);
}

int PQSubChannel16::amin() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(data_[7]);
}

void PQSubChannel16::asec(int as)
{
  assert(type_ == QMODE1DATA);
  data_[8] = bcd(as);
}

int PQSubChannel16::asec() const
{
  assert(type_ == QMODE1DATA);
  return bcd2int(data_[8]);
}

void PQSubChannel16::aframe(int af)
{
  assert(type_ == QMODE1DATA || type_ == QMODE2 || type_ == QMODE3);
  data_[9] = bcd(af);
}

int PQSubChannel16::aframe() const
{
  assert(type_ == QMODE1DATA || type_ == QMODE2 || type_ == QMODE3);
  return bcd2int(data_[9]);
}

void PQSubChannel16::pmin(int pm)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  data_[7] = bcd(pm);
}

void PQSubChannel16::psec(int ps)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  data_[8] = bcd(ps);
}

void PQSubChannel16::pframe(int pf)
{
  assert(type_ == QMODE1TOC || type_ == QMODE5TOC);
  data_[9] = bcd(pf);
}

void PQSubChannel16::catalog(char n1, char n2, char n3, char n4, char n5,
			     char n6, char n7, char n8, char n9, char n10,
			     char n11, char n12, char n13)
{
  assert(type_ == QMODE2);

  encodeCatalogNumber(data_ + 1, n1, n2, n3, n4, n5, n6, n7, n8, n9, n10,
		      n11, n12, n13);
  data_[8] = 0;
}

const char *PQSubChannel16::catalog() const
{
  static char buf[14];

  assert(type_ == QMODE2);

  decodeCatalogNumber(data_ + 1, &buf[0], &buf[1], &buf[2], &buf[3], &buf[4],
		      &buf[5], &buf[6], &buf[7], &buf[8], &buf[9], &buf[10],
		      &buf[11], &buf[12]);

  buf[13] = 0;

  return buf;
}

void PQSubChannel16::isrc(char c1, char c2, char o1, char o2, char o3,
			  char y1, char y2, char s1, char s2, char s3,
			  char s4, char s5)
{
  assert(type_ == QMODE3);

  encodeIsrcCode(data_ + 1, c1, c2, o1, o2, o3, y1, y2, s1, s2, s3, s4, s5);
}


const char *PQSubChannel16::isrc() const
{
  static char buf[13];

  assert(type_ == QMODE3);

  decodeIsrcCode(data_ + 1, &buf[0], &buf[1], &buf[2], &buf[3], &buf[4],
		 &buf[5], &buf[6], &buf[7], &buf[8], &buf[9], &buf[10], 
		 &buf[11]);
  buf[12] = 0;

  return buf;
}


void PQSubChannel16::print() const
{
  if (type_ != QMODE_ILLEGAL) 
    log_message(0, "P:%02x ", data_[15]);

  switch (type_) {
  case QMODE1TOC:
  case QMODE1DATA:
  case QMODE5TOC:
    log_message(0, "Q: (%02x) %02x,%02x %02x:%02x:%02x %02x %02x:%02x:%02x ", 
	   data_[0], data_[1], data_[2], data_[3], data_[4], data_[5], 
	   data_[6], data_[7], data_[8], data_[9]);
    break;
  case QMODE2:
    log_message(0, "Q: (%02x) MCN: %s      %02x ", data_[0], catalog(), data_[9]);
    break;
  case QMODE3:
    log_message(0, "Q: (%02x) ISRC: %s      %02x ", data_[0], isrc(), data_[9]);
    break;
  case QMODE_ILLEGAL:
    log_message(0, "INVALID QMODE: %02x", data_[0]);
    break;
  }

  if (type_ != QMODE_ILLEGAL) 
    log_message(0, "%04x %d", (data_[10] << 8) | data_[11], checkCrc());
}

int PQSubChannel16::checkConsistency()
{
  switch (type_) {
  case QMODE1DATA:
    if (!isBcd(data_[3]) || !isBcd(data_[4]) || !isBcd(data_[5]) ||
	!isBcd(data_[7]) || !isBcd(data_[8]) || !isBcd(data_[9]))
      return 0;
    break;

  case QMODE1TOC:
  case QMODE2:
  case QMODE3:
  case QMODE5TOC:
  case QMODE_ILLEGAL:
    // no checks, yet
    break;
  }

  return SubChannel::checkConsistency();
}

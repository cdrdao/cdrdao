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
 * $Log: PWSubChannel96.h,v $
 * Revision 1.2  2000/12/17 10:51:22  andreasm
 * Default verbose level is now 2. Adaopted message levels to have finer
 * grained control about the amount of messages printed by cdrdao.
 * Added CD-TEXT writing support to the GenericMMCraw driver.
 * Fixed CD-TEXT cue sheet creating for the GenericMMC driver.
 *
 * Revision 1.1.1.1  2000/02/05 01:35:05  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.5  1999/04/05 11:04:48  mueller
 * Added decoding of media catalog number and ISRC code.
 *
 * Revision 1.4  1999/03/27 20:58:55  mueller
 * Added various access functions.
 *
 * Revision 1.3  1998/09/27 19:19:39  mueller
 * Added retrieval of control nibbles for track with 'analyzeTrack()'.
 *
 * Revision 1.2  1998/08/30 19:10:32  mueller
 * Added handling of Catalog Number and ISRC codes.
 *
 * Revision 1.1  1998/08/29 21:32:27  mueller
 * Initial revision
 *
 *
 */

#ifndef __PW_SUB_CHANNEL_96_H__
#define __PW_SUB_CHANNEL_96_H__

#include "SubChannel.h"

class PWSubChannel96 : public SubChannel {
public:

  enum Channel { P_CHAN = 7, Q_CHAN = 6 };

  PWSubChannel96();
  PWSubChannel96(unsigned char *buf);
  ~PWSubChannel96();

  // virtual constructors:
  // create sub channel with specified q-mode
  SubChannel *makeSubChannel(Type);
  // create sub channel with reading sub channel data from given buffer
  SubChannel *makeSubChannel(unsigned char *);

  // initialize sub-channel from given buffer (96 bytes)
  void init(unsigned char *);

  void type(unsigned char); // set Q type
  Type type() const;        // returns Q mode type

  long dataLength() const; // returns number of sub channel bytes

  void pChannel(int); // sets P channel bit

  void ctl(int);     // sets control flags
  unsigned char ctl() const; // return control nibbles in bits 0-3

  void trackNr(int); // sets track number (QMODE1DATA)
  int trackNr() const; // returns track number (QMODE1DATA)
  
  void indexNr(int); // sets index number (QMODE1DATA)
  int indexNr() const; // returns index number (QMODE1DATA)

  void point(int);   // sets point filed (QMODE1TOC)

  void min(int);     // track relative time (QMODE1TOC, QMODE1DATA)
  int min() const;
  void sec(int);     // track relative time (QMODE1TOC, QMODE1DATA)
  int sec() const;
  void frame(int);   // track relative time (QMODE1TOC, QMODE1DATA)
  int frame() const;

  void amin(int);    // absolute time (QMODE1DATA)
  int amin() const;

  void asec(int);    // absolute time (QMODE1DATA)
  int asec() const;

  void aframe(int);  // absolute time (QMODE1DATA)
  int aframe() const;

  void pmin(int);    // track start time (QMODE1TOC)
  void psec(int);    // track start time (QMODE1TOC)
  void pframe(int);  // track start time (QMODE1TOC)
  void zero(int);    // zero field (QMODE5TOC)

  // sets raw R-W channels from 72 byte buffer
  void setRawRWdata(const unsigned char *); 
  
  // gets raw R-W channels to 72 byte buffer
  void getRawRWdata(unsigned char *) const;

  void print() const;

  void calcCrc(); // calculates crc and stores it in crc fields
  int checkCrc() const;

  void catalog(char, char, char, char, char, char, char, char, char, char,
	       char, char, char);
  const char *catalog() const;

  void isrc(char, char, char, char, char, char, char, char, char, char, char,
	    char);
  const char *isrc() const;

  const unsigned char *data() const;

protected:
  unsigned char data_[96]; // P - W sub channel data

private:
  void setChannelByte(Channel, int byteNr, unsigned char value);
  unsigned char getChannelByte(Channel, int byteNr) const;
};

#endif

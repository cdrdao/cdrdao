/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001  Andreas Mueller <andreas@daneb.de>
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

#ifndef __CDTEXTITEM_H__
#define __CDTEXTITEM_H__

#include <iostream>

class CdTextItem {
public:
  enum DataType { SBCC, DBCC, BINARY };

  enum PackType { CDTEXT_TITLE = 0x80,
		  CDTEXT_PERFORMER = 0x81,
		  CDTEXT_SONGWRITER = 0x82,
		  CDTEXT_COMPOSER = 0x83,
		  CDTEXT_ARRANGER = 0x84,
		  CDTEXT_MESSAGE = 0x85,
		  CDTEXT_DISK_ID = 0x86,
		  CDTEXT_GENRE = 0x87,
		  CDTEXT_TOC_INFO1 = 0x88,
		  CDTEXT_TOC_INFO2 = 0x89,
		  CDTEXT_RES1 = 0x8a,
		  CDTEXT_RES2 = 0x8b,
		  CDTEXT_RES3 = 0x8c,
		  CDTEXT_RES4 = 0x8d,
		  CDTEXT_UPCEAN_ISRC = 0x8e,
		  CDTEXT_SIZE_INFO = 0x8f };

  CdTextItem(PackType packType, int blockNr, const char *data);

  CdTextItem(PackType packType, int blockNr,
	     const unsigned char *data, long len);

  CdTextItem(int blockNr, unsigned char genreCode1, unsigned char genreCode2,
	     const char *description);

  CdTextItem(const CdTextItem &);

  ~CdTextItem();

  DataType dataType() const { return dataType_; }
  
  PackType packType() const { return packType_; }

  int blockNr() const { return blockNr_; }

  const unsigned char *data() const { return data_; }

  long dataLen() const { return dataLen_; }

  void print(int isTrack, std::ostream &) const;

  int operator==(const CdTextItem &);
  int operator!=(const CdTextItem &);
  
  static const char *packType2String(int isTrack, int packType);
  
  static PackType int2PackType(int);
  static int isBinaryPack(PackType);
  static int isTrackPack(PackType);

private:
  friend class CdTextContainer;

  DataType dataType_;
  PackType packType_;
  int blockNr_; // 0 ... 7

  unsigned char *data_;
  long dataLen_;

  CdTextItem *next_;
};

#endif

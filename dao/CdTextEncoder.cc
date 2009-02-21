/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <mueller@daneb.ping.de>
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


#include "CdTextEncoder.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "log.h"
#include "Toc.h"
#include "CdTextItem.h"
#include "PWSubChannel96.h"


unsigned short CdTextEncoder::CRCTAB_[256] = {
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


class CdTextPackEntry {
public:
  CdTextPackEntry(unsigned char packType, unsigned char trackNr, 
		  unsigned char packId);

  int blockNr();
  void blockNr(int);

  void characterPos(int);
  
  union {
    CdTextPack pack;
    unsigned char packData[18];
  };

  CdTextPackEntry *next_;
};

CdTextPackEntry::CdTextPackEntry(unsigned char packType, unsigned char trackNr,
				 unsigned char packId)
{
  memset(&pack, 0, sizeof(pack));
  next_ = NULL;

  pack.packType = packType;
  pack.trackNumber = trackNr;
  pack.sequenceNumber = packId;
}

int CdTextPackEntry::blockNr()
{
  return (pack.blockCharacter >> 4) & 0x07;
}

void CdTextPackEntry::blockNr(int blockNr)
{
  pack.blockCharacter &= 0x8f;
  pack.blockCharacter |= (blockNr << 4) & 0x70;
}

void CdTextPackEntry::characterPos(int pos)
{
  pack.blockCharacter &= 0xf0;

  if (pos > 15) {
    pack.blockCharacter |= 0x0f;
  }
  else {
    pack.blockCharacter |= pos;
  }
}


CdTextEncoder::CdTextEncoder(const Toc *toc)
{
  int i;

  toc_ = toc;

  packCount_ = 0;
  packs_ = lastPack_ = NULL;
  lastPackPos_ = 0;

  subChannels_ = NULL;
  subChannelCount_ = 0;

  for (i = 0; i < 8; i++)
    memset(&(sizeInfo_[i]), 0, sizeof(CdTextSizeInfo));
}

CdTextEncoder::~CdTextEncoder()
{
  long i;
  CdTextPackEntry *pnext;

  toc_ = NULL;

  while (packs_ != NULL) {
    pnext = packs_->next_;

    delete packs_;
    packs_ = pnext;
  }

  lastPack_ = NULL;

  if (subChannels_ != NULL) {
    for (i = 0; i < subChannelCount_; i++) {
      delete subChannels_[i];
      subChannels_[i] = NULL;
    }
    
    delete[] subChannels_;
    subChannels_ = NULL;
  }
}

int CdTextEncoder::encode()
{
  buildPacks();

  calcCrcs();

  if (packs_ != NULL) {
    log_message(4, "\nCD-TEXT packs:");
    CdTextPackEntry *prun;
    for (prun = packs_; prun != NULL; prun = prun->next_) {
      log_message(4, "%02x %02x %02x %02x: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x  CRC: %02x %02x", prun->pack.packType,
	      prun->pack.trackNumber, prun->pack.sequenceNumber,
	      prun->pack.blockCharacter, prun->pack.data[0],
	      prun->pack.data[1], prun->pack.data[2], prun->pack.data[3],
	      prun->pack.data[4], prun->pack.data[5], prun->pack.data[6],
	      prun->pack.data[7], prun->pack.data[8], prun->pack.data[9],
	      prun->pack.data[10], prun->pack.data[11],
	      prun->pack.crc0, prun->pack.crc1);
    }
  }
  
  buildSubChannels();

  return 0;
}

const PWSubChannel96 **CdTextEncoder::getSubChannels(long *subChannelCount)
{
  *subChannelCount = subChannelCount_;

  return (const PWSubChannel96 **)subChannels_;
}

void CdTextEncoder::buildPacks()
{
  int blockNr;

  for (blockNr = 0; blockNr <= 7; blockNr++) {
    if (toc_->cdTextLanguage(blockNr) >= 0) {
      // only build the packs if the language code is defined
      packId_ = 0;
    
      buildPacks(blockNr, CdTextItem::CDTEXT_TITLE);
      buildPacks(blockNr, CdTextItem::CDTEXT_PERFORMER);
      buildPacks(blockNr, CdTextItem::CDTEXT_SONGWRITER);
      buildPacks(blockNr, CdTextItem::CDTEXT_COMPOSER);
      buildPacks(blockNr, CdTextItem::CDTEXT_ARRANGER);
      buildPacks(blockNr, CdTextItem::CDTEXT_MESSAGE);
      buildPacks(blockNr, CdTextItem::CDTEXT_DISK_ID);
      buildPacks(blockNr, CdTextItem::CDTEXT_GENRE);
      buildPacks(blockNr, CdTextItem::CDTEXT_TOC_INFO1);
      buildPacks(blockNr, CdTextItem::CDTEXT_TOC_INFO2);
      buildPacks(blockNr, CdTextItem::CDTEXT_UPCEAN_ISRC);
    }
  }

  buildSizeInfoPacks();
}

// Build packs for specified language (blockNr) and pack type. First the
// global pack is created and then the track specific packs.
void CdTextEncoder::buildPacks(int blockNr, CdTextItem::PackType type)
{
  const CdTextItem *globalItem;
  const CdTextItem *item;
  int i;
  int n = toc_->nofTracks();
  int tracks;
  int offset = toc_->firstTrackNo() == 0 ? 0 : toc_->firstTrackNo() - 1;

  if ((globalItem = toc_->getCdTextItem(0, blockNr, type)) != NULL) {
    encodeCdTextItem(0, blockNr, globalItem);
  }

  if (CdTextItem::isTrackPack(type)) {
    // count tracks that have the current pack type defined
    tracks = 0;

    for (i = 1; i <= n; i++) {
      if (toc_->getCdTextItem(i, blockNr, type) != NULL)
	tracks++;
    }

    if (tracks > 0) {
      if (globalItem == NULL && type == CdTextItem::CDTEXT_UPCEAN_ISRC) {
	// Special handling for UPC/EAN field. If this field is not defined
	// but ISRC codes are defined for the tracks an pack with an empty
	// string is created.
	CdTextItem upcEan(CdTextItem::CDTEXT_UPCEAN_ISRC, blockNr, "");
	encodeCdTextItem(0, blockNr, &upcEan);
      }

      for (i = 1; i <= n; i++) {
	if ((item = toc_->getCdTextItem(i, blockNr, type)) == NULL)
	  item = globalItem;

	if (item != NULL)
	  encodeCdTextItem(offset + i, blockNr, item);
      }
    }
  }
}

// Build CD-TEXT packs for a CdTextItem. Space from the last pack is
// used if the pack type and language are matching.
void CdTextEncoder::encodeCdTextItem(int trackNr, int blockNr,
				     const CdTextItem *item)
{
  long dataLen = item->dataLen();
  const unsigned char *data = item->data();
  long pos, n;
  CdTextPackEntry *pack = NULL;
  int first = 1;
  
  if (CdTextItem::isBinaryPack(item->packType())) {
    pos = 0;

    while (dataLen > 0) {
      switch (item->packType()) {
      case CdTextItem::CDTEXT_GENRE:
	pack = new CdTextPackEntry(item->packType(), 0, packId_++);
	if (pos > 0)
	  pack->characterPos((pos * 12) - 2);
	break;
      case CdTextItem::CDTEXT_TOC_INFO1:
	if (pos == 0)
	  pack = new CdTextPackEntry(item->packType(), 0, packId_++);
	else
	  pack = new CdTextPackEntry(item->packType(), ((pos - 1) * 4) + 1,
				     packId_++);
	break;
      default:
	pack = new CdTextPackEntry(item->packType(), pos, packId_++);
	break;
      }

      n = (dataLen > 12) ? 12 : dataLen;

      memcpy(pack->pack.data, data, n);
      pack->blockNr(blockNr);

      appendPack(pack);
      lastPackPos_ = 12;
      
      data += n;
      dataLen -= n;
      pos++;
    }
  }
  else {
    pos = 0;

    while (dataLen > 0) {
      if (first && lastPack_ != NULL &&
	  lastPack_->pack.packType == item->packType() &&
	  lastPack_->blockNr() == blockNr &&
	  lastPackPos_ < 12) {
	// we can use space from previous block
	pack = lastPack_;
      }
      else {
	pack = new CdTextPackEntry(item->packType(), trackNr, packId_++);
	pack->blockNr(blockNr);
	pack->characterPos(pos);
	appendPack(pack);
	lastPackPos_ = 0;
      }

      n = (dataLen > 12 - lastPackPos_) ? 12 - lastPackPos_ : dataLen;

      memcpy(pack->pack.data + lastPackPos_, data, n);

      data += n;
      lastPackPos_ += n;
      pos += n;
      dataLen -= n;

      first = 0;
    }
  }
}

// Create the SIZE_INFO CD-TEXT packs which contains a summary about all
// CD-TEXT packs.
void CdTextEncoder::buildSizeInfoPacks()
{
  CdTextItem *sizeInfoItem;
  int i;
  int b;

  if (lastPack_ == NULL) {
    // no packs at all -> nothing to do
    return;
  }

  for (b = 0; b < 8; b++) {
    sizeInfo_[b].characterCode = 0; // ISO/IEC 8859-1

    sizeInfo_[b].firstTrack = toc_->firstTrackNo() == 0 ? 1 : toc_->firstTrackNo();
    sizeInfo_[b].lastTrack = toc_->firstTrackNo() == 0 ? toc_->nofTracks() : toc_->firstTrackNo() + toc_->nofTracks() - 1;

    sizeInfo_[b].copyright = 0; // no copy protection

    for (i = 0; i < 8; i++) {
      if (sizeInfo_[b].lastSequenceNumber[i] > 0) {
	// set language code if we have at least one pack for this language
	sizeInfo_[b].languageCode[i] = toc_->cdTextLanguage(i);
	// adjust the last sequence number for the packs we will create
	sizeInfo_[b].lastSequenceNumber[i] += 3;
      }
    }

    // adjust the counter for the packs we're currently creating
    sizeInfo_[b].packTypeCount[15] += 3; // we create 3 packs 
  }

  for (b = 0; b < 8; b++) {
    if (sizeInfo_[b].lastSequenceNumber[b] > 0) {
      // ' - 3' because we've already adjusted the pack count
      packId_ = sizeInfo_[b].lastSequenceNumber[b] - 3 + 1;

      sizeInfoItem = new CdTextItem(CdTextItem::CDTEXT_SIZE_INFO, b, 
				    (unsigned char*)&(sizeInfo_[b]),
				    sizeof(CdTextSizeInfo));

      encodeCdTextItem(0, b, sizeInfoItem);

      delete sizeInfoItem;
    }
  }
}

// Calculates checksum for each CD-TEXT pack.
void CdTextEncoder::calcCrcs()
{
  CdTextPackEntry *prun;
  register unsigned char data;
  register unsigned short crc;
  register int i;

  for (prun = packs_; prun != NULL; prun = prun->next_) {
    crc = 0;

    for (i = 0; i < 16; i++) {
      data = prun->packData[i];
      crc = CRCTAB_[(crc >> 8) ^ data] ^ (crc << 8);
    }

    crc = ~crc;
    
    prun->pack.crc0 = crc >> 8;
    prun->pack.crc1 = crc;
  }
}

// Builds the R-W sub-channel data for all CD-TEXT packs. The sub-channel
// data of each sector can hold 4 CD-TEXT packs.
void CdTextEncoder::buildSubChannels()
{
  long i, j;
  CdTextPackEntry *prun;
  unsigned char buf[72];

  switch (packCount_ % 4) {
  case 0:
    subChannelCount_ = packCount_ / 4;
    break;
  case 2:
    subChannelCount_ = packCount_ / 2;
    break;
  default:
    subChannelCount_ = packCount_;
    break;
  }

  if (subChannelCount_ == 0) {
    subChannels_ = NULL;
    return;
  }

  subChannels_ = new PWSubChannel96*[subChannelCount_];
  
  prun = packs_;

  for (i = 0; i < subChannelCount_; i++) {
    subChannels_[i] = new PWSubChannel96;

    for (j = 0; j < 4; j++) {
      memcpy(buf + j * 18, prun->packData, 18);

      if ((prun = prun->next_) == NULL)
	prun = packs_;
    }

    subChannels_[i]->setRawRWdata(buf);

#if 0
    int k;

    for (j = 0; j < 6; j++) {
      log_message(0, "%ld:%d: ", i, j);
      for (k = 0; k < 16; k++) {
	log_message(0, "%02x ", subChannels_[i]->data()[j * 16 + k]);
      }
      log_message(0, "");
    }

    unsigned char buf1[72];

    subChannels_[i]->getRawRWdata(buf1);

    log_message(0, "Match: %d", memcmp(buf, buf1, 72));
#endif
  }


  assert(prun == packs_);
}


void CdTextEncoder::appendPack(CdTextPackEntry *pack)
{
  int b;

  assert(pack->blockNr() >= 0 && pack->blockNr() <= 7);

  pack->next_ = NULL;

  if (packs_ == NULL) {
    packs_ = lastPack_ = pack;
  }
  else {
    if (pack->blockNr() >= lastPack_->blockNr()) {
      lastPack_->next_ = pack;
      lastPack_ = pack;
    }
    else {
      CdTextPackEntry *prun, *ppred;

      for (ppred = NULL, prun = packs_; prun != NULL;
	   ppred = prun, prun = prun->next_) {
	if (pack->blockNr() < prun->blockNr())
	  break;
      }
      
      if (prun == NULL) {
	// this case should have been handled above
	lastPack_->next_ = pack;
	lastPack_ = pack;
      }
      else {
	if (ppred == NULL) {
	  pack->next_ = packs_;
	  packs_ = pack;
	}
	else {
	  ppred->next_ = pack;
	  pack->next_ = prun;
	}
      }
    }
  }
  
  packCount_++;

  // update summary data used for creating the SIZE_INFO pack
  sizeInfo_[pack->blockNr()].packTypeCount[pack->pack.packType - 0x80] += 1;

  for (b = 0; b < 8; b++) {
    if (pack->pack.sequenceNumber >
	sizeInfo_[b].lastSequenceNumber[pack->blockNr()]) {
      sizeInfo_[b].lastSequenceNumber[pack->blockNr()] =
	pack->pack.sequenceNumber;
    }
  }
}

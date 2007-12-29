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

#include <config.h>

#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "Track.h"
#include "util.h"
#include "log.h"
#include "TrackDataList.h"
#include "CdTextItem.h"
#include "lec.h"

Track::Track(TrackData::Mode t, TrackData::SubChannelMode st) 
  : length_(0), start_(0), end_(0)
{
  type_ = t;
  subChannelType_ = st;
  audioCutMode_ = -1;

  nofSubTracks_ = 0;
  subTracks_ = lastSubTrack_ = NULL;

  nofIndices_ = 0;
  index_ = new Msf[98];

  isrcValid_ = 0;

  flags_.copy = 0;         // digital copy not permitted
  flags_.preEmphasis = 0;  // no pre-emphasis
  flags_.audioType = 0;    // two channel audio
}

Track::Track(const Track &obj)
  : length_(obj.length_), start_(obj.start_), end_(obj.end_),
    cdtext_(obj.cdtext_)
{
  int i;
  SubTrack *run;

  type_ = obj.type_;
  subChannelType_ = obj.subChannelType_;
  audioCutMode_ = obj.audioCutMode_;

  nofSubTracks_ = obj.nofSubTracks_;
  subTracks_ = lastSubTrack_ = NULL;
  for (run = obj.subTracks_; run != NULL; run = run->next_) {
    if (subTracks_ == NULL) {
      subTracks_ = lastSubTrack_ = new SubTrack(*run);
    }
    else {
      lastSubTrack_->next_ = new SubTrack(*run);
      lastSubTrack_->next_->pred_ = lastSubTrack_;
      lastSubTrack_ = lastSubTrack_->next_;
    }
  }

  nofIndices_ = obj.nofIndices_;
  index_ = new Msf[98];
  for (i = 0; i < nofIndices_; i++) {
    index_[i] = obj.index_[i];
  }
  
  isrcValid_ = obj.isrcValid_;
  memcpy(isrcCountry_, obj.isrcCountry_, 2);
  memcpy(isrcOwner_, obj.isrcOwner_, 3);
  memcpy(isrcYear_, obj.isrcYear_, 2);
  memcpy(isrcSerial_, obj.isrcSerial_, 5);

  flags_ = obj.flags_;
}


Track::~Track()
{
  SubTrack *run = subTracks_;
  SubTrack *next = NULL;

  while (run != NULL) {
    next = run->next_;
    delete run;
    run = next;
  }

  delete[] index_;
}

// Returns first sub-track or 'NULL' if no sub-tracks are defined
const SubTrack *Track::firstSubTrack() const
{
  return subTracks_ != NULL ? subTracks_ : NULL;
}

// Returns last sub-track or 'NULL' if no sub-tracks are defined
const SubTrack *Track::lastSubTrack() const
{
  return lastSubTrack_ != NULL ? lastSubTrack_ : NULL;
}


// Appends given sub-track to list of sub-tracks.
// return: 0: OK
//         1: tried to append PAD sub-track
//         2: tried to append sub-track with different audioCutMode
int Track::append(const SubTrack &strack)
{
  if (strack.type() == SubTrack::PAD)
    return 1;

  if (audioCutMode_ != -1 && audioCutMode_ != strack.audioCutMode())
    return 2;

  if (lastSubTrack_ != NULL && lastSubTrack_->type() == SubTrack::PAD &&
      lastSubTrack_->mode() == strack.mode() &&
      lastSubTrack_->subChannelMode() == strack.subChannelMode()) {
    // remove padding sub track
    delete removeSubTrack(lastSubTrack_);
  }

  // append sub track
  insertSubTrackAfter(lastSubTrack_, new SubTrack(strack));

  update();

  return 0;
}


// Traverses all sub-tracks to update summary data of track.
void Track::update()
{
  long lenLba = 0;           // accumulates total length of track in blocks
  unsigned long slength;     // length of track in samples
  unsigned long padLen;      // padding length
  unsigned long blen;        // block length for current data mode
  SubTrack *run, *next;
  SubTrack *pad;
  TrackData::Mode dataMode;
  TrackData::SubChannelMode subChannelMode;

  // remove all padding sub-tracks
  run = subTracks_;
  while (run != NULL) {
    next = run->next_;

    if (run->type() == SubTrack::PAD) {
      delete removeSubTrack(run);
    }

    run = next;
  }

  audioCutMode_ = -1;

  if ((run = subTracks_) != NULL) {
    do {
      dataMode = run->mode();
      subChannelMode = run->subChannelMode();

      if (audioCutMode_ == -1)
	audioCutMode_ = run->audioCutMode();
      else if (audioCutMode_ != run->audioCutMode())
	log_message(-3, "Track::update: mixed audio cut mode.");

      if (audioCutMode_)
	blen = SAMPLES_PER_BLOCK;
      else
	blen = TrackData::dataBlockSize(dataMode, subChannelMode);
      
      slength = 0;

      // find continues range of sub tracks with same data mode
      do {
	slength += run->length();
	run = run->next_;
      } while (run != NULL && run->mode() == dataMode &&
	       run->subChannelMode() == subChannelMode);
      
      if ((padLen = slength % blen) != 0) {
	padLen = blen - padLen;

	if (audioCutMode_)
	  pad = new SubTrack(SubTrack::PAD, 0, TrackData(padLen));
	else
	  pad = new SubTrack(SubTrack::PAD, 0, 
			     TrackData(dataMode, subChannelMode, padLen));
	
	if (run != NULL) {
	  insertSubTrackAfter(run->pred_, pad);
	}
	else {
	  insertSubTrackAfter(lastSubTrack_, pad);
	}
	slength += padLen;
      }

      // at this point 'slength' should be a multiple of 'blen'
      assert(slength % blen == 0);

      lenLba += slength / blen;
    
    } while (run != NULL);
  }

  length_ = Msf(lenLba);

  slength = 0;
  for (run = subTracks_; run != NULL; run = run->next_) {
    run->start(slength); // set start position of sub-track
    slength += run->length();
  }


  // reset 'start_' if necessary
  if (start_.lba() >= length_.lba()) {
    start_ = Msf(0);
  }
}

// Sets logical start of track, everthing before start (if != 0) is taken
// as pre-gap. FIXME: Already set index marks are not updated.
// return: 0: OK
//         1: given start behind track end
int Track::start(Msf s)
{
  // A length of zero gets a pass for this test. The track length is
  // optional for the last track entry of the TOC file, because the
  // actual length of the track filename is not necessarily known at
  // this stage (mp3...). The actual length will be filled in later by
  // recomputeLength().
  if (s.lba() >= length_.lba() && (length_.lba() != 0)) {
    return 1;
  }

  start_ = s;

  return 0;
}

// Sets end of user area of track which is the start of the post-gap. 
// An index entry is created at this point. The pre-gap length must be
// set with 'start()' before this function can be called.
// return: 0: OK
//         1: start of post-gap is behind or at track end
//         2: post-gap within pre-gap
//         3: cannot create index mark, 98 index marks are already defined
int Track::end(Msf e)
{
  if (e.lba() >= length_.lba()) {
    return 1;
  }

  if (e.lba() != 0 && e.lba() <= start_.lba()) {
    return 2;
  }

  if (e.lba() != 0) {
    // add index mark for post-gap
    if (addIndex(Msf(e.lba() - start_.lba())) == 1) {
      // already 98 index increments defined
      return 3;
    }
  }

  end_ = e;

  return 0;
}


// Appends given index to index increment list.
// return: 0: OK
//         1: > 98 index increments 
//         2: index at or beyond track end
//         3: index at start
int Track::appendIndex(const Msf &index)
{
  if (nofIndices_ == 98) {
    return 1;
  }

  if (index.lba() >= (length_ - start_).lba()) {
    return 2;
  }

  if (index.lba() == 0) {
    return 3;
  }

  index_[nofIndices_] = index;
  nofIndices_ += 1;

  return 0;
}

// Adds index at given position
// return: 0: OK
//         1: > 98 index increments 
//         2: cannot add index at specified position
int Track::addIndex(const Msf &index)
{
  int i;
  if (nofIndices_ == 98) {
    return 1;
  }

  if (index.lba() >= (length_ - start_).lba()) {
    return 2;
  }

  if (index.lba() == 0) {
    return 2;
  }

  for (i = 0; i < nofIndices_; i++) {
    if (index.lba() == index_[i].lba())
      return 2;

    if (index.lba() < index_[i].lba())
      break;
  }

  if (i == nofIndices_) {
    // append index
    index_[nofIndices_] = index;
    nofIndices_ += 1; 
  }
  else {
    int pos = i;
    for (i = nofIndices_; i > pos; i--)
      index_[i] = index_[i - 1];

    index_[pos] = index;
    nofIndices_ += 1; 
  }
  
  return 0;
}

// Removes specified index.
// return 0: OK
//        1: index not found
int Track::removeIndex(int index)
{
  int i;

  if (index < 0 || index >= nofIndices_)
    return 1;

  
  for (i = index; i < nofIndices_ - 1; i++)
    index_[i] = index_[i + 1];

  nofIndices_ -= 1;

  return 0;
}

// returns index increment
Msf Track::getIndex(int i) const
{
  if (i >= nofIndices_ || i < 0) {
    return Msf(0);
  }
  else {
    return index_[i];
  }
}

int Track::check(int trackNr) const
{
  int ret = 0;

  if (length().lba() - start().lba() < Msf(0, 4, 0).lba()) {
    log_message(-1, "Track %d: Length is shorter than 4 seconds.", trackNr);
    ret = 1;
  }

  for (SubTrack* st = subTracks_; st; st = st->next_) {
    ret |= st->check(trackNr);
  }

  return ret;
}

bool Track::recomputeLength()
{
  for (SubTrack* st = subTracks_; st; st = st->next_) {
    if (st->length() == 0) {
      st->determineLength();
    }
  }

  update();
  return true;
}

// Sets ISRC code. Expected string: "CCOOOYYSSSSS"
//                 C: country code (ASCII)
//                 O: owner code (ASCII)
//                 Y: year ('0'-'9')
//                 S: serial number ('0'-'9')
// return: 0: OK
//         1: ilegal ISRC string
int Track::isrc(const char *isrc)
{
  int i;

  if (isrc == NULL) {
    isrcValid_ = 0;
    return 0;
  }

  if (strlen(isrc) != 12) {
    return 1;
  }

  for (i=0; i < 5; i++) {
    if (!(isdigit(isrc[i]) || isupper(isrc[i]))) {
      return 1;
    }
  }

  for (i = 5; i < 12; i++) {
    if (!isdigit(isrc[i])) {
      return 1;
    }
  }

  isrcCountry_[0] = isrc[0];
  isrcCountry_[1] = isrc[1];

  isrcOwner_[0] = isrc[2];
  isrcOwner_[1] = isrc[3];
  isrcOwner_[2] = isrc[4];


  // store BCD
  isrcYear_[0] = isrc[5] - '0';
  isrcYear_[1] = isrc[6] - '0';
  
  isrcSerial_[0] = isrc[7] - '0';
  isrcSerial_[1] = isrc[8] - '0';
  isrcSerial_[2] = isrc[9] - '0';
  isrcSerial_[3] = isrc[10] - '0';
  isrcSerial_[4] = isrc[11] - '0';
  
  isrcValid_ = 1;

  return 0;
}

const char *Track::isrc() const
{
  static char buf[13];
  
  if (!isrcValid_)
    return NULL;

  buf[0] = isrcCountry_[0];
  buf[1] = isrcCountry_[1];
  buf[2] = isrcOwner_[0];
  buf[3] = isrcOwner_[1];
  buf[4] = isrcOwner_[2];
  buf[5] = isrcYear_[0] + '0';
  buf[6] = isrcYear_[1] + '0';
  buf[7] = isrcSerial_[0] + '0';
  buf[8] = isrcSerial_[1] + '0';
  buf[9] = isrcSerial_[2] + '0';
  buf[10] = isrcSerial_[3] + '0';
  buf[11] = isrcSerial_[4] + '0';
  buf[12] = 0;

  return buf;
}

int Track::isPadded() const
{
  SubTrack *run;

  for (run = subTracks_; run != NULL; run = run->next_) {
    if (run->type() == SubTrack::PAD)
      return 1;
  }

  return 0;
}

// writes out track data in TOC file syntax
void Track::print(std::ostream &out, bool conversions) const
{
  SubTrack *st;
  const char *s;
  int i;

  out << "TRACK " << TrackData::mode2String(type());

  s = TrackData::subChannelMode2String(subChannelType());

  if (*s != 0)
    out << " " << s;

  out << std::endl;

  if (!copyPermitted())
    out << "NO ";
  out << "COPY" << std::endl;

  if (type() == TrackData::AUDIO) {
    if (!preEmphasis())
      out << "NO ";
    out << "PRE_EMPHASIS" << std::endl;

    if (audioType() == 0)
      out << "TWO_CHANNEL_AUDIO" << std::endl;
    else
      out << "FOUR_CHANNEL_AUDIO" << std::endl;
  
    if (isrcValid()) {
      out << "ISRC \"" << isrcCountry(0) << isrcCountry(1)
	  << isrcOwner(0) << isrcOwner(1) << isrcOwner(2)
	  << (char)(isrcYear(0) + '0') << (char)(isrcYear(1) + '0')
	  << (char)(isrcSerial(0) + '0') << (char)(isrcSerial(1) + '0')
	  << (char)(isrcSerial(2) + '0') << (char)(isrcSerial(3) + '0')
	  << (char)(isrcSerial(4) + '0') << "\"" << std::endl;
    }

    cdtext_.print(1, out);
  }


  for (st = subTracks_; st != NULL; st = st->next_) {
    st->print(out, conversions);
  }

  if (start_.lba() != 0) {
    out << "START " << start_.str() << std::endl;
  }

  for (i = 0; i < nofIndices_; i++) {
    out << "INDEX " << index_[i].str() << std::endl;
  }
}

void Track::collectFiles(std::set<std::string>& set)
{
  SubTrack* st;
  for (st = subTracks_; st != NULL; st = st->next_) {
    const char* f = st->filename();
    if (f)
      set.insert(f);
  }
}

void Track::markFileConversion(const char* src, const char* dst)
{
  SubTrack* st;
  for (st = subTracks_; st != NULL; st = st->next_) {
    const char* f = st->filename();
    if (f && strcmp(f, src) == 0) {
      st->effectiveFilename(dst);
    }
  }
}

bool Track::resolveFilename(const char* path)
{
  SubTrack* st;
  for (st = subTracks_; st != NULL; st = st->next_) {
    std::string rfilename;
    const char* f = st->filename();

    if (f) {
      // STDIN is a special case (stdin input), don't process it.
      if (strcmp(f, "STDIN") == 0)
        continue;

      if (::resolveFilename(rfilename, f, path)) {
        st->effectiveFilename(rfilename.c_str());
      } else {
        log_message(-2, "Could not find input file \"%s\".", f);
        return false;
      }
    }
  }

  return true;
}

// Locates 'SubTrack' that contains specified sample.
// return: found 'SubTrack' or 'NULL' if given sample is out of range
SubTrack *Track::findSubTrack(unsigned long sample) const
{
  SubTrack *run;

  if (audioCutMode()) {
    if (sample >= length_.samples()) 
      return NULL;
  }
  else {
    if (sample >=
	length_.lba() * TrackData::dataBlockSize(type(), subChannelType()))
      return NULL;
  }

  for (run = subTracks_;
       run != NULL && run->next_ != NULL;
       run = run->next_) {
    if (sample < run->next_->start())
      return run;
  }

  return run;
}


void Track::countSubTracks()
{
  SubTrack *run;

  nofSubTracks_ = 0;
  for (run = subTracks_; run != NULL; run = run->next_)
    nofSubTracks_++;
}

// move track start or index increment within track range
// return: 0: OK
//         1: track length would be shorter than 4 seconds
//         2: cannot cross index boundary
int Track::moveIndex(int index, long lba)
{
  int i;
  long rangeMin;
  long rangeMax;

  assert(index > 0 && index - 2 < nofIndices_);
  assert(lba >= 0 && index < length_.lba());

  if (index == 1) {
    if (nofIndices_ > 0 && lba >= start_.lba() + getIndex(0).lba())
      return 2;

    if (lba > length_.lba() - 4 * 75)
      return 1;

    // adjust index increments to new track start
    for (i = 0; i < nofIndices_; i++) {
      index_[i] = Msf(index_[i].lba() + start_.lba() - lba);
    }

    start_ = Msf(lba);

    return 0;
  }

  // adjust 'index' for index array access
  index -= 2;

  if (lba <= start_.lba())
    return 2;

  lba -= start_.lba();

  rangeMin = (index == 0 ? 0 : index_[index - 1].lba());
  rangeMax =
    (index == nofIndices_ - 1 ? length_.lba() - start_.lba() :
                                index_[index + 1].lba());

  if (lba > rangeMin && lba < rangeMax) {
    index_[index] = Msf(lba);
    return 0;
  }

  return 2;
}

TrackDataList *Track::removeToEnd(unsigned long sample)
{
  SubTrack *strun;
  SubTrack *store;
  int i;

  assert(sample > 0 && sample < length_.samples());

  strun = findSubTrack(sample);

  assert(strun != NULL);

  TrackDataList *list = new TrackDataList;

  if (sample == strun->start()) {
    // we don't have to split the TrackData object
    list->append(new TrackData(*strun));

    // cannot be the first SubTrack because 'sample' > 0
    strun->pred_->next_ = NULL;
    lastSubTrack_ = strun->pred_;
    
    store = strun;
    strun = strun->next_;
    delete store;
  }
  else {
    // split audio data object
    TrackData *part1, *part2;

    strun->split(sample - strun->start(), &part1, &part2);

    list->append(part2);

    store = new SubTrack(strun->type(), *part1);
    delete part1;

    if (strun->pred_ == NULL) {
      subTracks_ = store;
    }
    else {
      strun->pred_->next_ = store;
      store->pred_ = strun->pred_;
    }
    lastSubTrack_ = store;
    
    store = strun;
    strun = strun->next_;
    delete store;
  }

  
  while (strun != NULL) {
    list->append(new TrackData(*strun));

    store = strun;
    strun = strun->next_;
    delete store;
  }

  countSubTracks();
  update();

  checkConsistency();

  // adjust index increments
  for (i = 0; i < nofIndices_; i++) {
    if (index_[i].lba() + start_.lba() >= length_.lba()) {
      nofIndices_ = i;
      break;
    }
  }

  return list;
}

TrackDataList *Track::removeFromStart(unsigned long sample)
{
  SubTrack *strun;
  SubTrack *store, *start;
  int i;

  assert(sample > 0 && sample < length_.samples());

  TrackDataList *list = new TrackDataList;

  for (strun = subTracks_;
       strun != NULL && strun->next_ != NULL;
       strun = strun->next_) {
    if (sample < strun->next_->start())
      break;
    else
      list->append(new TrackData(*strun));
  }

  assert(strun != NULL);

  start = subTracks_;

  if (sample == strun->start()) {
    // we don't have to split the TrackData object
    // cannot be the first SubTrack because 'sample' > 0

    strun->pred_->next_ = NULL;
    subTracks_ = strun;
    subTracks_->pred_ = NULL;
  }
  else {
    // split actual sub track

    TrackData *part1, *part2;

    strun->split(sample - strun->start(), &part1, &part2);
    
    list->append(part1);

    store = new SubTrack(strun->type(), *part2);
    delete part2;

    store->next_ = strun->next_;
    if (store->next_ != NULL)
      store->next_->pred_ = store;

    strun->next_ = NULL;
    subTracks_ = store;

    if (strun == lastSubTrack_)
      lastSubTrack_ = store;
  }

  // remove unlinked sub tracks
  while (start != NULL) {
    store = start;
    start = start->next_;

    delete store;
  }
  
  countSubTracks();
  update();

  checkConsistency();

  // adjust index increments
  for (i = 0; i < nofIndices_; i++) {
    if (index_[i].lba() + start_.lba() >= length_.lba()) {
      nofIndices_ = i;
      break;
    }
  }

  return list;
}
  

void Track::prependTrackData(const TrackDataList *list)
{
  SubTrack *start = NULL;
  SubTrack *last = NULL;
  SubTrack *ent;
  const TrackData *run;

  if (list->count() == 0)
    return;

  for (run = list->first(); run != NULL; run = list->next()) {
    ent = new SubTrack(SubTrack::DATA, *run);
    if (last == NULL) {
      start = ent;
    }
    else {
      last->next_ = ent;
      ent->pred_ = last;
    }
    last = ent;
  }

  if (subTracks_ == NULL) {
    subTracks_ = start;
    lastSubTrack_ = last;
  }
  else {
    last->next_ = subTracks_;
    subTracks_->pred_ = last;
    subTracks_ = start;
  }

  mergeSubTracks(); // this will also update the sub track counter
  update();

  checkConsistency();
}

void Track::appendTrackData(const TrackDataList *list)
{
  SubTrack *start = NULL;
  SubTrack *last = NULL;
  SubTrack *ent;
  const TrackData *run;

  if (list->count() == 0)
    return;

  for (run = list->first(); run != NULL; run = list->next()) {
    ent = new SubTrack(SubTrack::DATA, *run);
    if (last == NULL) {
      start = ent;
    }
    else {
      last->next_ = ent;
      ent->pred_ = last;
    }
    last = ent;
  }

  if (lastSubTrack_ != NULL) {
    if (lastSubTrack_->type() == SubTrack::PAD)
      lastSubTrack_->type_ = SubTrack::DATA;

    lastSubTrack_->next_ = start;
    start->pred_ = lastSubTrack_;
    lastSubTrack_ = last;
  }
  else {
    subTracks_ = start;
    lastSubTrack_ = last;
  }

  mergeSubTracks(); // this will also update the sub track counter
  update();

  checkConsistency();
}  

void Track::appendTrackData(const Track *track)
{
  SubTrack *run, *ent;

  for (run = track->subTracks_; run != NULL; run = run->next_) {
    ent = new SubTrack(*run);

    if (lastSubTrack_ == NULL) {
      subTracks_ = ent;
    }
    else {
      lastSubTrack_->next_ = ent;
      ent->pred_ = lastSubTrack_;
    }
    lastSubTrack_ = ent;
  }

  mergeSubTracks(); // this will also update the sub track counter
  update();

  checkConsistency();
}

TrackDataList *Track::removeTrackData(unsigned long start, unsigned long end)
{
  SubTrack *run;
  TrackData *part1, *part2, *part3;
  unsigned long plen;

  if (start > end || end >= length_.samples())
    return NULL;

  SubTrack *startSt = findSubTrack(start);
  SubTrack *endSt = findSubTrack(end);

  assert(startSt != NULL);
  assert(endSt != NULL);

  TrackDataList *list = new TrackDataList;

  if (startSt == endSt) {
    if (start == startSt->start() &&
	end == startSt->start() + startSt->length() - 1) {
      // remove complete sub track
      list->append(new TrackData(*startSt));
    }
    else if (start == startSt->start()) {
      // remove first part of sub track
      startSt->split(end + 1 - startSt->start(), &part1, &part2);
      list->append(part1);
      insertSubTrackAfter(startSt, new SubTrack(startSt->type(), *part2));
      delete part2;
    }
    else if (end == startSt->start() + startSt->length() - 1) {
      // remove last part of sub track
      startSt->split(start - startSt->start(), &part1, &part2);
      list->append(part2);
      insertSubTrackAfter(startSt, new SubTrack(startSt->type(), *part1));
      delete part1;
    }
    else {
      // remove middle part of sub track
      startSt->split(start - startSt->start(), &part1, &part2);

      insertSubTrackAfter(startSt->pred_,
			  new SubTrack(startSt->type(), *part1));
      plen = part1->length();
      delete part1;

      part2->split(end + 1 - startSt->start() - plen, &part1, &part3);
      list->append(part1);

      insertSubTrackAfter(startSt, new SubTrack(startSt->type(), *part3));
      delete part3;
      delete part2;
    }

    delete removeSubTrack(startSt);
  }
  else {
    if (start == startSt->start()) {
      // remove complete sub track
      list->append(new TrackData(*startSt));
    }
    else {
      startSt->split(start - startSt->start(), &part1, &part2);
      list->append(part2);
      insertSubTrackAfter(startSt->pred_,
			  new SubTrack(startSt->type(), *part1));
      delete part1;
    }

    for (run = startSt->next_; run != endSt; run = run->next_)
      list->append(new TrackData(*(startSt->next_)));

    if (end == endSt->start() + endSt->length() - 1) {
      // remove complete sub track
      list->append(new TrackData(*endSt));
    }
    else {
      endSt->split(end + 1 - endSt->start(), &part1, &part2);
      list->append(part1);
      insertSubTrackAfter(endSt, new SubTrack(endSt->type(), *part2));
      delete part2;
    }

    while (startSt->next_ != endSt)
      delete removeSubTrack(startSt->next_);

    delete removeSubTrack(startSt);
    delete removeSubTrack(endSt);
  }


  // adjust index marks
  unsigned long len;
  long preGapAdj = 0;
  long slba = 0;
  long elba = 0;
  long indexMove = 0;
  long indexAdj = 0;
  int i;

  if (start < start_.samples()) {
    if (end < start_.samples()) {
      len = end - start + 1;
      slba = -1;
      elba = -1;
    }
    else {
      len = start_.samples() - start;
      elba = (end - start_.samples()) / SAMPLES_PER_BLOCK;
      slba = 0;
      indexMove = (end - start_.samples() + 1) / SAMPLES_PER_BLOCK;
      if (((end - start_.samples() + 1) % SAMPLES_PER_BLOCK) != 0) 
	indexMove += 1;
    }

    preGapAdj = len / SAMPLES_PER_BLOCK;

    if ((len % SAMPLES_PER_BLOCK) != 0)
      indexAdj = 1;
  }
  else {
    slba = (start - start_.samples()) / SAMPLES_PER_BLOCK;
    if (((start - start_.samples()) % SAMPLES_PER_BLOCK) != 0)
      slba += 1;

    elba = (end - start_.samples()) / SAMPLES_PER_BLOCK;

    indexMove = (end - start + 1) / SAMPLES_PER_BLOCK;
    if (((end - start + 1) % SAMPLES_PER_BLOCK) != 0)
      indexMove += 1;
  }

  
  i = 0;

  while (i < nofIndices_) {
    if (index_[i].lba() >= slba && index_[i].lba() <= elba) {
      removeIndex(i);
      continue;
    }
    else if (index_[i].lba() > elba) {
      if (index_[i].lba() - indexMove <= 0) {
	removeIndex(i);
	continue;
      }
      else {
	index_[i] = Msf(index_[i].lba() - indexMove);
      }

      if (i > 0 && index_[i - 1].lba() == index_[i].lba()) {
	removeIndex(i);
	continue;
      }
    }

    if (index_[i].lba() - indexAdj <= 0) {
      removeIndex(i);
      continue;
    }
    else {
      index_[i] = Msf(index_[i].lba() - indexAdj);
    }

    i++;
  }

  // adjust pre-gap length
  start_ = Msf(start_.lba() - preGapAdj);
  

  mergeSubTracks(); // this will also update the sub track counter
  update();

  checkConsistency();

  return list;
}


void Track::insertTrackData(unsigned long pos, const TrackDataList *list)
{
  const TrackData *run;
  TrackData *part1, *part2;
  SubTrack *st;
  unsigned long len;
  long blen;
  int i;

  if (list == NULL || list->first() == NULL || list->length() == 0)
    return;


  if (pos >= length_.samples()) {
    // append data
    for (run = list->first(); run != NULL; run = list->next()) {
      insertSubTrackAfter(lastSubTrack_, new SubTrack(SubTrack::DATA, *run));
    }
  }
  else {
    st = findSubTrack(pos);

    assert(st != NULL);

    len = list->length();

    if (pos == st->start()) {
      for (run = list->first(); run != NULL; run = list->next()) {
	insertSubTrackAfter(st->pred_, new SubTrack(SubTrack::DATA, *run));
      }
    }
    else {
      st->split(pos - st->start(), &part1, &part2);
      
      insertSubTrackAfter(st->pred_, new SubTrack(SubTrack::DATA, *part1));
      insertSubTrackAfter(st, new SubTrack(st->type(), *part2));
      delete part1;
      delete part2;
      
      for (run = list->first(); run != NULL; run = list->next()) {
	insertSubTrackAfter(st->pred_, new SubTrack(SubTrack::DATA, *run));
      }
      
      delete removeSubTrack(st);
    }

    blen = len / SAMPLES_PER_BLOCK;

    if (pos <= start_.samples()) {
      start_ = Msf(start_.lba() + blen);
    }
    else {
      for (i = 0; i < nofIndices_; i++) {
	if (index_[i].samples() >= pos)
	  index_[i] = Msf(index_[i].lba() + blen);
      }
    }
  }

  mergeSubTracks(); // this will also update the sub track counter
  update();

  checkConsistency();

  return;
}
    
void Track::mergeSubTracks()
{
  SubTrack *run = subTracks_;
  SubTrack *newSubTrack;
  TrackData *mergedData;


  while (run != NULL && run->next_ != NULL) {
    if (run->type() == run->next_->type() &&
	(mergedData = run->merge(run->next_)) != NULL) {
      newSubTrack = new SubTrack(run->type(), *mergedData);
      delete mergedData;

      newSubTrack->next_ =  run->next_->next_;
      if (newSubTrack->next_ != NULL)
	newSubTrack->next_->pred_ = newSubTrack;

      if (run->pred_ == NULL) {
	subTracks_ = newSubTrack;
      }
      else {
	run->pred_->next_ = newSubTrack;
	newSubTrack->pred_ = run->pred_;
      }

      if (run->next_ == lastSubTrack_)
	lastSubTrack_ = newSubTrack;

      delete run->next_;
      delete run;
      run = newSubTrack;
    }
    else {
      run = run->next_;
    }
  }

  countSubTracks();
}


void Track::checkConsistency()
{
  SubTrack *run, *last = NULL;
  long cnt = 0;

  for (run = subTracks_; run != NULL; last = run, run = run->next_) {
    cnt++;
    if (run->pred_ != last) 
      log_message(-3, "Track::checkConsistency: wrong pred pointer.");
  }

  if (last != lastSubTrack_)
    log_message(-3, "Track::checkConsistency: wrong last pointer.");

  if (cnt != nofSubTracks_)
    log_message(-3, "Track::checkConsistency: wrong sub track counter.");
}

// Inserts 'newSubTrack' after existing sub track 'subTrack'. If 'subTrack'
// is NULL it will be prepended.
void Track::insertSubTrackAfter(SubTrack *subTrack, SubTrack *newSubTrack)
{
  if (subTrack == NULL) {
    if (subTracks_ == NULL) {
      subTracks_ = lastSubTrack_ = newSubTrack;
      newSubTrack->next_ = newSubTrack->pred_ = NULL;
    }
    else {
      newSubTrack->next_ = subTracks_;
      subTracks_->pred_ = newSubTrack;

      newSubTrack->pred_ = NULL;
      subTracks_ = newSubTrack;
    }
  }
  else {
    newSubTrack->next_ = subTrack->next_;
    if (newSubTrack->next_ != NULL) {
      newSubTrack->next_->pred_ = newSubTrack;
    }
    else {
      lastSubTrack_ = newSubTrack;
    }

    subTrack->next_ = newSubTrack;
    newSubTrack->pred_ = subTrack;
  }

  nofSubTracks_ += 1;

  checkConsistency();
}

// Removes given sub track from list. Returns the removed sub track.
SubTrack *Track::removeSubTrack(SubTrack *subTrack)
{
  if (subTrack->pred_ == NULL) {
    assert(subTrack == subTracks_);

    subTracks_ = subTrack->next_;

    if (subTracks_ != NULL) {
      subTracks_->pred_ = NULL;
    }
    else {
      lastSubTrack_ = NULL;
    }
  }
  else {
    subTrack->pred_->next_ = subTrack->next_;

    if (subTrack->next_ != NULL) {
      subTrack->next_->pred_ = subTrack->pred_;
    }
    else {
      lastSubTrack_ = subTrack->pred_;
    }
  }

  nofSubTracks_ -= 1;

  subTrack->next_ = subTrack->pred_ = NULL;

  checkConsistency();

  return subTrack;
}


// Fills provided buffer with an audio block that contains zero data
// encoded with given mode.
// encMode: encoding mode, see 'TrackReader::readBlock()'
// mode: sector mode
// smode: sub-channel mode
// lba : absolute address of sector
// buf : caller provided buffer that is filled with at most 2448 bytes

void Track::encodeZeroData(int encMode, TrackData::Mode mode,
			   TrackData::SubChannelMode smode,
			   long lba, unsigned char *buf)
{
  long subChanLen = TrackData::subChannelSize(smode);
  
  // we won't have to calculate P and Q parity for zero R-W sub-channels
  // because 0s have no impact on the parity
    
  if (encMode == 0) {
    memset(buf, 0, AUDIO_BLOCK_LEN + subChanLen);

    switch (mode) {
    case TrackData::AUDIO:
      break;
    case TrackData::MODE0:
      lec_encode_mode0_sector(lba, buf);
      lec_scramble(buf);
      break;
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
      lec_encode_mode1_sector(lba, (u_int8_t *)buf);
      lec_scramble(buf);
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_RAW:
      lec_encode_mode2_sector(lba, (u_int8_t *)buf);
      lec_scramble(buf);
      break;
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM_MIX: // encode as form 1
      lec_encode_mode2_form1_sector(lba, (u_int8_t *)buf);
      lec_scramble(buf);
      break;
    case TrackData::MODE2_FORM2:
      // setup sub header
      buf[16+2] = 0x20;
      buf[16+6] = 0x20;
      lec_encode_mode2_form2_sector(lba, (u_int8_t *)buf);
      lec_scramble(buf);
      break;
    }
  }
  else if (encMode == 1) {
    switch (mode) {
    case TrackData::AUDIO:
      memset(buf, 0, AUDIO_BLOCK_LEN + subChanLen);
      break;
    case TrackData::MODE0:
      memset(buf, 0, MODE0_BLOCK_LEN + subChanLen);
      break;
    case TrackData::MODE1:
    case TrackData::MODE1_RAW:
      memset(buf, 0, MODE1_BLOCK_LEN + subChanLen);
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_RAW:
    case TrackData::MODE2_FORM_MIX:
      memset(buf, 0, MODE2_BLOCK_LEN + subChanLen);
      break;
    case TrackData::MODE2_FORM1:
      memset(buf, 0, MODE2_BLOCK_LEN + subChanLen);
      break;
    case TrackData::MODE2_FORM2:
      memset(buf, 0, MODE2_BLOCK_LEN + subChanLen);
      // setup sub header
      buf[2] = 0x20;
      buf[6] = 0x20;
      break;
    }
  }
  else {
    log_message(-3, "Illegal sector encoding mode in 'Track::encodeZeroData()'.");
  }
}

void Track::addCdTextItem(CdTextItem *item)
{
  assert(CdTextItem::isTrackPack(item->packType()));

  cdtext_.add(item);
}

void Track::removeCdTextItem(CdTextItem::PackType type, int blockNr)
{
  cdtext_.remove(type, blockNr);
}

TrackReader::TrackReader(const Track *t) : reader(NULL)
{
  track_ = t;

  readPos_ = 0;
  readPosSample_ = 0;
  readSubTrack_ = NULL;
  open_ = 0;
  
  subChanDelayLineIndex_ = 0;
}

TrackReader::~TrackReader()
{
  if (open_) {
    closeData();
  }

  track_ = NULL;
  readSubTrack_ = NULL;
}

void TrackReader::init(const Track *t)
{
  if (open_) {
    closeData();
  }

  track_ = t;
  readSubTrack_ = NULL;
}  

// initiates reading track
// return: 0: OK
//         1: data file could not be opened
//         2: could not seek to start position
int TrackReader::openData()
{
  assert(open_ == 0);
  assert(track_ != NULL);

  int ret = 0;
  //int i;

  open_ = 1;
  readPos_ = 0;
  readPosSample_ = 0;

  readSubTrack_ = track_->subTracks_;

  reader.init(readSubTrack_);

  subChanDelayLineIndex_ = 0;
  /*
  for(i = 0; i < MAX_SUB_DEL; i++)
    memset(subChanDelayLine_[i], 0, 24);
  */

  if (readSubTrack_ != NULL) {
    ret = reader.openData();
  }

  return ret;
}

void TrackReader::closeData()
{
  if (open_) {
    open_ = 0;
    readPos_ = 0;
    readPosSample_ = 0;
    readSubTrack_ = NULL;
  
    reader.closeData();
  }
}


long TrackReader::readData(int encodingMode, int subChanEncodingMode,
			   long lba, char *buf, long len)
{
  long err = 0;
  long b;
  long offset; 

  assert(open_ != 0);

  if (readPos_ + len > track_->length_.lba()) {
    if ((len = track_->length_.lba() - readPos_) <= 0) {
      return 0;
    }
  }

  for (b = 0; b < len; b++) {
    if ((offset = readBlock(encodingMode, subChanEncodingMode, lba,
			    (Sample*)buf)) == 0) {
      err = 1;
      break;
    }
    buf += offset;
    lba++;
  }

  readPos_ += b;

  return err == 0 ? b : -1;
}

// Reads one block from sub-tracks and performs the data encoding for the
// block.
// encodingMode: conrols how the sector data is encoded
//               0: returned data is always an audio block (2352 bytes),
//                  data blocks are completely encoded
//               1: data is returned mostly unencoded, the block size
//                  depends on the actual sub-track mode, MODE2_FORM1 and
//                  MODE2_FORM2 blocks are extended by the sub header and
//                  filled with 0 bytes to match the MODE2 block size
//                  (2336 bytes).
// subChanEncodingMode: conrols how the R-W sub-channel data is encoded
//                      0: plain R-W data
//                      1: generate Q and P parity and interleave
// lba: Logical block address that must be encoded into header of data blocks
// Return: 0 if error occured, else length of block that has been filled
//         
int TrackReader::readBlock(int encodingMode, int subChanEncodingMode,
			   long lba, Sample *buf)
{
  TrackData::Mode dataMode; // current data mode of sub-track
  TrackData::SubChannelMode subChanMode; // current sub-channel mode of sub-track
  long actLen;
  long count; // amount of data the must be read from sub track
  long blen;
  long nread = 0;
  long offset = 0; // block length of returned data
  long subChannelDataLen;
  char subChanData[MAX_SUBCHANNEL_LEN];


  while (reader.readLeft() == 0) {
    // skip to next sub track with available data
    readSubTrack_ = readSubTrack_->next_;
    // next sub-track must exist since requested length matches available data
    assert(readSubTrack_ != NULL); 

    reader.init(readSubTrack_);
    if (reader.openData() != 0) {
      return 0;
    }
  }

  dataMode = readSubTrack_->mode(); // current data mode
  subChanMode = readSubTrack_->subChannelMode(); // current sub-channel mode

  if (track_->audioCutMode()) {
    count = SAMPLES_PER_BLOCK;
    blen = AUDIO_BLOCK_LEN;
  }
  else {
    count = TrackData::dataBlockSize(dataMode, subChanMode);
    blen = count;
  }

  subChannelDataLen = TrackData::subChannelSize(subChanMode);

  char *dataBuf = (char *)buf;

  if (encodingMode == 0) {
    offset = AUDIO_BLOCK_LEN;

    switch (dataMode) {
    case TrackData::AUDIO:
    case TrackData::MODE1_RAW:
    case TrackData::MODE2_RAW:
      break;
    case TrackData::MODE0:
    case TrackData::MODE1:
    case TrackData::MODE2:
    case TrackData::MODE2_FORM_MIX:
      dataBuf += 16;
      break;
    case TrackData::MODE2_FORM1:
      memset(dataBuf + 16, 0, 8); // clear sub-header
      dataBuf += 24;
      break;
    case TrackData::MODE2_FORM2:
      memset(dataBuf + 16, 0, 8); // clear sub-header
      dataBuf[16+2] = 0x20;
      dataBuf[16+6] = 0x20;
      dataBuf += 24;
      break;
    }
  }
  else if (encodingMode == 1) {
    switch (dataMode) {
    case TrackData::AUDIO:
      offset = AUDIO_BLOCK_LEN;
      break;
    case TrackData::MODE0:
      offset = MODE0_BLOCK_LEN;
      break;
    case TrackData::MODE1:
      offset = MODE1_BLOCK_LEN;
      break;
    case TrackData::MODE1_RAW:
      offset = MODE1_BLOCK_LEN;
      break;
    case TrackData::MODE2:
    case TrackData::MODE2_FORM_MIX:
      offset = MODE2_BLOCK_LEN;
      break;
    case TrackData::MODE2_FORM1:
    case TrackData::MODE2_FORM2:
      offset = MODE2_BLOCK_LEN;
      memset(dataBuf, 0, MODE2_BLOCK_LEN);
      dataBuf += 8; // reserve space for sub-header
      break;
    case TrackData::MODE2_RAW:
      offset = MODE2_BLOCK_LEN;
      break;
    }
  }

  // gather one block of data, block length depends on 'dataMode'
  while (count > 0) {
    if (track_->audioCutMode())
      actLen = reader.readData(buf + nread, count);
    else 
      actLen = reader.readData((Sample *)(dataBuf + nread), count);

    if (actLen < 0) // indicates read error
      return 0;

    if (actLen != count) {
      // end of data in sub-track reached, skip to next sub-track
      readSubTrack_ = readSubTrack_->next_;

      // next sub-track must exist since requested length match available data
      assert(readSubTrack_ != NULL); 

      // mode of next sub-track must match, this is ensured in 'update()'.
      assert(readSubTrack_->mode() == dataMode);
      assert(readSubTrack_->subChannelMode() == subChanMode);

      reader.init(readSubTrack_);
      if (reader.openData() != 0) {
	return 0;
      }
    }

    count -= actLen;
    nread += actLen;
  }

  if (subChannelDataLen > 0) {
    char *subChannelSourceBuf = dataBuf + blen - subChannelDataLen;

    // save sub-channel data for later processing
    memcpy(subChanData, subChannelSourceBuf, subChannelDataLen);
    memset(subChannelSourceBuf, 0, subChannelDataLen);
  }

  unsigned char *encBuf = (unsigned char *)buf;

  if (encodingMode == 0) {
    // encode data block according to 'dataMode'

    switch (dataMode) {
    case TrackData::AUDIO:
      break;
    case TrackData::MODE0:
      lec_encode_mode0_sector(lba, encBuf);
      lec_scramble(encBuf);
      break;
    case TrackData::MODE1:
      lec_encode_mode1_sector(lba, encBuf);
      lec_scramble(encBuf);
      break;
    case TrackData::MODE1_RAW:
      {
	Msf m(lba);

	if (int2bcd(m.min()) != encBuf[12] ||
	    int2bcd(m.sec()) != encBuf[13] ||
	    int2bcd(m.frac()) != encBuf[14]) {
	  // sector address mismatch -> rebuild L-EC since it covers the header
	  lec_encode_mode1_sector(lba, encBuf);
	}

	lec_scramble(encBuf);
      }
      break;
    case TrackData::MODE2:
      lec_encode_mode2_sector(lba, encBuf);
      lec_scramble(encBuf);
      break;
    case TrackData::MODE2_FORM1:
      lec_encode_mode2_form1_sector(lba, encBuf);
      lec_scramble(encBuf);
      break;
    case TrackData::MODE2_FORM2:
      lec_encode_mode2_form2_sector(lba, encBuf);
      lec_scramble(encBuf);
      break;
    case TrackData::MODE2_FORM_MIX:
      if ((encBuf[16+2] & 0x20) != 0)
	lec_encode_mode2_form2_sector(lba, encBuf);
      else
	lec_encode_mode2_form1_sector(lba, encBuf);

      lec_scramble(encBuf);
      break;
    case TrackData::MODE2_RAW:
      {
	Msf m(lba);

	// L-EC does not cover sector header so it is relocatable
	// just update the sector address in the header
	encBuf[12] = int2bcd(m.min());
	encBuf[13] = int2bcd(m.sec());
	encBuf[14] = int2bcd(m.frac());

	lec_scramble(encBuf);
      }
      break;
    }
  }
  else if (encodingMode == 1) {
    switch (dataMode) {
    case TrackData::MODE2_FORM2:
      // add sub header for data form 2 sectors
      encBuf[2] = 0x20;
      encBuf[6] = 0x20;
      break;
    case TrackData::MODE1_RAW:
      // strip off sync and header
      memmove(encBuf, encBuf + 16, MODE1_BLOCK_LEN);
      break;
    case TrackData::MODE2_RAW:
      // strip off sync and header
      memmove(encBuf, encBuf + 16, MODE2_BLOCK_LEN);
      break;

    default:
      break;
    }
  }

  // encode R-W sub-channel data
  switch (subChanMode) {
  case TrackData::SUBCHAN_NONE:
    break;
  case TrackData::SUBCHAN_RW:
    if (subChanEncodingMode == 1) {
      /*do_encode_sub((unsigned char*)subChanData, 1, 1,
		    &subChanDelayLineIndex_, subChanDelayLine_);
      */
    }
    break;

  case TrackData::SUBCHAN_RW_RAW:
    if (subChanEncodingMode == 0) {
      // to be done
    }
    break;
  }
  
  if (subChannelDataLen > 0) {
    char *subChannelTargetBuf = (char *)buf + offset;
    
    memcpy(subChannelTargetBuf, subChanData, subChannelDataLen);
  }
 
  return (offset + subChannelDataLen);
}

long TrackReader::readTrackData(Sample *buf, long len)
{
  long actLen;
  long count = len;
  long nread = 0;

  while (count > 0) {
    actLen = reader.readData(buf + nread, count);

    if (actLen < 0) {
      return -1;
    }

    if (actLen != count) {
      // end of audio data in sub-track reached, skip to next sub-track
      readSubTrack_ = readSubTrack_->next_;

      // next sub-track must exists since requested length match available data
      assert(readSubTrack_ != NULL); 

      reader.init(readSubTrack_);
      if (reader.openData() != 0) {
	return -1;
      }
    }

    count -= actLen;

    if (track_->audioCutMode())
      nread += actLen;
    else
      nread += actLen / SAMPLE_LEN;
  }

  return len;
}

// Seeks to specified sample.
// return: 0: OK
//        10: sample out of range
//        return code of 'TrackData::openData()'
int TrackReader::seekSample(unsigned long sample)
{
  int ret;
  unsigned long offset;

  assert(open_ != 0);

  if (track_->audioCutMode() == 0) {
    // all lengths are in byte units -> convert requested sample to a byte
    // offset

    // first determine the block which contains the requested sample
    unsigned long block = sample / SAMPLES_PER_BLOCK;

    // byte offset into block
    unsigned long boffset = (sample % SAMPLES_PER_BLOCK) * SAMPLE_LEN;
    

    unsigned long blen = TrackData::dataBlockSize(track_->type(),
						  track_->subChannelType());

    offset = block * blen + boffset;
  }
  else {
    offset = sample;
  }

  // find sub track containing 'sample'
  SubTrack *st = track_->findSubTrack(offset);

  if (st == NULL) 
    return 10;

  // open sub track if necessary
  if (readSubTrack_ != st) {
    readSubTrack_ = st;
    reader.init(readSubTrack_);

    if ((ret = reader.openData()) != 0) 
      return ret;
  }

  assert(offset >= readSubTrack_->start());

  unsigned long stOffset = offset - readSubTrack_->start();

  // seek in sub track
  if ((ret = reader.seekSample(stOffset)) != 0)
    return ret;

  readPosSample_ = sample;

  return 0;
}

long TrackReader::readSamples(Sample *buf, long len)
{
  long ret;
  long i;
  assert(open_ != 0);

  if (readPosSample_ + (unsigned long)len > track_->length_.samples()) {
    if ((len = track_->length_.samples() - readPosSample_) <= 0) {
      return 0;
    }
  }

  if (track_->type() == TrackData::AUDIO) {
    if (track_->audioCutMode() == 1) {
      if ((ret = readTrackData(buf, len)) > 0) {
	readPosSample_ += ret;
      }
    }
    else {
      long subChanLen = TrackData::subChannelSize(track_->subChannelType());
      char subChanBuf[MAX_SUBCHANNEL_LEN];
      ret = 0;
      
      while (len > 0) {
	long burst =
	  SAMPLES_PER_BLOCK - (readPosSample_ % SAMPLES_PER_BLOCK);
	int fullBurst = 0;
	
	if (burst > len)
	  burst = len;
	else
	  fullBurst = 1;
	
	if (readTrackData(buf, burst * SAMPLE_LEN) <= 0)
	  return -1;
	
	buf += burst;
	
	len -= burst;
	readPosSample_ += burst;
	ret += burst;
	
	if (subChanLen > 0 && fullBurst &&
	    (unsigned long)readPosSample_ < track_->length_.samples()) {
	  if (readTrackData((Sample*)subChanBuf, subChanLen) < 0)
	    return -1;
	}
      }
    }
  }
  else {
    for (i = 0; i < len; i++) {
      buf[i].left(16000);
      buf[i].right(16000);
    }
    readPosSample_ += len;
    ret = len;
  }
  
  return ret;
}

const char* TrackReader::curFilename()
{
  const TrackData* td = reader.trackData();

  if (td)
    return td->filename();
  else
    return NULL;
}

SubTrackIterator::SubTrackIterator(const Track *t)
{
  track_ = t;
  iterator_ = NULL;
}


SubTrackIterator::~SubTrackIterator()
{
  track_ = NULL;
  iterator_ = NULL;
}

const SubTrack *SubTrackIterator::first()
{
  iterator_ = track_->subTracks_;

  return next();
}

const SubTrack *SubTrackIterator::next()
{
  if (iterator_ != NULL) {
    SubTrack *s = iterator_;

    iterator_ = iterator_->next_;
    return s;
  }
  else {
    return NULL;
  }
}


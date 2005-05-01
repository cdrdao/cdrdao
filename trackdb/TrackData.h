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


#ifndef __TRACKDATA_H__
#define __TRACKDATA_H__

#include <iostream>
#include <string>
#include <set>

#include "Sample.h"

#define AUDIO_BLOCK_LEN 2352
#define MODE0_BLOCK_LEN 2336
#define MODE1_BLOCK_LEN 2048
#define MODE2_BLOCK_LEN 2336

#define MODE2_FORM1_DATA_LEN 2048
#define MODE2_FORM2_DATA_LEN 2324

#define PQ_SUBCHANNEL_LEN 16
#define PW_SUBCHANNEL_LEN 96
#define MAX_SUBCHANNEL_LEN 96

#define SAMPLE_LEN 4

class TrackData {
public:
  enum Mode { AUDIO, MODE0, MODE1, MODE2, MODE2_FORM1, MODE2_FORM2,
              MODE2_FORM_MIX, MODE1_RAW, MODE2_RAW };

  enum SubChannelMode { SUBCHAN_NONE, SUBCHAN_RW, SUBCHAN_RW_RAW };

  enum Type { DATAFILE, ZERODATA, STDIN, FIFO };
  
  enum FileType { RAW, WAVE, MP3, OGG };

  // creates an audio mode file entry
  TrackData(const char *filename, long offset, unsigned long start,
	    unsigned long length);
  TrackData(const char *filename, unsigned long start, unsigned long length);

  // creates a zero audio entry
  TrackData(unsigned long length);

  // creates a zero data entry with given mode
  TrackData(Mode, SubChannelMode, unsigned long length);

  // creates a file entry with given mode
  TrackData(Mode, SubChannelMode, const char *filename, long offset,
	    unsigned long length);

  // create a fifo entry with given mode
  TrackData(Mode, SubChannelMode, const char *filename, unsigned long length);

  // copy constructor
  TrackData(const TrackData &);

  ~TrackData();

  Type type() const                        { return type_; }
  Mode mode() const                        { return mode_; }
  SubChannelMode subChannelMode() const    { return subChannelMode_; }
  int audioCutMode() const                 { return audioCutMode_; }

  const char *filename() const             { return filename_; }
  unsigned long startPos() const           { return startPos_; }
  unsigned long length() const;

  // sets/returns flag for swapping expected byte order of audio samples
  void swapSamples(int f)                  { swapSamples_ = f != 0 ? 1 : 0; }
  int swapSamples() const                  { return swapSamples_; }

  int determineLength();
  int check(int trackNr) const;

  void effectiveFilename(const char*);

  void split(unsigned long, TrackData **part1, TrackData **part2);
  TrackData *merge(const TrackData *) const;

  void print(std::ostream &, bool conversions = false) const;

  static int checkAudioFile(const char *fn, unsigned long *length);
  static int waveLength(const char *filename, long offset, long *hdrlen,
			unsigned long *datalen = 0);
  static int audioDataLength(const char *fname, long offset,
			     unsigned long *length);
  static FileType audioFileType(const char *filename);
  static int dataFileLength(const char *fname, long offset,
			    unsigned long *length);

  static unsigned long dataBlockSize(Mode, SubChannelMode);
  static unsigned long subChannelSize(SubChannelMode);

  static const char *mode2String(Mode);
  static const char *subChannelMode2String(SubChannelMode);

private:
  int audioCutMode_; /* defines if audio cut mode is requested, if yes
			all lengths are in sample units and sub-channel
			data is not allowed
		     */
  Type type_; // type of data (file, stdin, zero)
  Mode mode_; // data mode for recording (audio, mode0, mode1, ...)
  SubChannelMode subChannelMode_; // sub-channel mode

  FileType fileType_; // only for audio mode data, type of file (raw, wave)

  char *filename_;    // used for object type 'FILE'
  char *effFilename_; // effective filename (absolute path or converted file)

  long offset_; // byte offset into file

  unsigned long length_; // length of data in samples (for audio data) or bytes

  // only used for audio files:
  unsigned long startPos_; // starting sample within file, 
                           // used for object type 'FILE', mode 'AUDIO'
  int swapSamples_;        // 1 if expected byte order of audio samples should
                           // be swapped

  friend class TrackDataReader;

  void init(const char *filename, long offset, unsigned long start,
	    unsigned long length);
  void init(Mode, SubChannelMode, const char *filename, long offset,
	    unsigned long length);
  

};

class TrackDataReader {
public:
  TrackDataReader(const TrackData * = 0);
  ~TrackDataReader();

  void init(const TrackData *);

  int openData();
  void closeData();
  long readData(Sample *buffer, long len);
  int seekSample(unsigned long sample);

  const TrackData* trackData() const { return trackData_; }

  // returns number of bytes/samples that are left for reading
  unsigned long readLeft() const;

private:
  const TrackData *trackData_;

  int open_;  // 1: data can be retrieved with 'readData()', 'fd_' is valid if
              // object type is 'FILE'

  int fd_;         // used for object type 'FILE'
  unsigned long readPos_; // actual read position (samples or bytes)
                          // (0 .. length_-1)

  long headerLength_; // length of audio file header

  int readUnderRunMsgGiven_;
};

#endif

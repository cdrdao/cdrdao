/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999  Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: TrackData.h,v $
 * Revision 1.1.1.1  2000/02/05 01:32:33  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.9  1999/04/02 20:36:21  mueller
 * Created implementation class that contains all mutual member data.
 *
 * Revision 1.8  1999/03/27 19:49:29  mueller
 * Added data file support.
 *
 * Revision 1.7  1999/01/24 15:59:52  mueller
 * Added static member functions 'waveLength()', 'audioDataLength()' and
 * 'audioFileType()'.
 * Fixed handling of WAVE files as indicated by Eberhard Mattes. The length
 * of audio data is now taken from the WAVE header instead assuming that
 * the audio data reaches until the end of the file.
 *
 * Revision 1.6  1999/01/10 15:13:02  mueller
 * Added function 'checkAudioFile()'.
 *
 * Revision 1.5  1998/11/21 18:07:55  mueller
 * Added on-the-fly writing patch from Michael Weber <Michael.Weber@Post.RWTH-AAchen.DE>
 *
 * Revision 1.4  1998/11/15 12:15:18  mueller
 * Added member functions 'split()' and 'merge()'.
 *
 * Revision 1.3  1998/09/22 19:17:19  mueller
 * Added seeking to and reading of samples for GUI.
 *
 * Revision 1.2  1998/07/28 13:46:39  mueller
 * Automatic length determination of audio files is now done in 'AudioData'.
 *
 */


#ifndef __TRACKDATA_H__
#define __TRACKDATA_H__

#include <iostream.h>
#include "Sample.h"

#define AUDIO_BLOCK_LEN 2352
#define MODE0_BLOCK_LEN 2336
#define MODE1_BLOCK_LEN 2048
#define MODE2_BLOCK_LEN 2336

#define MODE2_FORM1_DATA_LEN 2048
#define MODE2_FORM2_DATA_LEN 2324

class TrackData {
public:
  enum Mode { AUDIO, MODE0, MODE1, MODE2, MODE2_FORM1, MODE2_FORM2,
              MODE2_FORM_MIX, MODE1_RAW, MODE2_RAW };

  enum Type { DATAFILE, ZERODATA, STDIN };
  
  enum FileType { RAW, WAVE };

  // creates an audio mode file entry
  TrackData(const char *filename, long offset, unsigned long start,
	    unsigned long length);
  TrackData(const char *filename, unsigned long start, unsigned long length);

  // creates a zero data entry with given mode
  TrackData(Mode, unsigned long length);

  // creates a file entry with given mode
  TrackData(Mode, const char *filename, long offset,
	    unsigned long length);
  TrackData(Mode, const char *filename, unsigned long length);

  // copy constructor
  TrackData(const TrackData &);

  ~TrackData();

  Type type() const;
  Mode mode() const;

  const char *filename() const;
  unsigned long startPos() const;
  unsigned long length() const;

  // sets/returns flag for swapping expected byte order of audio samples
  void swapSamples(int);
  int swapSamples() const;

  int determineLength();
  int check() const;

  void split(unsigned long, TrackData **part1, TrackData **part2);
  TrackData *merge(const TrackData *) const;

  void print(ostream &) const;

  static int checkAudioFile(const char *fn, unsigned long *length);
  static int waveLength(const char *filename, long offset, long *hdrlen,
			unsigned long *datalen = 0);
  static int audioDataLength(const char *fname, long offset,
			     unsigned long *length);
  static FileType audioFileType(const char *filename);
  static int dataFileLength(const char *fname, long offset,
			    unsigned long *length);
  static unsigned long dataBlockSize(Mode m);
  static const char *mode2String(Mode m);

private:
  Type type_; // type of data (file, stdin, zero)
  Mode mode_; // data mode for recording (audio, mode0, mode1, ...)
  FileType fileType_; // only for audio mode data, type of file (raw, wave)

  char *filename_; // used for object type 'FILE'

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
  void init(Mode, const char *filename, long offset,
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

inline
TrackData::Type TrackData::type() const
{
  return type_;
}

inline
TrackData::Mode TrackData::mode() const
{
  return mode_;
}

inline
const char *TrackData::filename() const
{
  return filename_;
}

inline
unsigned long TrackData::startPos() const
{
  return startPos_;
}

inline
void TrackData::swapSamples(int f)
{
  swapSamples_ = f != 0 ? 1 : 0;
}

inline
int TrackData::swapSamples() const
{
  return swapSamples_;
}

#endif

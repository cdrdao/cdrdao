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
 * $Log: TrackData.cc,v $
 * Revision 1.3  2001/03/25 07:36:14  andreasm
 * Updated SCSI lib to version from cdrtools-1.10a17.
 * Added patches from compilation under UnixWare.
 *
 * Revision 1.2  2000/06/10 14:44:47  andreasm
 * Tracks that are shorter than 4 seconds do not lead to a fatal error anymore.
 * The user has the opportunity to record such tracks now.
 *
 * Revision 1.1.1.1  2000/02/05 01:34:30  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 * Revision 1.12  1999/04/02 20:36:21  mueller
 * Created implementation class that contains all mutual member data.
 *
 * Revision 1.11  1999/03/27 19:49:29  mueller
 * Added data file support.
 *
 * Revision 1.10  1999/01/24 15:59:52  mueller
 * Added static member functions 'waveLength()', 'audioDataLength()' and
 * 'audioFileType()'.
 * Fixed handling of WAVE files as indicated by Eberhard Mattes. The length
 * of audio data is now taken from the WAVE header instead assuming that
 * the audio data reaches until the end of the file.
 *
 * Revision 1.9  1999/01/10 15:13:02  mueller
 * Added function 'checkAudioFile()'.
 *
 * Revision 1.8  1998/11/21 18:07:55  mueller
 * Added on-the-fly writing patch from Michael Weber <Michael.Weber@Post.RWTH-AAchen.DE>
 *
 * Revision 1.7  1998/11/15 12:15:18  mueller
 * Added member functions 'split()' and 'merge()'.
 *
 * Revision 1.6  1998/09/23 17:55:59  mueller
 * Improved error message about short audio file.
 *
 * Revision 1.5  1998/09/22 19:17:19  mueller
 * Added seeking to and reading of samples for GUI.
 *
 * Revision 1.4  1998/09/06 12:00:26  mueller
 * Used 'message' function for messages.
 *
 * Revision 1.3  1998/07/28 13:47:48  mueller
 * Automatic length determination of audio files is now done in 'AudioData'.
 *
 */

static char rcsid[] = "$Id: TrackData.cc,v 1.3 2001/03/25 07:36:14 andreasm Exp $";

#include <config.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "TrackData.h"
#include "Msf.h"
#include "util.h"

#ifdef UNIXWARE
extern "C" {
  extern int      strcasecmp(const char *, const char *);
}
#endif

// creates an object representing a portion of an audio data file
TrackData::TrackData(const char *filename, long offset,
		     unsigned long start, unsigned long length)
{
  init(filename, offset, start, length);
}

TrackData::TrackData(const char *filename, unsigned long start,
		     unsigned long length)
{
  init(filename, 0, start, length);
}

void TrackData::init(const char *filename, long offset,
		     unsigned long start, unsigned long length)
{
  assert(offset >= 0);

  length_ = length;

  if (strcmp(filename, "-") == 0) {
    type_ = STDIN;
    fileType_ = RAW; // currently only raw data
    filename_ = strdupCC("STDIN");
  }
  else {
    type_ = DATAFILE;
    filename_ = strdupCC(filename);
    fileType_ = audioFileType(filename);
  }
  
  offset_ = offset;
  startPos_ = start;
  swapSamples_ = 0;
  mode_ = AUDIO;
}

// creates an object that contains constant data with specified mode
TrackData::TrackData(Mode m, unsigned long length)
{
  type_ = ZERODATA;
  mode_ = m;

  filename_ = NULL;
  fileType_ = RAW;
  startPos_ = 0;
  offset_ = 0;
  length_ = length;
  swapSamples_ = 0;
}

// creates a file object  with given mode
TrackData::TrackData(Mode m, const char *filename, long offset,
		     unsigned long length)
{
  init(m, filename, offset, length);
}

TrackData::TrackData(Mode m, const char *filename, unsigned long length)
{
  init(m, filename, 0, length);
}

void TrackData::init(Mode m, const char *filename, long offset,
		     unsigned long length)
{
  assert(offset >= 0);

  mode_ = m;

  if (strcmp(filename, "-") == 0) {
    type_ = STDIN;
    fileType_ = RAW; // currently only raw data
    filename_ = strdupCC("STDIN");
  }
  else {
    type_ = DATAFILE;
    filename_ = strdupCC(filename);

    if (mode_ == AUDIO)
      fileType_ = audioFileType(filename);
    else
      fileType_ = RAW; // data files are always raw
  }

  offset_ = offset;
  length_ = length;

  startPos_ = 0;
  swapSamples_ = 0;
}

// copy constructor
TrackData::TrackData(const TrackData &obj)
{
  type_ = obj.type_;
  mode_ = obj.mode_;

  offset_ = obj.offset_;

  if (type_ == DATAFILE) {
    filename_ = strdupCC(obj.filename_);
    startPos_ = obj.startPos_;
    fileType_ = obj.fileType_;
  }
  else if (type_ == STDIN) {
    filename_ = strdupCC("STDIN");
    startPos_ = obj.startPos_;
    fileType_ = obj.fileType_;
  }
  else {
    filename_ = NULL;
    startPos_ = 0;
    fileType_ = RAW;
  }

  length_ = obj.length_;

  swapSamples_ = obj.swapSamples_;
}
    
TrackData::~TrackData()
{
  if (filename_ != NULL) {
    delete[] filename_;
    filename_ = NULL;
  }
}

unsigned long TrackData::length() const
{
  return length_;
}

// Determines length of data by inspecting the data file. The available data
// from the specified start point up to the end of file is stored in 'length_'.
// Return: 0: OK
//         1: cannot open or access the file (see 'errno')
//         2: start pos or offset exceeds length of file
int TrackData::determineLength()
{
  unsigned long len;

  if (type_ == DATAFILE) {
    if (mode_ == AUDIO) {
      switch (audioDataLength(filename_, offset_, &len)) {
      case 1:
	message(-2, "Cannot open audio file \"%s\": %s", filename_,
		strerror(errno));
	return 1;
	break;

      case 2:
	message(-2, "Cannot determine length of audio file \"%s\": %s",
		filename_, strerror(errno));
	return 1;
	break;

      case 3:
	message(-2, "Header of audio file \"%s\" is corrupted.",
		filename_);
	return 1;
	break;

      case 4:
	message(-2, "Invalid offset %ld for audio file \"%s\".", offset_,
		filename_);
	return 2;
	break;
      }

      if (startPos_ < len) {
	length_ = len - startPos_;
      }
      else {
	message(-2,
		"Start position %lu exceeds available data of file \"%s\".",
		startPos_, filename_);
	return 2;
      }
    }
    else {
      switch (dataFileLength(filename_, offset_, &len)) {
      case 1:
	message(-2, "Cannot open data file \"%s\": %s", filename_,
		strerror(errno));
	return 1;
	break;
      case 2:
	message(-2, "Invalid offset %ld for audio file \"%s\".", offset_,
		filename_);
	return 2;
	break;
      }

      length_ = len;
    }
  }

  return 0;
}



// checks the consistency of object
// return: 0: OK
//         1: warning occured
//         2: error occured
int TrackData::check(int trackNr) const
{
  switch (type_) {
  case ZERODATA:
    // always OK
    break;
  case STDIN:
    // cannot do much here...
    break;
  case DATAFILE:
    if (mode_ == AUDIO) {
      unsigned long len = 0;

      switch (audioDataLength(filename_, offset_, &len)) {
      case 1:
	message(-2, "Track %d: Cannot open audio file \"%s\": %s", trackNr,
		filename_, strerror(errno));
	return 2;
	break;
      case 2:
	message(-2, "Track %d: Cannot access audio file \"%s\": %s", trackNr,
		filename_, strerror(errno));
	return 2;
	break;
      case 3:
	message(-2, "Track %d: %s: Unacceptable WAVE file.", trackNr,
		filename_);
	return 2;
	break;
      case 4:
	message(-2, "Track %d: Invalid offset %ld for audio file \"%s\".",
		trackNr, offset_, filename_);
	return 2;
	break;
      }

      if (length() == 0) {
	message(-2, "Track %d: Requested length for audio file \"%s\" is 0.",
		trackNr, filename_);
	return 2;
      }

      if (startPos_ + length() > len) {
	// requested part exceeds file size
	message(-2,
		"Track %d: Requested length (%lu + %lu samples) exceeds length of audio file \"%s\" (%lu samples at offset %ld).",
		trackNr, startPos_, length(), filename_, len, offset_);
	return 2;
      }
    }
    else {
      // data mode
      unsigned long len;

      switch (dataFileLength(filename_, offset_, &len) != 0) {
      case 1:
	message(-2, "Track %d: Cannot open data file \"%s\": %s", trackNr,
		filename_, strerror(errno));
	return 2;
	break;
      case 2:
	message(-2, "Track %d: Invalid offset %ld for data file \"%s\".",
		trackNr, offset_, filename_);
	return 2;
	break;
      }

      if (length() == 0) {
	message(-2, "Track %d: Requested length for data file \"%s\" is 0.",
		trackNr, filename_);
	return 2;
      }

      if (length() > len) {
	message(-2, "Track %d: Requested length (%lu bytes) exceeds length of file \"%s\" (%lu bytes at offset %ld).",
		trackNr, length(), filename_, len, offset_);
	return 2;
      }
    }
    break;
  }

  return 0;
}

// writes out contents of object in TOC file syntax
void TrackData::print(ostream &out) const
{
  unsigned long blen;

  if (mode() == AUDIO) {
    // we're calculating in samples and not in bytes for audio data
    blen = SAMPLES_PER_BLOCK;
  }
  else {
    blen  = dataBlockSize(mode());
  }

  switch (type()) {
  case STDIN:
  case DATAFILE:
    if (mode() == AUDIO) {
      if (type() == STDIN)
	out << "FILE \"-\" ";
      else
	out << "FILE \"" << filename_ << "\" ";

      if (swapSamples_)
	out << "SWAP ";

      if (offset_ > 0)
	out << "#" << offset_ << " ";

      if (startPos() != 0 && (startPos() % blen) == 0)
	out << Msf(startPos() / blen).str();
      else
	out << startPos();
    
      out << " ";
    }
    else {
      // data mode
      if (type() == STDIN)
	out << "DATAFILE \"-\" ";
      else
	out << "DATAFILE \"" << filename_ << "\" ";

      //out <<  mode2String(mode()) << " ";

      if (offset_ > 0)
	out << "#" << offset_ << " ";
    }


    if ((length() % blen) == 0)
      out << Msf(length() / blen).str();
    else
      out << length();

    if (mode() != AUDIO)
      out << " // length in bytes: " << length();

    out << endl;
    break;

  case ZERODATA:
    if (mode_ == AUDIO) {
      out << "SILENCE ";
    }
    else {
      out << "ZERO " << mode2String(mode()) << " ";
    }
    
    if ((length() % blen) == 0)
      out << Msf(length() / blen).str();
    else
      out << length();

    out << endl;
    break;
  }
}

void TrackData::split(unsigned long pos, TrackData **part1, TrackData **part2)
{
  assert(mode_ == AUDIO);
  assert(pos > 0 && pos < length_);

  *part1 = new TrackData(*this);
  *part2 = new TrackData(*this);

  (*part1)->length_ = pos;

  (*part2)->length_ = length_ - pos;

  if (type_ == DATAFILE)
    (*part2)->startPos_ = startPos_ + pos;
}

TrackData *TrackData::merge(const TrackData *obj) const
{

  if (type_ != obj->type_ || mode_ != obj->mode_ || mode_ != AUDIO ||
      offset_ != obj->offset_)
    return NULL;

  TrackData *data = NULL;

  switch (type_) {
  case ZERODATA:
    data = new TrackData(*this);
    data->length_ += obj->length_;
    break;

  case DATAFILE:
    if (strcmp(filename_, obj->filename_) == 0 &&
	startPos_ + length_ == obj->startPos_) {
      data = new TrackData(*this);
      data->length_ += obj->length_;
    }
    break;

  case STDIN:
    // can't merge this type at all
    break;
  }
  
  return data;
}

// Checks if given audio file is suitable for cdrdao. 'length' is filled
// with number of samples in audio file on success.
// return: 0: file is suitable
//         1: cannot open or access file
//         2: file has wrong format

int TrackData::checkAudioFile(const char *fn, unsigned long *length)
{
  int fd;
  int ret;
  struct stat buf;
  long headerLength = 0;
  
  if ((fd = open(fn, O_RDONLY)) < 0)
    return 1;

  ret = fstat(fd, &buf);

  close(fd);

  if (ret != 0)
    return 1;

  if (audioFileType(fn) == WAVE) {
    if (waveLength(fn, 0, &headerLength, length) != 0)
      return 2;
  }
  else {
    if (buf.st_size % sizeof(Sample) != 0) {
      message(-1, "%s: Length is not a multiple of sample size (4).", fn);
    }

    *length = buf.st_size / sizeof(Sample);
  }

  return 0;
}


// Determines length of header and audio data for WAVE files. 'hdrlen' is
// filled with length of WAVE header in bytes. 'datalen' is filled with
// length of audio data in samples (if != NULL).
// return: 0: OK
//         1: error occured
//         2: illegal WAVE file
int TrackData::waveLength(const char *filename, long offset,
			  long *hdrlen, unsigned long *datalen)
{
  FILE *fp;
  char magic[4];
  long headerLen = 0;
  long len;
  short waveFormat;
  short waveChannels;
  long waveRate;
  short waveBits;
  struct stat sbuf;

#ifdef _WIN32
  if ((fp = fopen(filename, "rb")) == NULL)
#else
  if ((fp = fopen(filename, "r")) == NULL)
#endif
  {
    message(-2, "Cannot open audio file \"%s\" for reading: %s",
	    filename, strerror(errno));
    return 1;
  }

  if (offset != 0) {
    if (fseek(fp, offset, SEEK_SET) != 0) {
      message(-2, "Cannot seek to offset %ld in file \"%s\": %s",
	      offset, filename, strerror(errno));
      return 1;
    }
  }

  if (fread(magic, sizeof(char), 4, fp) != 4 ||
      strncmp("RIFF", magic, 4) != 0) {
    message(-2, "%s: not a WAVE file.", filename);
    fclose(fp);
    return 2;
  }

  readLong(fp);

  if (fread(magic, sizeof(char), 4, fp) != 4 ||
      strncmp("WAVE", magic, 4) != 0) {
    message(-2, "%s: not a WAVE file.", filename);
    fclose(fp);
    return 2;
  }

  // search for format chunk
  for (;;) {
    if (fread(magic, sizeof(char), 4, fp) != 4) {
      message(-2, "%s: corrupted WAVE file.", filename);
      fclose(fp);
      return 1;
    }

    len = readLong(fp);
    len += len & 1; // round to multiple of 2

    if (strncmp("fmt ", magic, 4) == 0) {
      // format chunk found
      break;
    }

    // skip chunk data
    if (fseek(fp, len, SEEK_CUR) != 0) {
      message(-2, "%s: corrupted WAVE file.", filename);
      fclose(fp);
      return 1;
    }
  }

  if (len < 16) {
    message(-2, "%s: corrupted WAVE file.", filename);
    fclose(fp);
    return 1;
  }

  waveFormat = readShort(fp);

  if (waveFormat != 1) {
    // not PCM format
    message(-2, "%s: not in PCM format.", filename);
    fclose(fp);
    return 2;
  }

  waveChannels = readShort(fp);
  if (waveChannels != 2) {
    message(-2, "%s: found %d channel(s), require 2 channels.",
	    filename, waveChannels);
    fclose(fp);
    return 2;
  }

  waveRate = readLong(fp);
  if (waveRate != 44100) {
     message(-2, "%s: found sampling rate %ld, require 44100.",
	    filename, waveRate);
     fclose(fp);
     return 2;
  }
  
  readLong(fp); // average bytes/second
  readShort(fp); // block align
  
  waveBits = readShort(fp);
  if (waveBits != 16) {
    message(-2, "%s: found %d bits per sample, require 16.",
	    filename, waveBits);
    fclose(fp);
    return 2;
  }
  
  len -= 16;

  // skip chunk data
  if (fseek(fp, len, SEEK_CUR) != 0) {
    message(-2, "%s: corrupted WAVE file.", filename);
    fclose(fp);
    return 1;
  }

  // search wave data chunk
  for (;;) {
    if (fread(magic, sizeof(char), 4, fp) != 4) {
      message(-2, "%s: corrupted WAVE file.", filename);
      fclose(fp);
      return 1;
    }
    
    len = readLong(fp);

    if (strncmp("data", magic, 4) == 0) {
      // found data chunk
      break;
    }

    len += len & 1; // round to multiple of 2
     
    // skip chunk data
    if (fseek(fp, len, SEEK_CUR) != 0) {
      message(-2, "%s: corrupted WAVE file.", filename);
      fclose(fp);
      return 1;
    }
  }

  if ((headerLen = ftell(fp)) < 0) {
    message(-2, "%s: cannot determine file position: %s",
	    filename, strerror(errno));
    fclose(fp);
    return 1;
  }

  headerLen -= offset;

  if (fstat(fileno(fp), &sbuf) != 0) {
    message(-2, "Cannot fstat audio file \"%s\": %s", filename,
	    strerror(errno));
    fclose(fp);
    return 1;
  }

  fclose(fp);

  if (len + headerLen + offset > sbuf.st_size) {
    message(-1,	"%s: file length does not match length from WAVE header - using actual length.", filename);
    len = sbuf.st_size - offset - headerLen;
  }

  if (len % sizeof(Sample) != 0) {
    message(-1,
	    "%s: length of data chunk is not a multiple of sample size (4).",
	    filename);
  }

  *hdrlen = headerLen;
  
  if (datalen != NULL) 
    *datalen = len / sizeof(Sample);

  return 0;
}

// Returns length in samples for given audio file.
// return: 1: file cannot be opened
//         2: 'fstat' failed
//         3: file header corruption
//         4: invalid offset
//         0: OK
int TrackData::audioDataLength(const char *fname, long offset, 
			       unsigned long *length)
{
  int fd;
  struct stat buf;
  long headerLength = 0;
  int ret;

  *length = 0;

  if ((fd = open(fname, O_RDONLY)) < 0)
    return 1;

  ret = fstat(fd, &buf);
  close(fd);

  if (ret != 0)
    return 2;

  if (offset > buf.st_size)
    return 4;

  if (audioFileType(fname) == WAVE) {
    if (waveLength(fname, offset, &headerLength, length) != 0)
      return 3;
  }
  else {
    if (((buf.st_size - offset) % sizeof(Sample)) != 0) {
      message(-1,
	      "Length of file \"%s\" is not a multiple of sample size (4).",
	      fname);
    }

    *length = (buf.st_size - offset) / sizeof(Sample);
  }

  return 0;
}


// Sets 'length' to length of given data file.
// return: 0: OK
//         1: file cannot be opened or accessed
//         2: invalid offset
int TrackData::dataFileLength(const char *fname, long offset,
			      unsigned long *length)
{
  int fd;
  struct stat buf;
  int ret;
  *length = 0;

  if ((fd = open(fname, O_RDONLY)) < 0)
    return 1;

  ret = fstat(fd, &buf);
  close(fd);

  if (ret != 0)
    return 1;

  if (offset > buf.st_size)
    return 2;

  *length = buf.st_size - offset;

  return 0;
}

// determines type of audio file
// return: RAW: raw samples
//         WAVE: wave file
TrackData::FileType TrackData::audioFileType(const char *filename)
{
  char *p;

  if ((p = strrchr(filename, '.')) != NULL) {
    if (strcasecmp(p, ".wav") == 0) {
      return WAVE;
    }
  }

  return RAW;
}

unsigned long TrackData::dataBlockSize(Mode m)
{
  unsigned long b = 0;

  switch (m) {
  case AUDIO:
  case MODE1_RAW:
  case MODE2_RAW:
    b = AUDIO_BLOCK_LEN;
    break;

  case MODE0:
    b = MODE0_BLOCK_LEN;
    break;

  case MODE1:
    b = MODE1_BLOCK_LEN;
    break;

  case MODE2:
  case MODE2_FORM_MIX:
    b = MODE2_BLOCK_LEN;
    break;

  case MODE2_FORM1:
    b = MODE2_FORM1_DATA_LEN;
    break;

  case MODE2_FORM2:
    b = MODE2_FORM2_DATA_LEN;
    break;
  }

  return b;
}

const char *TrackData::mode2String(Mode m)
{
  const char *ret = NULL;

  switch (m) {
  case AUDIO:
    ret = "AUDIO";
    break;

  case MODE0:
    ret = "MODE0";
    break;

  case MODE1:
    ret = "MODE1";
    break;

  case MODE1_RAW:
    ret = "MODE1_RAW";
    break;

  case MODE2:
    ret = "MODE2";
    break;

  case MODE2_RAW:
    ret = "MODE2_RAW";
    break;

  case MODE2_FORM1:
    ret = "MODE2_FORM1";
    break;

  case MODE2_FORM2:
    ret = "MODE2_FORM2";
    break;

  case MODE2_FORM_MIX:
    ret = "MODE2_FORM_MIX";
    break;
  }

  return ret;
}


TrackDataReader::TrackDataReader(const TrackData *d)
{
  trackData_ = d;

  open_ = 0;
  fd_ = -1;
  readPos_ = 0;
  headerLength_ = 0;
  readUnderRunMsgGiven_ = 0;
}

TrackDataReader::~TrackDataReader()
{
  if (open_) {
    closeData();
  }

  trackData_ = NULL;
}

void TrackDataReader::init(const TrackData *d)
{
  if (open_) {
    closeData();
  }

  trackData_ = d;
}

// initiates reading audio data, an audio data file is opened
// return: 0: OK
//         1: file could not be opened
//         2: could not seek to start position
int TrackDataReader::openData()
{
  assert(open_ == 0);
  assert(trackData_ != NULL);

  if (trackData_->type_ == TrackData::DATAFILE) {
    if (trackData_->mode_ == TrackData::AUDIO) {
      long headerLength = 0;

#ifdef _WIN32
      if ((fd_ = open(trackData_->filename_, O_RDONLY | O_BINARY)) < 0)
#else
      if ((fd_ = open(trackData_->filename_, O_RDONLY)) < 0)
#endif
      {
	message(-2, "Cannot open audio file \"%s\": %s", trackData_->filename_,
		strerror(errno));
	return 1;
      }

      if (trackData_->fileType_ == TrackData::WAVE) {
	if (TrackData::waveLength(trackData_->filename_, trackData_->offset_,
				  &headerLength) != 0) {
	  message(-2, "%s: Unacceptable WAVE file.", trackData_->filename_);
	  return 1;
	}
      }
      
      if (lseek(fd_, trackData_->offset_ + headerLength + (trackData_->startPos_ * sizeof(Sample)), SEEK_SET) < 0) {
	message(-2, "Cannot seek in audio file \"%s\": %s",
		trackData_->filename_, strerror(errno));
	return 2;
      }

      headerLength_ = headerLength;
    }
    else {
      // data mode
      headerLength_ = 0;

#ifdef _WIN32
      if ((fd_ = open(trackData_->filename_, O_RDONLY | O_BINARY)) < 0)
#else
      if ((fd_ = open(trackData_->filename_, O_RDONLY)) < 0)
#endif
      {
	message(-2, "Cannot open data file \"%s\": %s", trackData_->filename_,
		strerror(errno));
	return 1;
      }

      if (trackData_->offset_ > 0) {
	if (lseek(fd_, trackData_->offset_ , SEEK_SET) < 0) {
	  message(-2, "Cannot seek to offset %ld in file \"%s\": %s",
		  trackData_->offset_, trackData_->filename_, strerror(errno));
	  return 2;
	}
      }
    }
  }
  else if (trackData_->type_ == TrackData::STDIN) {
    headerLength_ = 0;
    fd_ = fileno(stdin);
  }

  readPos_ = 0;
  open_ = 1;
  readUnderRunMsgGiven_ = 0;

  return 0;
}

// ends reading audio data, an audio data file is closed
void TrackDataReader::closeData()
{
  if (open_ != 0) {
    if (trackData_->type_ == TrackData::DATAFILE) {
      close(fd_);
    }

    fd_ = -1;
    open_ = 0;
    readPos_ = 0;
  }
}

// fills 'buffer' with 'len' samples
// return: number of samples written to buffer
long TrackDataReader::readData(Sample *buffer, long len)
{
  long readLen = 0;

  assert(open_ != 0);

  if (len == 0) {
    return 0;
  }

  if (readPos_ + len > trackData_->length()) {
    if ((len = trackData_->length() - readPos_) <= 0) {
      return 0;
    }
  }

  switch (trackData_->type_) {
  case TrackData::ZERODATA:
    if (trackData_->mode_ == TrackData::AUDIO)
      memset(buffer, 0, len * sizeof(Sample));
    else
      memset(buffer, 0, len);
    
    readLen = len;
    break;

  case TrackData::STDIN:
  case TrackData::DATAFILE:
    if (trackData_->mode_ == TrackData::AUDIO) {
      readLen = fullRead(fd_, buffer, len * sizeof(Sample));

      if (readLen < 0) {
	message(-2, "Read error while reading audio data from file \"%s\": %s",
		trackData_->filename_, strerror(errno));
      }
      else if (readLen != (long)(len * sizeof(Sample))) {
	long pad = len * sizeof(Sample) - readLen;

	if (readUnderRunMsgGiven_ == 0) {
	  message(-1, "Could not read expected amount of audio data from file \"%s\".", trackData_->filename_);
	  message(-1, "Padding with zeros.");

	  readUnderRunMsgGiven_ = 1;
	}

	// Adding zeros to the 'buffer'
	memset(buffer + readLen, 0, pad);
	readLen = len;
      }
      else {
	readLen = len;
      }
    }
    else {
      readLen = fullRead(fd_, buffer, len);
      if (readLen < 0) {
	message(-2, "Read error while reading data from file \"%s\": %s",
		trackData_->filename_, strerror(errno));
      }
      else if (readLen != len) {
	message(-2, "Could not read expected amount of data from file \"%s\".",
		trackData_->filename_);
	readLen = -1;
      }
    }
    break;
  }

  if (readLen > 0) {
    if (trackData_->mode_ == TrackData::AUDIO) {
      int swap = 0;

      if (trackData_->fileType_ == TrackData::WAVE) {
	// WAVE files contain little endian samples
	swap = 1;
      }

      if (trackData_->swapSamples_) 
	swap = !swap;

      if (swap) {
	// swap samples 
	swapSamples(buffer, readLen);
      }
    }

    readPos_ += readLen;
  }

  return readLen;
}

// Seeks to specified sample.
// return: 0: OK
//        10: sample out of range
int TrackDataReader::seekSample(unsigned long sample)
{
  assert(open_ != 0);

  if (sample >= trackData_->length()) 
    return 10;

  if (trackData_->type_ == TrackData::DATAFILE) {
    if (lseek(fd_,
	      trackData_->offset_ + headerLength_ +
	      (sample * sizeof(Sample)) + 
	      (trackData_->startPos_ * sizeof(Sample)),
	      SEEK_SET) < 0) {
      message(-2, "Cannot seek in audio file \"%s\": %s",
	      trackData_->filename_, strerror(errno));
      return 10;
    }
  }

  readPos_ = sample;
  
  return 0;
}

// Returns number of bytes that are still available for reading.
unsigned long TrackDataReader::readLeft() const
{
  assert(open_ != 0);
  assert(trackData_ != NULL);

  return trackData_->length() - readPos_;
}

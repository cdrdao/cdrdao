/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2004  Denis Leroy <denis@poolshark.org>
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

#ifndef __FORMATCONVERTER_H__
#define __FORMATCONVERTER_H__

#include <string>
#include <map>
#include <list>

#include "TrackData.h"
#include "Toc.h"

// Quick abstract class declarations. Format converters should derive
// their own FormatSupport and FormatSupportManager.

class FormatSupport
{
 public:
  virtual ~FormatSupport() {}

  typedef enum {
    FS_SUCCESS,
    FS_IN_PROGRESS,
    FS_WRONG_FORMAT,
    FS_INPUT_PROBLEM,
    FS_OUTPUT_PROBLEM,
    FS_DISK_FULL,
    FS_DECODE_ERROR,
    FS_OTHER_ERROR,
  } Status;

  // Convert source file to destination WAV or RAW. This is a blocking
  // call until conversion is finished.
  // Return values:
  //   0: success
  //   1: problem with input file
  //   2: problem with output file
  //   3: problem with conversion
  virtual Status convert(const char* from, const char* to) = 0;

  // Same as above, but asynchronous interface. Call start, then call
  // continue in a busy loop until it no longer returns
  // FS_IN_PROGRESS.
  virtual Status convertStart(const char* from, const char* to) = 0;
  virtual Status convertContinue() = 0;
  virtual void   convertAbort() = 0;
  
  // Specify what this object converts to. Should only returns either
  // TrackData::WAVE or TrackData::RAW
  virtual TrackData::FileType format() = 0;
};

class FormatSupportManager
{
 public:
  virtual ~FormatSupportManager() {}

  // Acts as virtual constructor. Returns a new converter if this
  // converter understands the given file extension.
  virtual FormatSupport* newConverter(const char* extension) = 0;

  // Add supported file extensions to list. Returns number added.
  virtual int supportedExtensions(std::list<std::string>&) = 0;
};


// The global format conversion class. A single global instance of
// this class exists and manages all format conversions.

class FormatConverter
{
 public:
  FormatConverter();
  virtual ~FormatConverter();

  // Returns true if the converter understands this format
  bool canConvert(const char* fn);

  // Convert file, return tempory file with WAV or RAW data (based on
  // temp file extension).Returns NULL if conversion failed.
  const char* convert(const char* src, FormatSupport::Status* st = NULL);

  // Convert all files contained in a given Toc object, and update the
  // Toc accordingly. This is a big time blocking call.
  FormatSupport::Status convert(Toc* toc);

  // Dynamic allocator.
  FormatSupport* newConverter(const char* src);

  // Do it yourself. Returns a converter and starts it. Sets dst to
  // the converter file name (or clears it if no converter
  // found). Returns NULL if file can't be converted.
  FormatSupport* newConverterStart(const char* src, std::string& dst,
                                   FormatSupport::Status* status = NULL);

  // Add all supported extensions to string list. Returns number added.
  int supportedExtensions(std::list<std::string>&);

 private:
  std::list<std::string*> tempFiles_;
  std::list<FormatSupportManager*> managers_;
};

extern FormatConverter formatConverter;

// Utility for parsing M3U files

bool parseM3u(const char* m3ufile, std::list<std::string>& list);

#endif

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

#include <stdlib.h>
#ifdef HAVE_AO
#include <ao/ao.h>
#endif
#include <fstream>
#include <cstring>

#include "config.h"
#include "log.h"
#include "FormatConverter.h"
#include "TempFileManager.h"

#ifdef HAVE_MP3_SUPPORT
#include "FormatMp3.h"
#endif
#ifdef HAVE_OGG_SUPPORT
#include "FormatOgg.h"
#endif

FormatConverter::FormatConverter()
{
#if defined(HAVE_MP3_SUPPORT) || defined(HAVE_OGG_SUPPORT)
  ao_initialize();
#endif
#ifdef HAVE_MP3_SUPPORT
  managers_.push_front(new FormatMp3Manager);
#endif
#ifdef HAVE_OGG_SUPPORT
  managers_.push_front(new FormatOggManager);
#endif
}

FormatConverter::~FormatConverter()
{
  std::list<FormatSupportManager*>::iterator i = managers_.begin();
  while (i != managers_.end()) {
    delete *i++;
  }
#if defined(HAVE_MP3_SUPPORT) || defined(HAVE_OGG_SUPPORT)
    ao_shutdown();
#endif
}

FormatSupport* FormatConverter::newConverter(const char* fn)
{
  const char* ext = strrchr(fn, '.');
  if (!ext)
    return NULL;
  ext++;

  FormatSupport* candidate = NULL;
  std::list<FormatSupportManager*>::iterator i = managers_.begin();
  while (i != managers_.end()) {
    FormatSupportManager* mgr = *i++;

    candidate = mgr->newConverter(ext);
    if (candidate)
      break;
  }

  return candidate;
}

FormatSupport* FormatConverter::newConverterStart(const char* src,
                                                  std::string& dst,
                                                  FormatSupport::Status* st)
{
  if (st)
    *st = FormatSupport::FS_SUCCESS;

  FormatSupport* candidate = newConverter(src);

  if (candidate) {
    const char* extension;
    if (candidate->format() == TrackData::WAVE)
      extension = "wav";
    else
      extension = "raw";

    bool exists = tempFileManager.getTempFile(dst, src, extension);

    if (exists) {
      delete candidate;
      return NULL;
    }

    FormatSupport::Status ret = candidate->convertStart(src, dst.c_str());
    if (st)
        *st = ret;

    if (ret == FormatSupport::FS_SUCCESS)
      return candidate;
    else
      delete candidate;
  }

  dst = "";
  return NULL;
}

bool FormatConverter::canConvert(const char* fn)
{
  FormatSupport* c = newConverter(fn);

  if (!c)
    return false;

  delete c;
  return true;
}

const char* FormatConverter::convert(const char* fn,
                                     FormatSupport::Status* err)
{
  *err = FormatSupport::FS_SUCCESS;

  FormatSupport* c = newConverter(fn);

  if (!c)
    return NULL;

  std::string* file = new std::string;
  const char* extension;
  if (c->format() == TrackData::WAVE)
      extension = "wav";
  else
      extension = "raw";

  bool exists = tempFileManager.getTempFile(*file, fn, extension);

  if (!exists) {
    log_message(2, "Decoding file \"%s\"", fn);
    *err = c->convert(fn, file->c_str());

    if (*err != FormatSupport::FS_SUCCESS)
      return NULL;
    
    tempFiles_.push_front(file);
  }

  return file->c_str();
}

int FormatConverter::supportedExtensions(std::list<std::string>& list)
{
  int num = 0;

  std::list<FormatSupportManager*>::iterator i = managers_.begin();
  for (;i != managers_.end(); i++) {
    num += (*i)->supportedExtensions(list);
  }
  return num;
}

FormatSupport::Status FormatConverter::convert(Toc* toc)
{
  FormatSupport::Status err;

  std::set<std::string> set;

  toc->collectFiles(set);

  std::set<std::string>::iterator i = set.begin();

  for (; i != set.end(); i++) {

    const char* dst = convert((*i).c_str(), &err);
    if (!dst && err != FormatSupport::FS_SUCCESS)
      return FormatSupport::FS_OTHER_ERROR;

    if (dst)
      toc->markFileConversion((*i).c_str(), dst);
  }

  return FormatSupport::FS_SUCCESS;
}

bool parseM3u(const char* m3ufile, std::list<std::string>& list)
{
  // You'd think STL would be smart enough to NOT have to use a stack
  // buffer like this. STL is so poorly designed there's no any other
  // way.
  char buffer[1024];

  std::string dir = m3ufile;
  dir = dir.substr(0, dir.rfind("/"));
  dir += "/";

  std::ifstream file(m3ufile, std::ios::in);

  if (!file.is_open())
    return false;

  while (!file.eof()) {
    file.getline(buffer, 1024);

    std::string e = buffer;
    if (!e.empty() && (*(e.begin())) != '#') {

      if (e[0] != '/')
        e = dir + e;

      int n;
      while ((n = e.find('\r')) >= 0)
        e.erase(n, 1);
      while ((n = e.find('\n')) >= 0)
        e.erase(n, 1);

      list.push_back(e);
    }
  }

  file.close();
  return true;
}

FormatConverter formatConverter;

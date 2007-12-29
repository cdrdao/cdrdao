/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2001 Andreas Mueller <andreas@daneb.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "Settings.h"

#include "util.h"
#include "log.h"


#ifdef UNIXWARE
extern "C" {
  extern int      strcasecmp(const char *, const char *);
}
#endif

  
enum SettingType { SET_INTEGER, SET_STRING };

const char* Settings::setWriteSpeed       = "write_speed";
const char* Settings::setWriteDriver      = "write_driver";
const char* Settings::setWriteDevice      = "write_device";
const char* Settings::setWriteBuffers     = "write_buffers";
const char* Settings::setUserCapacity     = "user_capacity";
const char* Settings::setFullBurn         = "full_burn";
const char* Settings::setReadSpeed        = "read_speed";
const char* Settings::setReadDriver       = "read_driver";
const char* Settings::setReadDevice       = "read_device";
const char* Settings::setReadParanoiaMode = "read_paranoia_mode";
const char* Settings::setCddbServerList   = "cddb_server_list";
const char* Settings::setCddbTimeout      = "cddb_timeout";
const char* Settings::setCddbDbDir        = "cddb_directory";
const char* Settings::setTmpFileDir       = "tmp_file_dir";

class SettingEntry {
public:
  SettingEntry(const char *, int);
  SettingEntry(const char *, const char *);
  ~SettingEntry();

  SettingType type_;

  char *name_;
  union {
    int integerValue_;
    char *stringValue_;
  } val_;

  SettingEntry *next_;
};

class SettingsImpl {
public:
  SettingsImpl();
  ~SettingsImpl();

  void addSetting(SettingEntry *);
  SettingEntry *findSetting(const char *, SettingType);
  void set(const char *, int);
  void set(const char *, const char *);

  int read(FILE *);
  
  void parseAndSetValue(char *name, char *valStr);

  SettingEntry *settings_;
};

SettingEntry::SettingEntry(const char *name, int val)
{
  type_ = SET_INTEGER;
  name_ = strdupCC(name);
  val_.integerValue_ = val;
}

SettingEntry::SettingEntry(const char *name, const char *val)
{
  type_ = SET_STRING;
  name_ = strdupCC(name);
  val_.stringValue_ = strdupCC(val);
}

SettingEntry::~SettingEntry()
{
  delete[] name_;
  name_ = NULL;

  if (type_ == SET_STRING) {
    delete[] val_.stringValue_;
    val_.stringValue_ = NULL;
  }
}


SettingsImpl::SettingsImpl()
{
  settings_ = NULL;
}

SettingsImpl::~SettingsImpl()
{
  SettingEntry *next;

  while (settings_ != NULL) {
    next = settings_->next_;
    delete settings_;
    settings_ = next;
  }
}

void SettingsImpl::addSetting(SettingEntry *s)
{
  s->next_ = settings_;
  settings_ = s;
}


SettingEntry *SettingsImpl::findSetting(const char *name, SettingType type)
{
  SettingEntry *run;

  for (run = settings_; run != NULL; run = run->next_) {
    if (run->type_ == type && strcasecmp(run->name_, name) == 0)
      return run;
  }

  return NULL;
}

void SettingsImpl::set(const char *name, int val)
{
  SettingEntry *s = findSetting(name, SET_INTEGER);

  if (s == NULL) {
    addSetting(new SettingEntry(name, val));
  }
  else {
    s->val_.integerValue_ = val;
  }
}

void SettingsImpl::set(const char *name, const char *val)
{
  SettingEntry *s = findSetting(name, SET_STRING);

  if (s == NULL) {
    addSetting(new SettingEntry(name, val));
  }
  else {
    delete[] s->val_.stringValue_;
    s->val_.stringValue_ = strdupCC(val);
  }
}

#define MAX_LINE_LENGTH 1024
int SettingsImpl::read(FILE *fp)
{
  char buf[MAX_LINE_LENGTH];
  char *p, *p1;
  char *name;
  long n;

  while (fgets(buf, MAX_LINE_LENGTH, fp) != NULL) {
    // handle comment
    if ((p = strchr(buf, '#')) != NULL)
      continue;

    if ((p = strchr(buf, ':')) != NULL) {
      *p++ = 0;

      p1 = buf;
      while (*p1 != 0 && isspace(*p1))
        p1++;

      name = p1;

      while (*p1 != 0 && !isspace(*p1))
        p1++;
      *p1 = 0;

      while (*p != 0 && isspace(*p))
        p++;

      // strip off trailing white space
      if ((n = strlen(p)) > 0) {
        for (p1 = p + n - 1; p1 >= p; p1--) {
          if (isspace(*p1)) {
            *p1 = 0;
          }
        }
      }

      parseAndSetValue(name, p);
    }
  }

  return 0;
}

void SettingsImpl::parseAndSetValue(char *name, char *valStr)
{
  char *p;
  char *val;
  int intValue;

  if (name == NULL || *name == 0 || valStr == NULL || *valStr == 0)
    return;

  if (*valStr == '"') {
    val = valStr + 1;
    p = val + strlen(val)-1;
    if (*p != '"') {
      fprintf(stderr,"Error in string constant '%s'\n", valStr);
    } else {
      *p=0;
      set(name, val);
    }
    
  } else {  /* valSTR is numeric? */
    char * end = NULL;
    errno=0;
    intValue = strtol(valStr, &end, 0);
    if(errno != 0 || !end || *end) {
      fprintf(stderr,"Error parsing numeric option '%s' (missing quotes?)\n",
              valStr);
    } else
      set(name, intValue);
  }
}

Settings::Settings()
{
  impl_ = new SettingsImpl;
}

Settings::~Settings()
{
  delete impl_;
  impl_ = NULL;
}

int Settings::read(const char *fname)
{
  FILE *fp;

  if ((fp = fopen(fname, "r")) == NULL)
    return 1;

  impl_->read(fp);

  fclose(fp);

  return 0;
}

int Settings::write(const char *fname) const
{
  SettingEntry *run;
  FILE *fp;
  
  if ((fp = fopen(fname, "w")) == NULL) {
    log_message(-2, "Cannot open \"%s\" for writing: %s", fname, strerror(errno));
    return 1;
  }

  for (run = impl_->settings_; run != NULL; run = run->next_) {
    switch (run->type_) {
    case SET_INTEGER:
      fprintf(fp, "%s: %d\n", run->name_, run->val_.integerValue_);
      break;
    case SET_STRING:
      fprintf(fp, "%s: \"%s\"\n", run->name_, run->val_.stringValue_);
      break;
    }
  }

  fclose(fp);

  return 0;
}

const int *Settings::getInteger(const char *name) const
{
  SettingEntry *s = impl_->findSetting(name, SET_INTEGER);

  if (s != NULL)
    return &(s->val_.integerValue_);
  else
    return NULL;
}
  
const char *Settings::getString(const char *name) const
{
  SettingEntry *s = impl_->findSetting(name, SET_STRING);

  if (s != NULL)
    return s->val_.stringValue_;
  else
    return NULL;
}

void Settings::set(const char *name, int val)
{
  impl_->set(name, val);
}

void Settings::set(const char *name, const char *val)
{
  impl_->set(name, val);
}  

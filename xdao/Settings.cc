/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998, 1999 Andreas Mueller <mueller@daneb.ping.de>
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
 * $Log: Settings.cc,v $
 * Revision 1.1.1.1  2000/02/05 01:39:57  llanero
 * Uploaded cdrdao 1.1.3 with pre10 patch applied.
 *
 */

static char rcsid[] = "$Id: Settings.cc,v 1.1.1.1 2000/02/05 01:39:57 llanero Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "Settings.h"

#include "CdDevice.h"

#include "util.h"

enum SettingType { SET_INTEGER, SET_STRING };

const char *SET_CDRDAO_PATH = "cdrdaoPath";
const char *SET_RECORD_EJECT_WARNING = "recordEjectWarning";
const char *SET_RECORD_RELOAD_WARNING = "recordReloadWarning";

#define SET_CDDEVICE "cdDevice"
#define SET_CDDEVICE_LEN 8

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

  while (fgets(buf, MAX_LINE_LENGTH, fp) != NULL) {
    // handle comment
    if ((p = strchr(buf, '#')) != NULL)
      *p = 0;

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
  CdDevice *dev;

  if (name == NULL || *name == 0 || valStr == NULL || *valStr == 0)
    return;

  if (*valStr == '"') {
    val = p = valStr + 1;

    while (*p != 0 && *p != '"')
      p++;

    if (*p == '"') {
      *p = 0;

      if (strncasecmp(name, SET_CDDEVICE, SET_CDDEVICE_LEN) == 0) {
	if ((dev = CdDevice::add(val)) != NULL)
	  dev->manuallyConfigured(1);
      }
      else {
	set(name, val);
      }
    }
  }
  else {
    intValue = strtol(valStr, NULL, 0);
    set(name, intValue);
  }
}

Settings::Settings()
{
  impl_ = new SettingsImpl;

  set(SET_CDRDAO_PATH, "cdrdao");
  set(SET_RECORD_EJECT_WARNING, 1);
  set(SET_RECORD_RELOAD_WARNING, 1);
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
  int i;
  char *s;
  const CdDevice *drun;
  SettingEntry *run;
  FILE *fp;
  
  if ((fp = fopen(fname, "w")) == NULL) {
    message(-2, "Cannot open \"%s\" for writing: %s", fname, strerror(errno));
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

  for (drun = CdDevice::first(), i = 0; drun != NULL;
       drun = CdDevice::next(drun)) {
    if (drun->manuallyConfigured()) {
      s = drun->settingString();

      fprintf(fp, "%s%d: \"%s\"\n", SET_CDDEVICE, i, s);
      delete[] s;
      i++;
    }
  }

  fclose(fp);

  return 0;
}

int Settings::getInteger(const char *name) const
{
  SettingEntry *s = impl_->findSetting(name, SET_INTEGER);

  if (s != NULL)
    return s->val_.integerValue_;
  else
    return 0;
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

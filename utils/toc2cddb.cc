/*  toc2cddb - translates a TOC file into a cddb file
 *  I use it to print covers for self-made CD-TEXT audio CDs
 *  coupled with disc-cover (http://www.liacs.nl/~jvhemert/disc-cover)
 *
 *  Cdrdao
 *	 Copyright (C) 2002 Andreas Mueller <andreas@daneb.de>
 *  Toc2cddb 
 *	 Copyright (C) 2003 Giuseppe "Cowo" Corbelli <cowo@lugbs.linux.it>
 *	 Parts by Andreas Mueller <andreas@daneb.de>
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <string>
#include "util.h"
#include "Toc.h"

#define FRAME_OFFSET 150
#define FRAMES_PER_SECOND 75

using namespace std;

static int VERBOSE = 1;

static unsigned int cddbSum(unsigned int n);
const char *calcCddbId(const Toc *toc);
void message_args(int level, int addNewLine, const char *fmt, va_list args);
void message(int level, const char *fmt, ...);

static void printVersion()	{
	message (1, "toc2cddb version %s - (C) Giuseppe \"Cowo\" Corbelli <cowo@lugbs.linux.it>", VERSION);
	message (1, "Is a part of cdrdao - (C) Andreas Mueller <andreas@daneb.de>");
}
static void printUsage()	{
	message (0, "toc2cddb converts a cdrdao TOC file into a cddb file and prints it to stdout.");
	message (0, "Usage: toc2cddb {-V | -h | toc-file}");
}

int main (int argc, char *argv[])	{
	const char *tocFile = NULL;
	Toc *toc = NULL;
	const Track *track = NULL;
	int cdTextLanguage = 0;
	int c = 0;

	while ((c = getopt(argc, argv, "Vh")) != EOF) {
    	switch (c) {
			case 'V':
				printVersion ();
				exit (EXIT_SUCCESS);
			case 'h':
				printUsage ();
				exit (EXIT_SUCCESS);
			case '?':
				message(-2, "Invalid option: %c", optopt);
				exit (EXIT_FAILURE);
		}
	}
	if (optind < argc) {
		tocFile = strdupCC(argv[optind]);
    	optind++;
	}	else	{
    	message(-2, "Missing toc-file name.");
    	printUsage ();
    	exit (EXIT_FAILURE);
	}
	if (optind != argc) {
		message(-2, "More arguments than expected.");
		printUsage ();
    	exit (EXIT_FAILURE);	
	}

	if ((toc = Toc::read(tocFile)) == NULL)
		message(-10, "Failed to read toc-file '%s'.", tocFile);
	
	if (toc->tocType () != Toc::CD_DA)
		message (-10, "Toc does not refer to a CDDA");

	int ntracks = toc->nofTracks ();
	if (ntracks < 1)
		message (-10, "Wrong no. of tracks: %d. Expected >= 0.", ntracks);

	cout << "# xmcd\n#\n# Track frame offsets:\n#" << endl;
	
	{
		TrackIterator titr (toc);
		Msf start, end;
		for (track=titr.first (start, end); track != NULL; track=titr.next (start,end))
			cout << "# " << start.lba()+FRAME_OFFSET << endl;
	
		int seconds = (end.min () * 60) + end.sec () + (FRAME_OFFSET/FRAMES_PER_SECOND);
		cout << "#\n# Disc length: " << seconds << " seconds\n#" << endl;
	}
	
	cout << "# Revision: 0\n# Submitted via: cdrdao-" << VERSION << endl;
	cout << "DISCID=" << calcCddbId (toc) << endl;

	{
		const CdTextItem *cdTextItem = NULL;
		string album(""), albumPerformer(""), genre("");

		if ((cdTextItem = toc->getCdTextItem(0, cdTextLanguage, CdTextItem::CDTEXT_TITLE)) != NULL)
			album = (const char*)cdTextItem->data();
	
		if ((cdTextItem = toc->getCdTextItem(0, cdTextLanguage, CdTextItem::CDTEXT_PERFORMER)) != NULL)
			albumPerformer = (const char*)cdTextItem->data();

		cout << "DTITLE=" << albumPerformer << " / " << album << endl;
		cout << "DYEAR=" << endl;
	
		if ((cdTextItem = toc->getCdTextItem(0, cdTextLanguage, CdTextItem::CDTEXT_GENRE)) != NULL)
			genre = (const char*)cdTextItem->data();

		cout << "DGENRE=" << genre << endl;
		
		string title("");
		for (int i = 1; i <= ntracks; i++)	{
			if ((cdTextItem = toc->getCdTextItem(i, cdTextLanguage, CdTextItem::CDTEXT_TITLE)) != NULL)
				title = (const char*)cdTextItem->data();
			cout << "TTITLE" << i-1 << "=" << title << endl;
		}
	}

	// Don't know what EXTD means, nor EXTT
	cout << "EXTD=" << endl;
	for (int i = 1; i <= ntracks; i++)
		cout << "EXTT" << i-1 << "=" << endl;

	cout << "PLAYORDER=" << endl;

	delete[] tocFile;
	exit (EXIT_SUCCESS);
}

static unsigned int cddbSum(unsigned int n)
{
  unsigned int ret;

  ret = 0;
  while (n > 0) {
    ret += (n % 10);
    n /= 10;
  }

  return ret;
}

const char *calcCddbId(const Toc *toc)
{
  const Track *t;
  Msf start, end;
  unsigned int n = 0;
  unsigned int o = 0;
  int tcount = 0;
  static char buf[20];
  unsigned long id;

  TrackIterator itr(toc);

  for (t = itr.first(start, end); t != NULL; t = itr.next(start, end)) {
    if (t->type() == TrackData::AUDIO) {
      n += cddbSum(start.min() * 60 + start.sec() + 2/* gap offset */);
      o  = end.min() * 60 + end.sec();
      tcount++;
    }
  }

  id = (n % 0xff) << 24 | o << 8 | tcount;
  sprintf(buf, "%08lx", id);

  return buf;
}

void message_args(int level, int addNewLine, const char *fmt, va_list args)
{
  long len = strlen(fmt);
  char last = len > 0 ? fmt[len - 1] : 0;

  if (level < 0) {
    switch (level) {
    case -1:
      fprintf(stderr, "WARNING: ");
      break;
    case -2:
      fprintf(stderr, "ERROR: ");
      break;
    case -3:
      fprintf(stderr, "INTERNAL ERROR: ");
      break;
    default:
      fprintf(stderr, "FATAL ERROR: ");
      break;
    }
    vfprintf(stderr, fmt, args);
    if (addNewLine) {
      if (last != ' ' && last != '\r')
	fprintf(stderr, "\n");
    }

    fflush(stderr);
    if (level <= -10)
      exit(1);
  }
  else if (level <= VERBOSE) {
    vfprintf(stderr, fmt, args);

    if (addNewLine) {
      if (last != ' ' && last != '\r')
	fprintf(stderr, "\n");
    }

    fflush(stderr);
  }
}

void message(int level, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);

  message_args(level, 1, fmt, args);

  va_end(args);
}

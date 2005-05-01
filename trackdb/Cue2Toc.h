/* cue2toc.h - declarations for conversion routines
 * Copyright (C) 2004 Matthias Czapla <dermatsch@gmx.de>
 *
 * This file is part of cue2toc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <iostream>

#include "Track.h"

/* Maximum length of the FILEname */
#define FILENAMELEN 1024
/* Number of characters allowed per CD-Text entry (w/o termin. Null) */
#define CDTEXTLEN 80

/* Index can be 0 to 99, but 0 and 1 are pre-gap and track start
   respectively, so 98 are left */
#define NUM_OF_INDEXES 98

enum session_type {
    CD_DA = 1,	/* only audio tracks */
    CD_ROM,		/* mode1 [and audio] */
    CD_ROM_XA,	/* mode2 form1 or mode2 form2 [and audio] */
    INVALID		/* invalid mixture of track modes */
};

struct trackspec {
    TrackData::Mode mode;
    int copy;			/* boolean */
    int pre_emphasis;		/* boolean */
    int four_channel_audio;		/* boolean */
    char isrc[13];
    char title[CDTEXTLEN + 1];
    char performer[CDTEXTLEN + 1];
    char songwriter[CDTEXTLEN + 1];
    char filename[FILENAMELEN + 1];;
    long pregap;			/* Pre-gap in frames */
    int pregap_data_from_file;	/* boolean */
    long start;			/* track start in frames */
    long postgap;			/* Post-gap in frames */
    long indexes[NUM_OF_INDEXES];	/* indexes in frames */
    struct trackspec *next;
};

struct cuesheet {
    char catalog[14];
    enum session_type type;
    char title[CDTEXTLEN + 1];
    char performer[CDTEXTLEN + 1];
    char songwriter[CDTEXTLEN + 1];
    struct trackspec *tracklist;
};

struct cuesheet *read_cue(const char*, const char*);

void write_toc(std::ostream&, struct cuesheet*, bool withCdText);

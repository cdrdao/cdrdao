/* timecode.c - timecode conversion routines
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXDIGITS	2
#define NUMOFNUMS	3


/* Interpret argument as timecode value ("MM:SS:FF") and return the total
   number of frames. Tries to work in a way similar to atoi(), ignoring any
   trailing non-timecode junk. Skips leading whitespace.
   I want it to be as flexible as possible, recognizing simple values like
   "0" (interpreted as "00:00:00"), "1:2" ("00:01:02") and so on.
   Returns -1 on error (argument NULL or some value out of range) */

long
tc2fr(const char *tc)
{
	int minutes = 0;
	int seconds = 0;
	int frames = 0;
	long totalframes = 0;

	char tmp[MAXDIGITS + 1];
	int nums[NUMOFNUMS];
	int n = 0;
	int i = 0;
	int last_was_colon = 0;
	int stop = 0;

	if (tc == NULL)
		return -1;

	for (i = 0; i <= MAXDIGITS; i++)
		tmp[i] = '\0';

	while (isspace(*tc))
		tc++;

	for (n = 0; n < NUMOFNUMS; n++) {
		if (n > 0) {
			if (tc[0] != ':') {
				--n;
				break;
			} else
				tc++;
		}

		for (i = 0; i < MAXDIGITS; i++) {

			if (isdigit(tc[i])) {
				tmp[i] = tc[i];
				last_was_colon = 0;
			} else if (tc[i] == ':') {
				if (i == 0)
					stop = 1;
				break;
			} else { 
				stop = 1;
				break;
			}
		}
		
		if (i != 0) {
			tmp[i] = '\0';
			nums[n] = atoi(tmp);
			tc = &tc[i];
		} else
			--n;

		if (stop)
			break;
	}

	if (n == NUMOFNUMS)
		--n;
	
	frames = seconds = minutes = 0;

	switch (n) {
	case 0:
		frames = nums[0];
		break;
	case 1:
		seconds = nums[0];
		frames = nums[1];
		break;
	case 2:
		minutes = nums[0];
		seconds = nums[1];
		frames = nums[2];
		break;
	}

	totalframes = ((60 * minutes) + seconds) * 75 + frames;

	if (seconds > 59 || frames > 74)
		return -1;

	return totalframes;
}


/* Writes formatted timecode string ("MM:SS:FF") into tc, calculated from
   frame number fr.
   Returns -1 on error (frames value out of range) */

int
fr2tc(char *tc, long fr)
{
	int m;
	int s;
	int f;

	if (fr > 449999 || fr < 0) {	/* 99:59:74 */
		strcpy(tc, "00:00:00");
		return -1;
	}

	f = fr % 75;
	fr -= f;
	s = (fr / 75) % 60;
	fr -= s * 75;
	m = fr / 75 / 60;

	sprintf(tc, "%02d:%02d:%02d", m, s, f);
	return 0;
}

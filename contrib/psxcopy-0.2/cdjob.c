/*  cdjob.c 
 *
 *  Copyright (C) 1999  Fabio Baracca <fabiobar@tiscalinet.it>
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
#include <string.h>

#define BUF_S 1024
#define DATA_SIGN "DATAFILE"
#define AUDIO_SIGN "FILE"
#define TRACKTYPE_SIGN "TRACK"
#define MODE1_SIGN "MODE1"
#define MODE2_SIGN "MODE2"
#define SECT_SIZE 2048

FILE *inf, *outf, *device;
char *fname, *track_info, *cdrom_device;
char buf[BUF_S], file_to_read[BUF_S];
int is_data_track = 0, line_count = 0;
int i, j, k, x, data_size, chr;
int min, sec, frame, size_in_sect, rcode = 0, rem, readin;
int audio_begin = 0, audio_end = 0, tracks = 0, prevseek = 0;

int main(int argc, char *argv[])
{
    if (argc != 3) {
	fprintf(stderr, "Usage: %s <cue-sheet> <cdrom device>\n", argv[0]);
	return 1;
    }
    fname = argv[1];
    cdrom_device = argv[2];

    inf = (FILE *) fopen(fname, "r+");
    if (inf == NULL) {
	fprintf(stderr, "Unable to open \"%s\" cue-sheet!\n", fname);
	return 2;
    }
    device = (FILE *) fopen(cdrom_device, "r");

/*
    if (device == NULL) {
	fprintf(stderr, "Unable to open \"%s\" for reading!\n", cdrom_device);
	return 4;
    }
    fclose (device);
*/    
    /* Testing for PSX CD */

    sprintf (buf, "psxdump -T -d %s", cdrom_device);
    i=system (buf);   
    
    if (i!=0) {
       fprintf(stderr, "Unable to read from %s or disk is not PSX, please check and retry.\n", cdrom_device);
       exit (1);
    }
    
    /* Total bytes (about to) read */
    readin = 0;

    printf("cdjob 0.2 - Fabio Baracca <fabiobar@tiscalinet.it>\n\n");

    while (!feof(inf)) {
        prevseek=ftell (inf);
	fgets(buf, BUF_S, inf);
	if (!feof(inf)) {
	    line_count++;

	    /* Store data type */
	    if (strncasecmp(buf, TRACKTYPE_SIGN, strlen(TRACKTYPE_SIGN)) == 0) {
		if (strstr(buf, MODE1_SIGN))
		    is_data_track = 1;
		else if (strstr(buf, MODE2_SIGN)) { /* HACK IT! */
		    is_data_track = 1;
		    buf[11]=0x20;
		    buf[12]='/';
		    buf[13]='/';
		    fseek (inf, prevseek, SEEK_SET);
		    fputs (buf, inf);
		}
		else
		    is_data_track = 0;
	    }
	    /* Data to be read.. */
	    if ((strncasecmp(buf, DATA_SIGN, strlen(DATA_SIGN)) == 0) ||
		(strncasecmp(buf, AUDIO_SIGN, strlen(AUDIO_SIGN)) == 0)) {
		/* Zero vars */
		i = j = k = x = 0;

		/* Search the name */
		for (j = 0, i = 0; i < strlen(buf); i++) {
		    if (buf[i] == '"') {
			j++;
			if (j == 1)
			    k = i + 1;
			else if (j >= 2) {
			    x = i - 1;
			    break;
			}
		    }
		}

		/* Check the name */
		if (j != 2) {
		    fprintf(stderr, "Bogus characters at line %d\n", line_count);
		    rcode = 100;
		} else if ((x - k + 1) > BUF_S) {
		    fprintf(stderr, "Ooppss.. filename too long..\n");
		    rcode = 101;
		} else {
		    strcpy(file_to_read, buf + k);
		    /* Track size info */
		    track_info = buf + x + 2;
		    for (j = 0, i = x + 2; i < strlen(buf); i++) {
			if (buf[i] == ':') {
			    j++;
			    if (j >= 2) {
				track_info[i - x + 1] = 0;
				break;
			    }
			}
		    }

		    x -= k;
		    file_to_read[++x] = 0;

		    if (!is_data_track) {	/* audio ? :-D */
			tracks++;
			if (audio_begin != 0)
			    audio_end = tracks;
			else
			    audio_begin = tracks;
		    }
		    if (is_data_track) {
			tracks++;
/*			printf("About data file: \"%s\"..", file_to_read);*/
			if ((sscanf(track_info, "%d:%d:%d", &min, &sec, &frame)) != 3) {
			    printf("bogus size info!\n");
			    rcode = 102;
			} else {
                            char toshell[1024]; /* HACK!!! */
			    size_in_sect = (((min * 60 + sec) * 75 + frame) * SECT_SIZE);
			    track_info[1]=0x20;
			    track_info[2]='/';
			    track_info[3]='/';
                            fseek (inf, prevseek, SEEK_SET);
                            fputs (buf, inf);
                                       			    
			    sprintf (toshell, "psxdump -f %s -d %s", file_to_read, cdrom_device);
                            system (toshell);
			}
		    } else {
			/* Silenty trash garbage */
			/* printf ("mm.. not a data file.. ignoring!\n"); */
			/* rcode=200; */
		    }
		}
	    }
	}
    }

    fclose(inf);
    fclose(device);
    if (audio_begin != 0) {	/* Start cdparanoia */
	printf("Ok.. now reading audio part of the disk.\n\n");
	sprintf(buf, "cdparanoia -z %d- data.wav", audio_begin);
	if (system(buf) != 0)
	    fprintf(stderr, "\n\nCdparanoia execution error. - CD dump can be corrupted.\n");
    }
    printf ("\n");
    return rcode;
}

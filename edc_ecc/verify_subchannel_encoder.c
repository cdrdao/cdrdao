/* test program for the sector formatting library */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "ecc.h"

int main(int argc, char *argv[])
{
	int fin;
	unsigned int sector = 0;
	unsigned int verified = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s 2448-byte-sector-file\n", argv[0]);
		return 0;
	}

	if (argv[1][0] == '-') {
		fin = STDIN_FILENO;
	} else {
		fin = open(argv[1], O_RDONLY);
	}
	if (fin == -1) {
		perror("");
		fprintf(stderr, "could not open file %s for reading.\n",argv[1]);
		exit(1);
	}

	for (; 1; sector++) {
		unsigned char inbuf[2448];
		unsigned char outbuf[96];
		unsigned char inbuf_copy[2448];
		int have_read;

		/* get one original sector */
		have_read = 0;
		while (2448 != have_read) {
			int retval;

			retval = read(fin, inbuf+have_read, 2448-have_read);
			if (retval < 0) break;

			if (retval == 0)
				break;
			have_read += retval;
		}

	        /* make a copy to work on */
		memcpy(inbuf_copy, inbuf, 2448);

		{ int j;
		  int checked_ok = 0;
			for (j = 0; j < 4; j++) {
				if (decode_LSUB_P(inbuf_copy+2352+j*24) != 0) {
					break;
				}
				if (decode_LSUB_Q(inbuf_copy+2352+j*24) != 0) {
					break;
				}
			}
			if (j != 4) continue;
		}
		/* compact fields, move data over parity fields */
		memmove(inbuf_copy+2352+72+2, inbuf_copy+2352+72+4, 16);
		memmove(inbuf_copy+2352+68, inbuf_copy+2352+72, 18);
		memmove(inbuf_copy+2352+48+2, inbuf_copy+2352+48+4, 16+18);
		memmove(inbuf_copy+2352+44, inbuf_copy+2352+48, 18+18);
		memmove(inbuf_copy+2352+24+2, inbuf_copy+2352+24+4, 16+18+18);
		memmove(inbuf_copy+2352+20, inbuf_copy+2352+24, 18+18+18);
		memmove(inbuf_copy+2352+2, inbuf_copy+2352+4, 16+18+18+18);

		/* build expanded version */
		do_encode_sub(inbuf_copy+2352, outbuf, 0, 0);

		/* copy p and q subchannel from original */
		{ int i;
			for (i = 0; i < 96; i++) {
				outbuf[i] |= inbuf[i+2352] & 0xc0;
			}
		}

		/* check subchannels */
		if (memcmp(outbuf, inbuf+2352, 96) != 0) {
			fprintf(stderr, "difference at sector %u\n", sector);
			{ int i;
				for (i = 0; i < 96; i++) {
					fprintf(stderr, "%2d: Soll : 0x%02x,   0x%02x  : Ist,  %s\n", i, inbuf[2352+i], outbuf[i], inbuf[2352+i]==outbuf[i] ? "true" : "false");
				}
			}
		} else verified++;
	}
fprintf(stderr, "%u sectors, %u verified ok\n", sector, verified);
	close(fin);
	return 0;
}

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
	int address;
	
	int fout;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s 2352-byte-sector-file\n", argv[0]);
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

	address = 150;
	for (; 1; sector++) {
		unsigned char inbuf[2448];
		unsigned char inbuf_copy[2448];
		int have_read;

		/* get one original sector */
		have_read = 0;
		while (2352 != have_read) {
			int retval;

			retval = read(fin, inbuf+have_read, 2352-have_read);
			if (retval < 0) break;

			if (retval == 0)
				break;
			have_read += retval;
		}

		if (have_read != 2352)
		  break;

	        /* make a copy to work on */
		memcpy(inbuf_copy, inbuf, 2448);

#if 0
		/* check CRC value */
		if (memcmp(inbuf_copy+16+8+2048, "\0\0\0", 4) != 0
		    || build_edc(inbuf_copy,16,16+8+2048-1) != 0U)
			continue;

		{ int j;

			for (j = 0; j < 1; j++) {
				if (decode_L2_P(inbuf_copy) != 0) {
					break;
				}
				if (decode_L2_Q(inbuf_copy) != 0) {
					break;
				}
			}
			if (j != 1) continue;
		}
#endif

		/* clear fields,  which shall be build */
		memset(inbuf_copy, 0, 16);
		memset(inbuf_copy+16+8+2048, 0, 4 + 276);

		/* build fields */
		do_encode_L2(inbuf_copy, MODE_2_FORM_1, address++);

		/* check fields */
		if (memcmp(inbuf_copy, inbuf, 2352) != 0) {
			fprintf(stderr, "difference at sector %u\n", sector);
		} else verified++;

		printf("%d\r", sector);
		fflush(stdout);
	}
fprintf(stderr, "%u sectors, %u verified ok\n", sector, verified);
	close(fin);

	return 0;
}



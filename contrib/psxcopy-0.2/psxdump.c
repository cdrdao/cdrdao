/*
 * psxdump.c - Dumps playstation disks to file
 *
 * Portions (c) 1999 Fabio Baracca <fabiobar@tiscalinet.it>
 *
 * *HEAVILY BASED* on code from readxa.c 
 * that's (c) 1996,1997 Gerd Knorr <kraxel@cs.tu-berlin.de>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <linux/fs.h>

/* Ugly, I know.. */
extern char *sys_errlist[];

typedef unsigned char u8bit;
typedef unsigned short u16bit;
typedef unsigned u32bit;

unsigned char psx_sign[] =
{0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01,
 0x43, 0x44, 0x30, 0x30, 0x31, 0x01, 0x00, 0x50, 0x4C,
 0x41, 0x59, 0x53, 0x54, 0x41, 0x54, 0x49, 0x4F, 0x4E,
 0x20, 0x20, 0x20, 0x20, 0x20};

typedef struct tag_iso_dir_entry {
    u8bit len_dr;
    u8bit XAR_len;
    u32bit loc_extentI;
    u32bit loc_extentM;
    u32bit data_lenI;
    u32bit data_lenM;
    u8bit record_time[7];
    u8bit file_flags_iso;
    u8bit il_size;
    u8bit il_skip;
    u16bit VSSNI;
    u16bit VSSNM;
    u8bit len_fi;
    u8bit file_id[32];
    u8bit padding;
} iso_dir_entry;

iso_dir_entry dir_entry;

static const char *device_list[] =
{
    /* try most common first ... */
    "/dev/cdrom",
    "/dev/scd0",
    "/dev/sr0",
    "/dev/hdc",
    "/dev/hdd",

    "/dev/scd1",
    "/dev/sr1",
    "/dev/scd2",
    "/dev/sr2",
    "/dev/scd3",
    "/dev/sr3",
    "/dev/hda",
    "/dev/hdb",
    "/dev/hde",
    "/dev/hdf",
    "/dev/hdg",
    "/dev/hdh",
    NULL
};

int print_info = 0;
int dump_raw = 0;

void about(void)
{
    fprintf(stderr, "psxdump.c - Dumps playstation disks to file\n\nA quick hack that's (c) 1999 Fabio Baracca <fabiobar@tiscalinet.it>\nLicensed under GNU GPL - *HEAVILY BASED* on code from readxa.c\nthat's (c) 1996,1997 Gerd Knorr <kraxel@cs.tu-berlin.de>\n\n");
}

void usage(void)
{
    fprintf(stderr,
	    "syntax: psxdump -d <device> -f <output file>\n\n"
	    "  -d <device>       give device name\n"
	    "  -f <output file>  file to put track dump\n"
	    "  -F                force the program to proceed even if it's not a PSX disk\n"
	    "  -T                test type of disc (PSX or non) and report it as exit code\n"
	    "  -h                display this banner :-)\n");
}

int read_raw_frame(int fd, int lba, unsigned char *buf)
{
    struct cdrom_msf *msf;
    int rc;

    msf = (struct cdrom_msf *) buf;
    msf->cdmsf_min0 = (lba + CD_MSF_OFFSET) / CD_FRAMES / CD_SECS;
    msf->cdmsf_sec0 = (lba + CD_MSF_OFFSET) / CD_FRAMES % CD_SECS;
    msf->cdmsf_frame0 = (lba + CD_MSF_OFFSET) % CD_FRAMES;
    rc = ioctl(fd, CDROMREADMODE2, buf);
    if (-1 == rc)
	perror("ioctl CDROMREADMODE2");
    return rc;
}

int main(int argc, char *argv[])
{
    int forcepsx = 0, ofile, cdrom, rc, c, size, percent, maxsize, testpsx = 0;
    unsigned long block;
    unsigned char *buf;
    struct stat st, dev_st;
    int device;
    long seek = -1;
    struct cdrom_tocentry toc, toc2;

    char *outfile = NULL, *devname = NULL;

    /* parse options */
    for (;;) {
	c = getopt(argc, argv, "TFd:f:");
	if (c == -1)
	    break;
	switch (c) {
	case 'f':
	    outfile = optarg;
	    break;
	case 'd':
	    devname = optarg;
	    break;
	case 'F':
	    forcepsx = 1;
	    break;
	case 'T':
	    testpsx = 1;
	    break;
	case 'h':
	default:
	    usage();
	    exit(1);
	}
    }

    /* Malloc-ing memory */
    if (NULL == (buf = malloc(CD_FRAMESIZE_RAW0))) {
	fprintf(stderr, "Out of memory\n");
	exit(1);
    }

    if ((testpsx) && (devname)) {
	if (-1 == (cdrom = open(devname ? devname : device_list[device], O_RDONLY | O_NONBLOCK))) {
	    fprintf(stderr, "open %s: %s\n", devname ? devname : device_list[device], sys_errlist[errno]);
	    exit(2);
	}
	if (0 != (rc = read_raw_frame(cdrom, 16, buf))) {
	    perror("read error");
	    exit(2);
	}
	close(rc);
	if (memcmp(psx_sign, buf, sizeof(psx_sign)) == 0)
	    exit(0);
	else
	    exit(1);
    }
    
    about();

    if ((!outfile) || (!devname)) {
	fprintf(stderr, "Please specify *either* the file name (or -T flag) *and* the device of the CD reader.\n\n");
	usage();
	exit(2);
    }
    /* Opening the CDROM reader  */
    if (-1 == (cdrom = open(devname ? devname : device_list[device], O_RDONLY | O_NONBLOCK))) {
	fprintf(stderr, "open %s: %s\n", devname ? devname : device_list[device], sys_errlist[errno]);
	exit(1);
    }

    if (!forcepsx) {
	/* Checking if it's a PSX disk */
	printf("Checking if the disk is a PSX disk..");
	fflush(stdout);
	if (0 != (rc = read_raw_frame(cdrom, 16, buf))) {	/* Safe PSX signature is at 0x9200 */
	    perror("read error");
	    exit(1);
	}
	/* Checking PSX signature */
	if (memcmp(psx_sign, buf, sizeof(psx_sign)) == 0) {
	    printf("okay, found a PSX disk.\n");
	} else {
	    printf("sorry.. this doesn't seem a PSX disk.\n\nIf you think it's a bug, use -F flag and send a dump of the sector 16\nto me, Fabio Baracca <fabiobar@tiscalinet.it>. Thanks!\n");
	    exit(3);
	}
	close(rc);
    } else {
	printf("-F specified: assuming that the disk is a PSX disk.\n");
    }

    /* Checking the limits of the track */
    if (-1 == (rc = open(devname, O_RDONLY | O_NONBLOCK))) {
	fprintf(stderr, "open %s: %s\n", devname, sys_errlist[errno]);
	exit(1);
    }
    toc.cdte_track = 1;
    toc.cdte_format = CDROM_LBA;
    if (-1 == ioctl(rc, CDROMREADTOCENTRY, &toc)) {
	perror("ioctl CDROMREADTOCENTRY [start]");
	exit(1);
    }
    toc2.cdte_track = 2;
    toc2.cdte_format = CDROM_LBA;
    if (-1 == ioctl(rc, CDROMREADTOCENTRY, &toc2)) {
	toc2.cdte_track = CDROM_LEADOUT;
	toc2.cdte_format = CDROM_LBA;
	if (-1 == ioctl(rc, CDROMREADTOCENTRY, &toc2)) {
	    perror("ioctl CDROMREADTOCENTRY [stop]");
	    exit(1);
	}
    }
    block = toc.cdte_addr.lba;	/* Should be zero */
    if (block) {
	fprintf(stderr, "Hei!.. S-T-R-A-N-G-E.. starting sector is NOT zero !??!\n");
    }
    size = (toc2.cdte_addr.lba - block) * 2048;
    close(rc);

    /* Opening the output file */
    if (-1 == (ofile = open(outfile, O_WRONLY | O_CREAT))) {
	fprintf(stderr, "open %s: %s\n", outfile, sys_errlist[errno]);
	exit(1);
    }
    /* Finnally dump the track */
    if (-1 == (rc = open(devname ? devname : device_list[device], O_RDONLY | O_NONBLOCK))) {
	fprintf(stderr, "open %s: %s\n", devname ? devname : device_list[device], sys_errlist[errno]);
	exit(1);
    }
    if (0 != (rc = read_raw_frame(cdrom, block, buf))) {
	perror("read error");
	exit(1);
    }
    printf("\nDumping %d bytes (or maybe a bit less..) to disk..\n", (toc2.cdte_addr.lba - block) * 2336);
    printf("Please IGNORE any error! Your disk WILL BE OK! 100%% Assured!\n\n");

    maxsize = size;
    percent = 0;		/* Status bar */

    printf("Completed: %d%%\r", percent);
    fflush(stdout);

    for (;;) {
	int offset, len, tmp;

	if (buf[0] == buf[4] &&
	    buf[1] == buf[5] &&
	    buf[2] == buf[6] &&
	    buf[3] == buf[7] &&
	    buf[2] != 0 &&
	    buf[0] < 8 &&
	    buf[1] < 8) {
	    offset = 8;
	    len = (buf[2] & 0x20) ? 2324 : 2048;
	} else {
	    offset = 0;
	    len = 2048;
	}
	write(ofile, buf, CD_FRAMESIZE_RAW0);	/* dump to disk */
	size -= 2048 /* len */ ;
	block++;
	if (size <= 0)
	    break;

	tmp = (int) 100 - (((double) size / (double) maxsize) * 100);
	if (tmp != percent) {
	    percent = tmp;
	    printf("Completed: %d%%\r", percent);
	    fflush(stdout);
	}
	if (0 != (rc = read_raw_frame(cdrom, block, buf))) {
	    perror("read error (hei! the disk is 99.9%% ok if we are in the last part of the disk)");
	    exit(1);
	}
    }
    printf("PSX data track dump complete.\n");
    free(buf);
    exit(0);
}

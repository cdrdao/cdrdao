/***************************************************************************
                          mp3dec.c  -  description
                             -------------------
    begin                : Thu Jan 18 2001
    copyright            : (C) 2001 by cowo
    email                : cowo@lugbs.linux.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <libmpeg3.h>
#include <sndfile.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#define RIGHT_CHANNEL 0
#define LEFT_CHANNEL 1
#define CHUNK 100 /* Number of short int samples to handle at a time */
#define OGG_CHUNK 10000 /* 50000 short int samples */
#define DEBUG

int decode_mp3 (char* inpath, char* outpath);
int decode_ogg (char* inpath, char* outpath);
void progress_callback (long int total, long int actual);

int main(int argc, char* argv[])
{
	return decode_mp3 (argv[1], argv[2]);
}

void progress_callback (long int total, long int actual)	{
	ldiv_t result, granularity;
	long int scarto;

	if (total < 100) return;
	granularity = ldiv (total, 100);
	result = ldiv (actual, granularity.quot);
	if ( actual >= (granularity.quot*(result.quot)) )	{
		fprintf (stdout, "\b\b\b%02ld%%", result.quot);
	}
}

/*return 0 if failure, 1 if OK*/
/*
int decode_ogg (char* inpath, char* outpath)	{
	char *outbuf;
	OggVorbis_File vf;
	vorbis_info *vi;
	int current_section;
	FILE *inputfile;
	long ret;
	SF_INFO *wavdesc;
	SNDFILE *outfile;
	
	if ( !(outbuf = (char *) malloc (sizeof (char) * OGG_CHUNK)) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	if ( !(vi = (vorbis_info *) malloc (sizeof (vorbis_info))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	if ( !(wavdesc = (SF_INFO *) malloc (sizeof (SF_INFO))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	if ( !(outfile = (SNDFILE *) malloc (sizeof (SNDFILE))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}

	if ( !(inputfile = fopen (inpath, "r"))) {
		fprintf (stderr, "Cannot open input file %s\n", inpath);
		return (EXIT_FAILURE);
	}

	if ( (ov_open (inputfile, &vf, NULL, 0)) < 0) { 
		fprintf (stderr, "Input does not appear to be an Ogg bitstream.\n");
		return (EXIT_FAILURE);
	} 
	if ( !(vi = ov_info(&vf, -1)) )	{
		fprintf (stderr, "The specified bitstream does not exist or the file has been initialized improperly.\n");
		return (EXIT_FAILURE);
	}

	fprintf (stdout, "Total PCM samples %ld\n", ov_bitrate (&vf, -1));
	wavdesc->samplerate = vi->rate;
	wavdesc->channels = vi->channels;
	wavdesc->pcmbitwidth = 16;
	wavdesc->format = SF_FORMAT_WAV|SF_FORMAT_PCM;
	
	if ( !(outfile=sf_open_write(outpath, wavdesc)))	{
		fprintf (stderr, "Error while opening write file\n");
		return (EXIT_FAILURE);
	}
	ret=1;
	while(ret != 0)	{
		ret = ov_read (&vf, outbuf, OGG_CHUNK, 0, 2, 1, &current_section);
		if (ret < 0) {
			fprintf (stderr, "Error in the stream: %d\n", ret);
		} else {
			if ( (sf_write_short (outfile, (short *)outbuf, ret/2)) < ret/2)	{
				fprintf (stderr, "Not enough bytes written!\n");
				return (EXIT_FAILURE);
			}
		}
	}
	sf_close (outfile);
	ov_clear (&vf);
	free (outbuf);
	return (EXIT_SUCCESS);
}*/
/*return 0 if failure, 1 if OK*/
int decode_mp3 (char* inpath, char* outpath)	{
	short *bufr, *bufl, *temp;
	int srate, res, nstreams, nchannels;
	register int lindex, rindex, sindex;
	long nsamples;
	mpeg3_t* mp3fd;
	SF_INFO *wavdesc;
	SNDFILE *outfile;
	
	/* Buffer to contain mixed right and left channel */
	if ( !(temp = (short *) malloc (sizeof (short) * CHUNK)) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	/* Right channel, half the capacity of temp */
	if ( !(bufr = (short *) malloc (CHUNK)) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}	
	/* Left channel, half the capacity of temp */
	if ( !(bufl = (short *) malloc (CHUNK)) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}	
	/* libsndfile structures */
	if ( !(wavdesc = (SF_INFO *) malloc (sizeof (SF_INFO))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	if ( !(outfile = (SNDFILE *) malloc (sizeof (SNDFILE))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	if ( !(mp3fd = (mpeg3_t *) malloc (sizeof (mpeg3_t))) )	{
		fprintf (stderr, "Error while allocating memory at line %d of file %s.\n", __LINE__, __FILE__);
		perror("");
		return (EXIT_FAILURE);
	}
	
	if (!mpeg3_check_sig (inpath))	{
		fprintf (stderr, "Not compatible MPEG stream\n");
		return (EXIT_FAILURE);
	}

	if ( !(mp3fd = mpeg3_open(inpath)) )	{
		fprintf (stderr, "Cannot open source file %s\n", inpath);
		return (EXIT_FAILURE);
	}
	if (!mpeg3_has_audio (mp3fd))	{
		fprintf (stderr, "No audio in MPEG file.\n");
		return (EXIT_FAILURE);
	}
	if ( (nstreams = mpeg3_total_astreams (mp3fd)) != 1)	{
		fprintf (stderr, "Sorry, unsupported number of streams: %d\n", nstreams);
		return (EXIT_FAILURE);
	}
	if ( (nchannels = mpeg3_audio_channels (mp3fd, 0)) != 2)	{
		fprintf (stderr, "Sorry, unsupported number of channels: %d\n", nchannels);
		return (EXIT_FAILURE);
	}
	nsamples = mpeg3_audio_samples (mp3fd, 0);
	srate = mpeg3_sample_rate (mp3fd, 0);
	if (srate != 44100)	{
		fprintf (stderr, "Sorry, CD-DA needs 44100 hz samplerate\n");
		return (EXIT_FAILURE);
	}
	wavdesc->samplerate = srate;
	wavdesc->channels = nchannels;
	wavdesc->pcmbitwidth = 16;
	wavdesc->format = SF_FORMAT_WAV|SF_FORMAT_PCM;
	
	/* Rewind stream */
	mpeg3_set_sample (mp3fd, 0, 0);

	if ( !(outfile=sf_open_write(outpath, wavdesc)))	{
		fprintf (stderr, "Error while opening write file\n");
		return (EXIT_FAILURE);
	}
	fprintf (stdout, "Progress:    ");
	do	{
		if ( ((res = mpeg3_read_audio (mp3fd, NULL, bufr, RIGHT_CHANNEL, CHUNK/2, 0)) != 0) ||
			 ((res = mpeg3_reread_audio (mp3fd, NULL, bufl, LEFT_CHANNEL, CHUNK/2, 0)) != 0) )	{
			fprintf (stderr, "Error decoding audio\n");
			return (EXIT_FAILURE);
		}
		progress_callback (nsamples, mpeg3_get_sample (mp3fd, 0));
		rindex=0;
		lindex=1;
		sindex=0;
		while ( (rindex < CHUNK) && (lindex < CHUNK) && (sindex < CHUNK/2) )	{
			memcpy ((short *)temp+(rindex*sizeof(short)), (short *)bufr+(sindex*sizeof(short)), sizeof (short));			
			memcpy ((short *)temp+(lindex*sizeof(short)), (short *)bufl+(sindex*sizeof(short)), sizeof (short));
			rindex+=sizeof(short);
			lindex+=sizeof(short);
			sindex++;
		}			
		if ( (sf_write_short (outfile, temp, CHUNK)) < CHUNK)	{
			fprintf (stderr, "Not enough bytes written!\n");
			return (EXIT_FAILURE);
		}
	}	while ( !(mpeg3_end_of_audio(mp3fd, 0)));
	
	mpeg3_close (mp3fd);
	sf_close (outfile);
	fprintf (stdout, "\n");
	return (EXIT_SUCCESS);
}

/***
 * CopyPolicy: GNU Public License 2 applies
 * Copyright (C) by Monty (xiphmont@mit.edu)
 ***/

#ifndef _GAP_H_
#define _GAP_H_

extern long i_paranoia_overlap_r(size16 *buffA,size16 *buffB,
				 long offsetA, long offsetB);
extern long i_paranoia_overlap_f(size16 *buffA,size16 *buffB,
				 long offsetA, long offsetB,
				 long sizeA,long sizeB);
extern int i_stutter_or_gap(size16 *A, size16 *B,long offA, long offB,
			    long gap);
extern void i_analyze_rift_f(size16 *A,size16 *B,
			     long sizeA, long sizeB,
			     long aoffset, long boffset, 
			     long *matchA,long *matchB,long *matchC);
extern void i_analyze_rift_r(size16 *A,size16 *B,
			     long sizeA, long sizeB,
			     long aoffset, long boffset, 
			     long *matchA,long *matchB,long *matchC);

extern void analyze_rift_silence_f(size16 *A,size16 *B,long sizeA,long sizeB,
				   long aoffset, long boffset,
				   long *matchA, long *matchB);
#endif

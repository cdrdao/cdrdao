/* reed-solomon encoder table generator 
 * Copyright 1998 by Heiko Eissfeldt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ecc.h"

#define RS_L12_POLY 0x1d	/* x^8 + x^4 + x^3 + x^2 + 1 */
#define RS_L12_BITS 8

#define RS_SUB_RW_POLY 0x03	/* x^6 + x + 1 */
#define RS_SUB_RW_BITS 6

#define RS_L12_PRIM_ELEM 2
#define RS_SUB_RW_PRIM_ELEM 2

/* define macros for polynomial arithmetic on GF(2^n) */
#define add(a,b) (a ^ b)
#define sub(a,b) (a ^ b)
#define neg(a) (a)
#define mul(a,b,m) (((a) != 0) && ((b) != 0) ? alog[(log[a] + log[b]) % (m)] : 0)
#define div(a,b,m) ((a) != 0 ? alog[(log[a] - log[b] + (m)) % (m)] : 0)

/* global variables */
static unsigned char rs_l12_log[1 << RS_L12_BITS];
static unsigned char rs_l12_alog[1 << RS_L12_BITS];

static unsigned char rs_sub_rw_log[1 << RS_SUB_RW_BITS];
static unsigned char rs_sub_rw_alog[1 << RS_SUB_RW_BITS];

static unsigned char *log, *alog;
static unsigned char mod;

/* generate galois field log and antilog tables */
static int init_rs_l12( void )
{
  unsigned char r;
  int i;

  r = RS_L12_PRIM_ELEM; /* primitive element a */
  for (i = 1; i < (1 << RS_L12_BITS)-1; i++) {
        r &= (1 << RS_L12_BITS)-1;
	rs_l12_alog[i] = r;
	rs_l12_log[r] = i;

	if (r & (1 << (RS_L12_BITS-1))) {
		r = (r << 1) ^ RS_L12_POLY;
	} else {
		r = r << 1;
	}
  }
  rs_l12_alog[0] = 1;

  /* print tables on standard output */
  printf("static unsigned char rs_l12_alog[%d] = {\n", (1 << RS_L12_BITS)-1);
  for (i = 0; i < (1 << RS_L12_BITS)-1; i++) {
        printf("%2d,", rs_l12_alog[i]);
  }
  printf("};\n");
  printf("static unsigned char rs_l12_log[%d] = {\n", (1 << RS_L12_BITS));
  for (i = 0; i < (1 << RS_L12_BITS); i++) {
        printf("%2d,", rs_l12_log[i]);
  }
  printf("};\n");
  return 0;
}

/* generate galois field log and antilog tables */
static int init_rs_sub_rw( void )
{
  unsigned char r;
  int i;

  r = RS_SUB_RW_PRIM_ELEM; /* primitive element a */
  for (i = 1; i < (1 << RS_SUB_RW_BITS)-1; i++) {
        r &= (1 << RS_SUB_RW_BITS)-1;
	rs_sub_rw_alog[i] = r;
	rs_sub_rw_log[r] = i;

	if (r & (1 << (RS_SUB_RW_BITS-1))) {
		r = r << 1 ^ RS_SUB_RW_POLY;
	} else {
		r = r << 1;
	}
  }
  rs_sub_rw_alog[0] = 1;

  /* print tables on standard output */
  printf("static unsigned char rs_sub_rw_alog[%d] = {\n", (1 << RS_SUB_RW_BITS)-1);
  for (i = 0; i < (1 << RS_SUB_RW_BITS)-1; i++) {
        printf("%2d,", rs_sub_rw_alog[i]);
  }
  printf("};\n");
  printf("static unsigned char rs_sub_rw_log[%d] = {\n", (1 << RS_SUB_RW_BITS)-1);
  for (i = 0; i < (1 << RS_SUB_RW_BITS)-1; i++) {
        printf("%2d,", rs_sub_rw_log[i]);
  }
  printf("};\n");
  return 0;
}

static int debug = 0;

#define DEBUG
/* print coefficients of the current matrix */
static void print_c(unsigned char **co, unsigned char datas, unsigned char parities)
{
#if defined(DEBUG)
  unsigned char P, D;

  if (debug != 1) return;

  for (P = 0; P < parities; P++) {
    fprintf(stderr,"%d:",P);
    for (D = 0; D < datas+parities; D++) {
      fprintf(stderr," %3d", co[P][D]);
    }
    fprintf(stderr,"\n");
  }
#endif
}

/* routines to solve systems of equations */

/* initialize matrix with 'parities' lines and 'datas+parities' columns
 ^  ...
 |  1 a^3 a^6 a^9 a^12  ...
 |  1 a^2 a^4 a^6 a^8   ...
 |  1 a^1 a^2 a^3 a^4   ...
 |  1   1   1   1   1   ...
 v  <- datas + parities  ->
 */
static unsigned char **init_matrix(int datas, int parities, 
				   unsigned char alog[], unsigned char mod)
{
  unsigned char P, D;
  unsigned char **coeff = malloc(sizeof(unsigned char *)*parities);

  if (coeff == NULL) {
    fprintf(stderr, "sorry, no memory for table generation. Aborted.\n");
    exit(1);
  }

  /* initialize */
  for (P = 0; P < parities; P++) {
    coeff[P] = malloc(datas+parities);
    if (coeff[P] == NULL) {
      fprintf(stderr, "sorry, no memory for table generation. Aborted.\n");
      exit(1);
    }

    for (D = 0; D < datas+parities; D++) {
      coeff[P][D] = alog[((parities-1-P) * D) % mod];
    }
  }
  return coeff;
}

/* do gaussian elimination in order to bring the matrix into a form of
   a  b  c ...
   0  d  e ...
   0  0  f ...
   0  0  0 ...
   ...
 */
static void gauss_elimination(int datas, int parities, unsigned char **coeff,
			      unsigned char mod, int shift)
{
  unsigned char P, D, E;

  /* gaussian elimination */
  for (E = 1; E < parities; E++) {
    for (P = E; P < parities; P++) {
      unsigned char fak = div(coeff[P][E-1+shift],coeff[E-1][E-1+shift], mod);
      for (D = E-1; D < datas+parities; D++) {
        coeff[P][D] = sub(coeff[P][D], mul(fak, coeff[E-1][D], mod));
      }
    }
  }
}

/* solve the triangelized equation system by recursive substitution
 */
static void solve_triangelized(int datas, int parities, unsigned char **coeff,
			       char *name, unsigned char mod, int shift)
{
  unsigned char P, D, E;
  unsigned char lower_bound = shift;

  printf("static unsigned char %s[%d][%d] = {\n", name, parities,datas);

  /* down-counting loops with wrapped end values! */
  for (P = parities-1; P < parities; P--) {
    printf("{");
    for (D = parities+datas-1; D < parities+datas && D >= P; D--) {
      coeff[P][D] = div(neg(coeff[P][D]),coeff[P][P+shift], mod);

      if (D < lower_bound || D >= (parities+shift))
        printf("%d,", log[coeff[P][D]]);

      if (P > 0) {
        unsigned char fak = coeff[P][D];
        for (E = P-1; E < P; E--) {
          coeff[E][D] = sub(coeff[E][D],mul(fak,coeff[E][P+shift], mod));
        }
      }
    }
    printf("},\n");
  }
  printf("};\n");
}

static void solve_tail(int datas, int parities, char *name, unsigned char mod)
{
  unsigned char **coeff;

  coeff = init_matrix(datas, parities, alog, mod);
print_c(coeff, datas, parities);

  gauss_elimination(datas, parities, coeff, mod, 0);
print_c(coeff, datas, parities);

  solve_triangelized(datas, parities, coeff, name, mod, 0);
print_c(coeff, datas, parities);

}

static int solve_sub_tail(int datas, int parities, char *name)
{
  alog = rs_sub_rw_alog;
  log = rs_sub_rw_log;
  mod = (1 << RS_SUB_RW_BITS)-1;

  solve_tail(datas, parities, name, mod);
  return 0;
}

static int solve_rs_tail(int datas, int parities, char *name)
{
  alog = rs_l12_alog;
  log = rs_l12_log;
  mod = (1 << RS_L12_BITS)-1;

  solve_tail(datas, parities, name, mod);
  return 0;
}

static int solve_rs_centered(int datas, int parities, char *name)
{
  unsigned char **coeff;

  alog = rs_l12_alog;
  log = rs_l12_log;
  mod = (1 << RS_L12_BITS)-1;

  coeff = init_matrix(datas, parities, alog, mod);
print_c(coeff, datas, parities);

  gauss_elimination(datas, parities, coeff, mod, datas/2);
print_c(coeff, datas, parities);

  solve_triangelized(datas, parities, coeff, name, mod, datas/2);
print_c(coeff, datas, parities);

  return 0;
}

#define L2_SCRAMBLER_PRESET 0x1
static void L2_scrambler(void)
{
  int i;

  unsigned short scr;
  unsigned char *l2_scrambler, *scrtabp;

  /* initialize Yellow Book scrambler table */
  l2_scrambler = calloc(2340 ,1);
  if (l2_scrambler == NULL) {
    fprintf(stderr, "sorry, no memory for scrambler table generation. Aborted.\n");
    exit(1);
  }

  scrtabp = l2_scrambler;
  scr = L2_SCRAMBLER_PRESET;
  printf("static unsigned char yellowbook_scrambler[2340] = {\n  ");
  for (i = 0; i < 2340; i++) {
    int j;
    for (j = 0; j < 8; j++) {
      if (scr & 1) {
        *scrtabp |= 1 << j;
      }
      if ((scr & 1) ^ ((scr >> 1) & 1)) {
        scr |= 1 << 15;
      }
      scr = scr >> 1;
    }
    printf("%d,", *scrtabp);
    if (((i+1) % 23) == 0) printf("\n  ");
    scrtabp++;
  }
  printf("\n};\n");
}

int main(int argc, char **argv)
{
  if (argc > 1) debug = 1;

  init_rs_l12();
  init_rs_sub_rw();

  /* generate encoding table for Subchannel Q */
  solve_sub_tail(2,2, "SQ");

  /* generate encoding table for Subchannel P */
  solve_sub_tail(20,4, "SP");

  /* generate encoding table for audio sectors Q */
  solve_rs_centered(24,4, "AQ");

  /* generate encoding table for audio sectors P */
  solve_rs_tail(32,4, "AP");

  /* generate encoding table for data sectors Q */
  solve_rs_tail(43,2, "DQ");

  /* generate encoding table for data sectors P */
  solve_rs_tail(24,2, "DP");

  /* generate the scrambler table for data sectors */
  L2_scrambler();
  return 0;
}

/*****************************************************************************
 * builtin_funcs.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          code from projectM http://xmms-projectm.sourceforge.net
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#include <math.h>
#include <stdlib.h>
#include <stdio.h>
/* Values to optimize the sigmoid function */
#define R  32767   
#define RR 65534   
 
static inline double int_wrapper(double * arg_list) {

  return floor(arg_list[0]);

}


static inline double sqr_wrapper(double * arg_list) {
	
	return pow(2, arg_list[0]);
}	
	
	
static inline double sign_wrapper(double * arg_list) {	
	
	return -arg_list[0];	
}	

static inline double min_wrapper(double * arg_list) {
	
	if (arg_list[0] > arg_list[1])
		return arg_list[1];
	
	return arg_list[0];
}		

static inline double max_wrapper(double * arg_list) {

	if (arg_list[0] > arg_list[1])
	  return arg_list[0];

	return arg_list[1];
}

/* consult your AI book */
static inline double sigmoid_wrapper(double * arg_list) {
  return (RR / (1 + exp( -(((double)(arg_list[0])) * arg_list[1]) / R) - R));
}
	
	
static inline double bor_wrapper(double * arg_list) {

	return (double)((int)arg_list[0] || (int)arg_list[1]);
}	
	
static inline double band_wrapper(double * arg_list) {
	return (double)((int)arg_list[0] && (int)arg_list[1]);
}	

static inline double bnot_wrapper(double * arg_list) {
	return (double)(!(int)arg_list[0]);
}		

static inline double if_wrapper(double * arg_list) {

		if ((int)arg_list[0] == 0)
			return arg_list[2];
		return arg_list[1];
}		


static inline double rand_wrapper(double * arg_list) {
  double l;

  //  printf("RAND ARG:(%d)\n", (int)arg_list[0]);
  l = (double)((rand()) % ((int)arg_list[0]));
  //printf("VAL: %f\n", l);
  return l;
}	

static inline double equal_wrapper(double * arg_list) {

	return (arg_list[0] == arg_list[1]);
}	


static inline double above_wrapper(double * arg_list) {

	return (arg_list[0] > arg_list[1]);
}	


static inline double below_wrapper(double * arg_list) {

	return (arg_list[0] < arg_list[1]);
}

static inline double sin_wrapper(double * arg_list) {
	return (sin (arg_list[0]));	
}


static inline double cos_wrapper(double * arg_list) {
	return (cos (arg_list[0]));
}

static inline double tan_wrapper(double * arg_list) {
	return (tan(arg_list[0]));
}

static inline double asin_wrapper(double * arg_list) {
	return (asin (arg_list[0]));
}

static inline double acos_wrapper(double * arg_list) {
	return (acos (arg_list[0]));
}

static inline double atan_wrapper(double * arg_list) {
	return (atan (arg_list[0]));
}

static inline double atan2_wrapper(double * arg_list) {
  return (atan2 (arg_list[0], arg_list[1]));
}

static inline double pow_wrapper(double * arg_list) {
  return (pow (arg_list[0], arg_list[1]));
}

static inline double exp_wrapper(double * arg_list) {
  return (exp(arg_list[0]));
}

static inline double abs_wrapper(double * arg_list) {
  return (fabs(arg_list[0]));
}

static inline double log_wrapper(double *arg_list) {
  return (log (arg_list[0]));
}

static inline double log10_wrapper(double * arg_list) {
  return (log10 (arg_list[0]));
}

static inline double sqrt_wrapper(double * arg_list) {
  return (sqrt (arg_list[0]));
}


static inline double nchoosek_wrapper(double * arg_list) {
      unsigned long cnm = 1UL;
      int i, f;
      int n, m;

      n = (int)arg_list[0];
      m = (int)arg_list[1];

      if (m*2 >n) m = n-m;
      for (i=1 ; i <= m; n--, i++)
      {
            if ((f=n) % i == 0)
                  f   /= i;
            else  cnm /= i;
            cnm *= f;
      }
      return (double)cnm;
}


static inline double fact_wrapper(double * arg_list) {


  int result = 1;
  
  int n = (int)arg_list[0];
  
  while (n > 1) {
    result = result * n;
    n--;
  }
  return (double)result;
}

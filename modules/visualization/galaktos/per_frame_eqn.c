/*****************************************************************************
 * per_frame_eqn.c:
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fatal.h"
#include "common.h"
#include "param.h"
#include "per_frame_eqn_types.h"
#include "per_frame_eqn.h"

#include "expr_types.h"
#include "eval.h"

/* Evaluate an equation */
void eval_per_frame_eqn(per_frame_eqn_t * per_frame_eqn) {

  if (per_frame_eqn == NULL)
    return;

     if (PER_FRAME_EQN_DEBUG) { 
		 printf("per_frame_%d=%s= ", per_frame_eqn->index, per_frame_eqn->param->name);
		 fflush(stdout);
	 }
	 
    //*((double*)per_frame_eqn->param->engine_val) = eval_gen_expr(per_frame_eqn->gen_expr);
	set_param(per_frame_eqn->param, eval_gen_expr(per_frame_eqn->gen_expr));
     if (PER_FRAME_EQN_DEBUG) printf(" = %.4f\n", *((double*)per_frame_eqn->param->engine_val)); 
		 
}

/*
void eval_per_frame_init_eqn(per_frame_eqn_t * per_frame_eqn) {

   double val;
   init_cond_t * init_cond;
   if (per_frame_eqn == NULL)
     return;

     if (PER_FRAME_EQN_DEBUG) { 
		 printf("per_frame_init: %s = ", per_frame_eqn->param->name);
		 fflush(stdout);
	 }
	 		
	
    val = *((double*)per_frame_eqn->param->engine_val) = eval_gen_expr(per_frame_eqn->gen_expr);
    if (PER_FRAME_EQN_DEBUG) printf(" = %f\n", *((double*)per_frame_eqn->param->engine_val)); 
     
	if (per_frame_eqn->param->flags & P_FLAG_QVAR) {
		
		per_frame_eqn->param->init_val.double_val = val;
		if ((init_cond = new_init_cond(per_frame_eqn->param)) == NULL)
			return;
		
		if ((list_append(init_cond_list, init_cond)) < 0) {
			free_init_cond(init_cond);
			return;
		}
    }
}
*/

/* Frees perframe equation structure */
void free_per_frame_eqn(per_frame_eqn_t * per_frame_eqn) {

	if (per_frame_eqn == NULL)
	  return;
	
  free_gen_expr(per_frame_eqn->gen_expr);
  free(per_frame_eqn);
}

/* Create a new per frame equation */
per_frame_eqn_t * new_per_frame_eqn(int index, param_t * param, gen_expr_t * gen_expr) {

  per_frame_eqn_t * per_frame_eqn;

  per_frame_eqn = (per_frame_eqn_t*)malloc(sizeof(per_frame_eqn_t));

  if (per_frame_eqn == NULL)
    return NULL;

  per_frame_eqn->param = param;
  per_frame_eqn->gen_expr = gen_expr;
  per_frame_eqn->index = index;
  /* Set per frame eqn name */
  //  memset(per_frame_eqn->name, 0, MAX_TOKEN_SIZE);
  //strncpy(per_frame_eqn->name, name, MAX_TOKEN_SIZE-1);
  return per_frame_eqn;

}

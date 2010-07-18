/*****************************************************************************
 * custom_wave.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id: a294cf5b1043d36e349a13471c9b93ee82c39b68 $
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
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "fatal.h"

#include "param_types.h"
#include "param.h"

#include "expr_types.h"
#include "eval.h"

#include "splaytree_types.h"
#include "splaytree.h"
#include "tree_types.h"

#include "per_frame_eqn_types.h"
#include "per_frame_eqn.h"

#include "init_cond_types.h"
#include "init_cond.h"

#include "preset_types.h"

#include "custom_wave_types.h"
#include "custom_wave.h"

#include "init_cond_types.h"
#include "init_cond.h"

#include "engine_vars.h"
#define MAX_SAMPLE_SIZE 4096

extern int mesh_i;

custom_wave_t * interface_wave = NULL;
int interface_id = 0;
extern preset_t * active_preset;
inline void eval_custom_wave_init_conds(custom_wave_t * custom_wave);
void load_unspec_init_cond(param_t * param);
void destroy_per_point_eqn_tree(splaytree_t * tree);
void destroy_param_db_tree(splaytree_t * tree);
void destroy_per_frame_eqn_tree(splaytree_t * tree);
void destroy_per_frame_init_eqn_tree(splaytree_t * tree);
void destroy_init_cond_tree(splaytree_t * tree);
inline void evalPerPointEqn(per_point_eqn_t * per_point_eqn);

custom_wave_t * new_custom_wave(int id) {

  custom_wave_t * custom_wave;
  param_t * param;
  
  if ((custom_wave = (custom_wave_t*)malloc(sizeof(custom_wave_t))) == NULL)
    return NULL;

  custom_wave->id = id;
  custom_wave->per_frame_count = 0;

  custom_wave->samples = 512;
  custom_wave->bSpectrum = 0;
  custom_wave->enabled = 1;
  custom_wave->sep = 1;
  custom_wave->smoothing = 0.0;
  custom_wave->bUseDots = 0;
  custom_wave->bAdditive = 0;
  custom_wave->r = custom_wave->g = custom_wave->b = custom_wave->a = 0.0;
  custom_wave->scaling = 1.0;
  custom_wave->per_frame_eqn_string_index = 0;
  custom_wave->per_frame_init_eqn_string_index = 0;
  custom_wave->per_point_eqn_string_index = 0;

  custom_wave->r_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->g_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->b_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->a_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->x_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->y_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->value1 = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->value2 = malloc(MAX_SAMPLE_SIZE*sizeof(double));
  custom_wave->sample_mesh = malloc(MAX_SAMPLE_SIZE*sizeof(double));

  /* Initialize tree data structures */
  
  if ((custom_wave->param_tree = 
       create_splaytree(compare_string, copy_string, free_string)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((custom_wave->per_point_eqn_tree = 
       create_splaytree(compare_int, copy_int, free_int)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((custom_wave->per_frame_eqn_tree = 
       create_splaytree(compare_int, copy_int, free_int)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((custom_wave->init_cond_tree = 
       create_splaytree(compare_string, copy_string, free_string)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  
  if ((custom_wave->per_frame_init_eqn_tree = 
       create_splaytree(compare_string, copy_string, free_string)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  
  /* Start: Load custom wave parameters */

  if ((param = new_param_double("r", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->r, custom_wave->r_mesh, 1.0, 0.0, .5)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
  if ((param = new_param_double("g", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->g,  custom_wave->g_mesh, 1.0, 0.0, .5)) == NULL){
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("b", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->b,  custom_wave->b_mesh, 1.0, 0.0, .5)) == NULL){
    free_custom_wave(custom_wave);
    return NULL;				       
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("a", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->a,  custom_wave->a_mesh, 1.0, 0.0, .5)) == NULL){
    free_custom_wave(custom_wave);
    return NULL;
  }
  
  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("x", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->x,  custom_wave->x_mesh, 1.0, 0.0, .5)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("y", P_FLAG_DONT_FREE_MATRIX | P_FLAG_PER_POINT, &custom_wave->y,  custom_wave->y_mesh, 1.0, 0.0, .5)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_bool("enabled", P_FLAG_NONE, &custom_wave->enabled, 1, 0, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_int("sep", P_FLAG_NONE, &custom_wave->sep, 100, -100, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_bool("bSpectrum", P_FLAG_NONE, &custom_wave->bSpectrum, 1, 0, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_bool("bDrawThick", P_FLAG_NONE, &custom_wave->bDrawThick, 1, 0, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_bool("bUseDots", P_FLAG_NONE, &custom_wave->bUseDots, 1, 0, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
  if ((param = new_param_bool("bAdditive", P_FLAG_NONE, &custom_wave->bAdditive, 1, 0, 0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_int("samples", P_FLAG_NONE, &custom_wave->samples, 2048, 1, 512)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("sample", P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX | P_FLAG_ALWAYS_MATRIX | P_FLAG_PER_POINT,
				&custom_wave->sample, custom_wave->sample_mesh, 1.0, 0.0, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
 if (insert_param(param, custom_wave->param_tree) < 0) {
    printf("failed to insert sample\n");
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("value1", P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX | P_FLAG_ALWAYS_MATRIX | P_FLAG_PER_POINT, &custom_wave->v1, custom_wave->value1, 1.0, -1.0, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("value2", P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX | P_FLAG_ALWAYS_MATRIX | P_FLAG_PER_POINT, &custom_wave->v2, custom_wave->value2, 1.0, -1.0, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("smoothing", P_FLAG_NONE, &custom_wave->smoothing, NULL, 1.0, 0.0, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("scaling", P_FLAG_NONE, &custom_wave->scaling, NULL, MAX_DOUBLE_SIZE, 0.0, 1.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
  if ((param = new_param_double("t1", P_FLAG_PER_POINT | P_FLAG_TVAR, &custom_wave->t1, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("t2",  P_FLAG_PER_POINT |P_FLAG_TVAR, &custom_wave->t2, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("t3",  P_FLAG_PER_POINT |P_FLAG_TVAR, &custom_wave->t3, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  if ((param = new_param_double("t4",  P_FLAG_PER_POINT |P_FLAG_TVAR, &custom_wave->t4, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  if ((param = new_param_double("t5", P_FLAG_TVAR, &custom_wave->t5, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }
 
  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  if ((param = new_param_double("t6", P_FLAG_TVAR | P_FLAG_PER_POINT, &custom_wave->t6, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  if ((param = new_param_double("t7", P_FLAG_TVAR | P_FLAG_PER_POINT, &custom_wave->t7, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if ((param = new_param_double("t8", P_FLAG_TVAR | P_FLAG_PER_POINT, &custom_wave->t8, NULL, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, 0.0)) == NULL) {
    free_custom_wave(custom_wave);
    return NULL;
  }

  if (insert_param(param, custom_wave->param_tree) < 0) {
    free_custom_wave(custom_wave);
    return NULL;
  }
  
  /* End of parameter loading. Note that the read only parameters associated
     with custom waves (ie, sample) are global variables, and not specific to 
     the custom wave datastructure. */


  return custom_wave;

}

void destroy_per_frame_init_eqn_tree(splaytree_t * tree) {

  if (!tree)
    return;

  splay_traverse(free_init_cond, tree);
  destroy_splaytree(tree);

}


void destroy_per_point_eqn_tree(splaytree_t * tree) {

  if (!tree)
    return;

  splay_traverse(free_per_point_eqn, tree);
  destroy_splaytree(tree);

}

void destroy_init_cond_tree(splaytree_t * tree) {

  if (!tree)
    return;

  splay_traverse(free_init_cond, tree);
  destroy_splaytree(tree);

}

void destroy_per_frame_eqn_tree(splaytree_t * tree) {


  if (!tree)
    return;

  splay_traverse(free_per_frame_eqn, tree);
  destroy_splaytree(tree);

}


void destroy_param_db_tree(splaytree_t * tree) {

  if (!tree)
    return;

  splay_traverse(free_param, tree);
  destroy_splaytree(tree);

}

/* Frees a custom wave form object */
void free_custom_wave(custom_wave_t * custom_wave) {

  if (custom_wave == NULL)
    return;

  if (custom_wave->param_tree == NULL)
    return;

  destroy_per_point_eqn_tree(custom_wave->per_point_eqn_tree);
  destroy_per_frame_eqn_tree(custom_wave->per_frame_eqn_tree);
  destroy_init_cond_tree(custom_wave->init_cond_tree);
  destroy_param_db_tree(custom_wave->param_tree);
  destroy_per_frame_init_eqn_tree(custom_wave->per_frame_init_eqn_tree);

  free(custom_wave->r_mesh);
  free(custom_wave->g_mesh);
  free(custom_wave->b_mesh);
  free(custom_wave->a_mesh);
  free(custom_wave->x_mesh);
  free(custom_wave->y_mesh);
  free(custom_wave->value1);
  free(custom_wave->value2);
  free(custom_wave->sample_mesh);

  free(custom_wave);

  return;

}



int add_per_point_eqn(char * name, gen_expr_t * gen_expr, custom_wave_t * custom_wave) {

  per_point_eqn_t * per_point_eqn;
  int index;
  param_t * param = NULL;

  /* Argument checks */
  if (custom_wave == NULL)
	  return FAILURE;
  if (gen_expr == NULL)
	  return FAILURE;
  if (name == NULL)
	  return FAILURE;
  
 if (CUSTOM_WAVE_DEBUG) printf("add_per_point_eqn: per pixel equation (name = \"%s\")\n", name);

 /* Search for the parameter so we know what matrix the per pixel equation is referencing */

 if ((param = find_param_db(name, custom_wave->param_tree, TRUE)) == NULL) {
   if (CUSTOM_WAVE_DEBUG) printf("add_per_point_eqn: failed to allocate a new parameter!\n");
   return FAILURE;
 
 } 	 

 /* Find most largest index in the splaytree */
 if ((per_point_eqn = splay_find_max(custom_wave->per_point_eqn_tree)) == NULL)
   index = 0;
 else
   index = per_point_eqn->index+1;

 /* Create the per pixel equation given the index, parameter, and general expression */
 if ((per_point_eqn = new_per_point_eqn(index, param, gen_expr)) == NULL)
	 return FAILURE;
 if (CUSTOM_WAVE_DEBUG) 
   printf("add_per_point_eqn: created new equation (index = %d) (name = \"%s\")\n", per_point_eqn->index, per_point_eqn->param->name);
 /* Insert the per pixel equation into the preset per pixel database */
 if (splay_insert(per_point_eqn, &per_point_eqn->index, custom_wave->per_point_eqn_tree) < 0) {
	free_per_point_eqn(per_point_eqn);
	return FAILURE;	 
 }
	 
 /* Done */ 
 return SUCCESS;
}

per_point_eqn_t * new_per_point_eqn(int index, param_t * param, gen_expr_t * gen_expr) {

	per_point_eqn_t * per_point_eqn;
	
	if (param == NULL)
		return NULL;
	if (gen_expr == NULL)
		return NULL;

	if ((per_point_eqn = (per_point_eqn_t*)malloc(sizeof(per_point_eqn_t))) == NULL)
		return NULL;

      
	per_point_eqn->index = index;
	per_point_eqn->gen_expr = gen_expr;
	per_point_eqn->param = param;
	return per_point_eqn;	
}


void free_per_point_eqn(per_point_eqn_t * per_point_eqn) {

	if (per_point_eqn == NULL)
		return;
	
	free_gen_expr(per_point_eqn->gen_expr);
	
	free(per_point_eqn);
	
	return;
}

custom_wave_t * find_custom_wave(int id, preset_t * preset, int create_flag) {

  custom_wave_t * custom_wave = NULL;

  if (preset == NULL)
    return NULL;
  
  if ((custom_wave = splay_find(&id, preset->custom_wave_tree)) == NULL) {

    if (CUSTOM_WAVE_DEBUG) { printf("find_custom_wave: creating custom wave (id = %d)...", id);fflush(stdout);}

    if (create_flag == FALSE) {
      if (CUSTOM_WAVE_DEBUG) printf("you specified not to (create flag = false), returning null\n");
       return NULL;
    }

    if ((custom_wave = new_custom_wave(id)) == NULL) {
      if (CUSTOM_WAVE_DEBUG) printf("failed...out of memory?\n");
      return NULL;
    }

    if  (CUSTOM_WAVE_DEBUG) {printf("success.Inserting..."); fflush(stdout);}

   if (splay_insert(custom_wave, &custom_wave->id, preset->custom_wave_tree) < 0) {
     if (CUSTOM_WAVE_DEBUG) printf("failed!\n");
     free_custom_wave(custom_wave);
     return NULL;
    }
 
   if (CUSTOM_WAVE_DEBUG) printf("done.\n");
  }
 
  return custom_wave;
}

inline void evalCustomWaveInitConditions() {
  splay_traverse(eval_custom_wave_init_conds, active_preset->custom_wave_tree);
}

inline void eval_custom_wave_init_conds(custom_wave_t * custom_wave) {
  splay_traverse(eval_init_cond, custom_wave->init_cond_tree);
  splay_traverse(eval_init_cond, custom_wave->per_frame_init_eqn_tree);
}

/* Interface function. Makes another custom wave the current
   concern for per frame / point equations */
inline custom_wave_t * nextCustomWave() {

  if ((interface_wave = splay_find(&interface_id, active_preset->custom_wave_tree)) == NULL) {
    interface_id = 0;
    return NULL;
  }

  interface_id++;

  /* Evaluate all per frame equations associated with this wave */
  splay_traverse(eval_per_frame_eqn, interface_wave->per_frame_eqn_tree);
  return interface_wave;
}


inline void evalPerPointEqns() { 

  int x;

  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->r_mesh[x] = interface_wave->r;
  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->g_mesh[x] = interface_wave->g;
  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->b_mesh[x] = interface_wave->b;
  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->a_mesh[x] = interface_wave->a;
  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->x_mesh[x] = interface_wave->x;
  for (x = 0; x < interface_wave->samples; x++)
    interface_wave->y_mesh[x] = interface_wave->y;

 
  /* Evaluate per pixel equations */
  splay_traverse(evalPerPointEqn, interface_wave->per_point_eqn_tree);

  /* Reset index */
  mesh_i = -1;
}

/* Evaluates a per point equation for the current custom wave given by interface_wave ptr */
inline void evalPerPointEqn(per_point_eqn_t * per_point_eqn) {
  
  
  int samples, size;
  double * param_matrix;
  gen_expr_t * eqn_ptr;

  samples = interface_wave->samples;
  eqn_ptr = per_point_eqn->gen_expr;
 
  if (per_point_eqn->param->matrix == NULL) {
    if ((param_matrix = per_point_eqn->param->matrix = malloc(size = samples*sizeof(double))) == NULL)
      return;
    memset(param_matrix, 0, size);
  }
  else 
    param_matrix = (double*)per_point_eqn->param->matrix;
  
  for (mesh_i = 0; mesh_i < samples; mesh_i++) {    
      param_matrix[mesh_i] = eval_gen_expr(eqn_ptr);
  }
  
  /* Now that this parameter has been referenced with a per
     point equation, we let the evaluator know by setting
     this flag */
  per_point_eqn->param->matrix_flag = 1; 

}


void load_unspecified_init_conds(custom_wave_t * custom_wave) {

  interface_wave = custom_wave;
  splay_traverse(load_unspec_init_cond, interface_wave->param_tree);
  interface_wave = NULL;
 
}

void load_unspec_init_cond(param_t * param) {

  init_cond_t * init_cond;
  value_t init_val;

  /* Don't count these parameters as initial conditions */
  if (param->flags & P_FLAG_READONLY)
    return;
  if (param->flags & P_FLAG_QVAR)
    return;
  if (param->flags & P_FLAG_TVAR)
    return;
  if (param->flags & P_FLAG_USERDEF)
    return;

  /* If initial condition was not defined by the preset file, force a default one
     with the following code */
  if ((init_cond = splay_find(param->name, interface_wave->init_cond_tree)) == NULL) {
    
    /* Make sure initial condition does not exist in the set of per frame initial equations */
    if ((init_cond = splay_find(param->name, interface_wave->per_frame_init_eqn_tree)) != NULL)
      return;
    
    if (param->type == P_TYPE_BOOL)
      init_val.bool_val = 0;
    
    else if (param->type == P_TYPE_INT)
      init_val.int_val = *(int*)param->engine_val;

    else if (param->type == P_TYPE_DOUBLE)
      init_val.double_val = *(double*)param->engine_val;

    //printf("%s\n", param->name);
    /* Create new initial condition */
    if ((init_cond = new_init_cond(param, init_val)) == NULL)
      return;
    
    /* Insert the initial condition into this presets tree */
    if (splay_insert(init_cond, init_cond->param->name, interface_wave->init_cond_tree) < 0) {
      free_init_cond(init_cond);
      return;
    }
    
  }
 
}

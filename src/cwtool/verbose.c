/****************************************************************************
 ****************************************************************************
 *
 * verbose.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "verbose.h"
#include "error.h"




/****************************************************************************
 *
 * local data structures, variables and defines
 *
 ****************************************************************************/




static cw_count_t			verbose_levels[VERBOSE_NR_CLASSES];




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * verbose_set_level
 ****************************************************************************/
cw_void_t
verbose_set_level(
	cw_index_t			class,
	cw_count_t			level)

	{
	error_condition((class < 0) || (class >= VERBOSE_NR_CLASSES));
	if (level < VERBOSE_LEVEL_NONE) level = VERBOSE_LEVEL_NONE;
	if (level > VERBOSE_LEVEL_ALL)  level = VERBOSE_LEVEL_ALL;
	verbose_levels[class] = level;
	}



/****************************************************************************
 * verbose_set_all_levels
 ****************************************************************************/
cw_void_t
verbose_set_all_levels(
	cw_count_t			level)

	{
	cw_index_t			c;

	for (c = 0; c < VERBOSE_NR_CLASSES; c++) verbose_set_level(c, level);
	}



/****************************************************************************
 * verbose_get_level
 ****************************************************************************/
cw_count_t
verbose_get_level(
	cw_index_t			class)

	{
	error_condition((class < 0) || (class >= VERBOSE_NR_CLASSES));
	return (verbose_levels[class]);
	}
/******************************************************** Karsten Scheibler */

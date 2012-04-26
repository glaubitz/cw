/****************************************************************************
 ****************************************************************************
 *
 * debug.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdarg.h>

#include "debug.h"
#include "error.h"




/****************************************************************************
 *
 * local data structures, variables and defines
 *
 ****************************************************************************/




static cw_bool_t			debug_enabled;
static cw_count_t			debug_levels[DEBUG_NR_CLASSES];




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * debug_message2
 ****************************************************************************/
cw_void_t
debug_message2(
	const cw_char_t			*file,
	cw_count_t			line,
	const cw_char_t			*format,
	...)

	{
	va_list				args;

	va_start(args, format);
	if (debug_enabled) fprintf(stderr, "%s:%d: ", file, line);
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
	va_end(args);
	}



/****************************************************************************
 * debug_set_level
 ****************************************************************************/
cw_void_t
debug_set_level(
	cw_index_t			class,
	cw_count_t			level)

	{
	cw_index_t			c;

	error_condition((class < 0) || (class >= DEBUG_NR_CLASSES));
	if (level < DEBUG_LEVEL_NONE) level = DEBUG_LEVEL_NONE;
	if (level > DEBUG_LEVEL_ALL)  level = DEBUG_LEVEL_ALL;
	debug_levels[class] = level;

	/*
	 * if level == 0 look if other levels are > 0 to set debug_enabled.
	 * this is needed for verbose, because it also calls debug_message2()
	 */

	if (level <= DEBUG_LEVEL_NONE)
		{
		debug_enabled = CW_BOOL_FALSE;
		for (c = 0; c < DEBUG_NR_CLASSES; c++)
			{
			if (debug_get_level(c) <= DEBUG_LEVEL_NONE) continue;
			debug_enabled = CW_BOOL_TRUE;
			break;
			}
		}
	else debug_enabled = CW_BOOL_TRUE;
	}



/****************************************************************************
 * debug_set_all_levels
 ****************************************************************************/
cw_void_t
debug_set_all_levels(
	cw_count_t			level)

	{
	cw_index_t			c;

	for (c = 0; c < DEBUG_NR_CLASSES; c++) debug_set_level(c, level);
	}



/****************************************************************************
 * debug_get_level
 ****************************************************************************/
cw_count_t
debug_get_level(
	cw_index_t			class)

	{
	error_condition((class < 0) || (class >= DEBUG_NR_CLASSES));
	return (debug_levels[class]);
	}
/******************************************************** Karsten Scheibler */

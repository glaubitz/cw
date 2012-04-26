/****************************************************************************
 ****************************************************************************
 *
 * verbose.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_VERBOSE_H
#define CWTOOL_VERBOSE_H

#include "types.h"
#include "debug.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define VERBOSE_CLASS_GENERIC		0
#define VERBOSE_CLASS_CWTOOL_ILRW	1
#define VERBOSE_CLASS_CWTOOL_S		2
#define VERBOSE_NR_CLASSES		3

#define VERBOSE_LEVEL_NONE		0
#define VERBOSE_LEVEL_1			1
#define VERBOSE_LEVEL_2			2
#define VERBOSE_LEVEL_3			3
#define VERBOSE_LEVEL_4			4
#define VERBOSE_LEVEL_ALL		4
#define VERBOSE_NR_LEVELS		5




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_void_t
verbose_set_level(
	cw_index_t			class,
	cw_count_t			level);

extern cw_void_t
verbose_set_all_levels(
	cw_count_t			level);

extern cw_count_t
verbose_get_level(
	cw_index_t			class);

#define verbose_message(class, level, msg...)			\
	do							\
		{						\
		if (verbose_get_level(VERBOSE_CLASS_ ##class) < VERBOSE_LEVEL_ ##level) break;	\
		debug_message2(__FILE__, __LINE__, msg);	\
		}						\
	while (0)



#endif /* !CWTOOL_VERBOSE_H */
/******************************************************** Karsten Scheibler */

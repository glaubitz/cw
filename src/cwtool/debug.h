/****************************************************************************
 ****************************************************************************
 *
 * debug.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_DEBUG_H
#define CWTOOL_DEBUG_H

#include "types.h"
#include "error.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define DEBUG_CLASS_GENERIC		0
#define DEBUG_NR_CLASSES		1

#define DEBUG_LEVEL_NONE		0
#define DEBUG_LEVEL_1			1
#define DEBUG_LEVEL_2			2
#define DEBUG_LEVEL_3			3
#define DEBUG_LEVEL_ALL			3
#define DEBUG_NR_LEVELS			4




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_void_t
debug_message2(
	const cw_char_t			*file,
	cw_count_t			line,
	const cw_char_t			*format,
	...);

extern cw_void_t
debug_set_level(
	cw_index_t			class,
	cw_count_t			level);

extern cw_void_t
debug_set_all_levels(
	cw_count_t			level);

extern cw_count_t
debug_get_level(
	cw_index_t			class);

#ifdef CWTOOL_DEBUG
#define debug_compiled_in		1

#define debug_message(class, level, msg...)				\
	do								\
		{							\
		if (debug_get_level(DEBUG_CLASS_ ##class) < DEBUG_LEVEL_ ##level) break;	\
		debug_message2(__FILE__, __LINE__, msg);		\
		}							\
	while (0)

#define debug_error()							\
	do								\
		{							\
		debug_message2(__FILE__, __LINE__, "debug error");	\
		error_exit();						\
		}							\
	while (0)

#define debug_error_condition(cond)				\
	do							\
		{						\
		if (! (cond)) break;				\
		debug_message2(__FILE__, __LINE__, #cond);	\
		error_exit();					\
		}						\
	while (0)
#else /* CWTOOL_DEBUG */
#define debug_compiled_in		0
#define debug_message(c, l, m...)	while (0)
#define debug_generic(m...)		while (0)
#define debug_error()			while (0)
#define debug_error_condition(c)	while (0)
#endif /* CWTOOL_DEBUG */

#define debug_msg(c, l, m...)		debug_message(c, l, m)



#endif /* !CWTOOL_DEBUG_H */
/******************************************************** Karsten Scheibler */

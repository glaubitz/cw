/****************************************************************************
 ****************************************************************************
 *
 * error.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_ERROR_H
#define CWTOOL_ERROR_H

#include "types.h"




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_void_t
error_exit(
	cw_void_t);

#define ERROR_FLAG_NONE			0
#define ERROR_FLAG_EXIT			(1 << 0)
#define ERROR_FLAG_PERROR		(1 << 1)

extern cw_void_t
error_message2(
	cw_flag_t			flags,
	const cw_char_t			*prepend,
	const cw_char_t			*append,
	const cw_char_t			*format,
	...);

extern cw_void_t
error_oom(
	cw_void_t);

extern cw_void_t
error_internal(
	cw_void_t);

extern cw_void_t
error_perror(
	cw_void_t);

#define error_warning(m...)		error_message2(ERROR_FLAG_NONE, NULL, NULL, m)
#define error_message(m...)		error_message2(ERROR_FLAG_EXIT,  NULL, NULL, m)
#define error_perror_message(m...)	error_message2(ERROR_FLAG_EXIT | ERROR_FLAG_PERROR,  NULL, NULL, m)

#define error_condition(cond)						\
	do								\
		{							\
		if (cond) error_message("%s:%d: internal error: %s", __FILE__, __LINE__, #cond);	\
		}							\
	while (0)

#define error_condition_message(cond, msg...)				\
	do								\
		{							\
		if (cond) error_message(msg);				\
		}							\
	while (0)

#define error_msg(m...)			error_message(m)
#define error_perror_msg(m...)		error_perror_message(m)
#define error_condition_msg(c, m...)	error_condition_message(c, m)



#endif /* !CWTOOL_ERROR_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * error.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "global.h"




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * error_exit
 ****************************************************************************/
cw_void_t
error_exit(
	cw_void_t)

	{
	exit(1);
	}



/****************************************************************************
 * error_message2
 ****************************************************************************/
cw_void_t
error_message2(
	cw_flag_t			flags,
	const cw_char_t			*prepend,
	const cw_char_t			*append,
	const cw_char_t			*format,
	...)

	{
	va_list				args;
	const cw_char_t			empty[] = "";

	va_start(args, format);
	if (prepend == NULL) prepend = empty;
	if (append == NULL) append = empty;
	if (format != NULL)
		{
		fprintf(stderr, "%s: %s", global_program_name(), prepend);
		vfprintf(stderr, format, args);
		fprintf(stderr, "%s\n", append);
		}
	va_end(args);
	if (flags & ERROR_FLAG_PERROR) fprintf(stderr, "%s: %s\n", global_program_name(), strerror(errno));
	if (flags & ERROR_FLAG_EXIT) error_exit();
	}



/****************************************************************************
 * error_oom
 ****************************************************************************/
cw_void_t
error_oom(
	cw_void_t)

	{
	error_message2(ERROR_FLAG_EXIT, NULL, NULL, "could not allocate memory");
	}



/****************************************************************************
 * error_internal
 ****************************************************************************/
cw_void_t
error_internal(
	cw_void_t)

	{
	error_message2(ERROR_FLAG_EXIT, NULL, NULL, "internal error");
	}



/****************************************************************************
 * error_perror
 ****************************************************************************/
cw_void_t
error_perror(
	cw_void_t)

	{
	error_message2(ERROR_FLAG_EXIT | ERROR_FLAG_PERROR, NULL, NULL, NULL);
	}
/******************************************************** Karsten Scheibler */

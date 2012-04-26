/****************************************************************************
 ****************************************************************************
 *
 * string.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "string.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"



/****************************************************************************
 * string_equal
 ****************************************************************************/
cw_bool_t
string_equal(
	const cw_char_t			*string1,
	const cw_char_t			*string2)

	{
	return ((strcmp(string1, string2) == 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE);
	}



/****************************************************************************
 * string_equal2
 ****************************************************************************/
cw_bool_t
string_equal2(
	const cw_char_t			*string1,
	const cw_char_t			*string2,
	const cw_char_t			*string3)

	{
	if (string_equal(string1, string2)) return (CW_BOOL_TRUE);
	if (string_equal(string1, string3)) return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * string_length
 ****************************************************************************/
cw_count_t
string_length(
	const cw_char_t			*string)

	{
	return (strlen(string));
	}



/****************************************************************************
 * string_copy
 ****************************************************************************/
cw_char_t *
string_copy(
	cw_char_t			*dst,
	cw_size_t			size,
	const cw_char_t			*src)

	{
	if (strlen(src) >= size) error_message("string_copy() size exceeded");
	strcpy(dst, src);
	return (dst);
	}



/****************************************************************************
 * string_snprintf
 ****************************************************************************/
cw_count_t
string_snprintf(
	cw_char_t			*string,
	cw_size_t			size,
	const cw_char_t			*format,
	...)

	{
	va_list				args;
	cw_count_t			len;

	va_start(args, format);
	len = vsnprintf(string, size, format, args);
	va_end(args);
	if ((len == -1) || (len >= size)) error_message("string_snprintf() size exceeded");
	return (len);
	}



/****************************************************************************
 * string_dot
 ****************************************************************************/
cw_char_t *
string_dot(
	cw_char_t			*dst,
	cw_size_t			size,
	cw_char_t			*src)

	{
	cw_index_t			i;

	if (size < 4) error_message("string_dot: size < 4");
	if (strlen(src) < size) return (src);
	for (i = 0; i < size - 4; i++) dst[i] = src[i];
	dst[size - 4] = '.';
	dst[size - 3] = '.';
	dst[size - 2] = '.';
	dst[size - 1] = '\0';
	return (dst);
	}
/******************************************************** Karsten Scheibler */

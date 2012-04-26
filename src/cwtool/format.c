/****************************************************************************
 ****************************************************************************
 *
 * format.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "format.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "string.h"



static struct format_desc		*fmt_dsc[] =
	{
	&raw_format_desc,
	&gcr_cbm_format_desc,
	&gcr_g64_format_desc,
	&gcr_apple_format_desc,
	&gcr_apple_test_format_desc,
	&gcr_v9000_format_desc,
	&fm_nec765_format_desc,
	&mfm_nec765_format_desc,
	&mfm_amiga_format_desc,
	&tbe_cw_format_desc,
	&fill_format_desc,
	NULL
	};



/****************************************************************************
 * format_search_desc
 ****************************************************************************/
struct format_desc *
format_search_desc(
	const char			*name)

	{
	int				i;

	for (i = 0; fmt_dsc[i] != NULL; i++) if (string_equal(name, fmt_dsc[i]->name)) break;
	return (fmt_dsc[i]);
	}



/****************************************************************************
 * format_search_option
 ****************************************************************************/
struct format_option *
format_search_option(
	struct format_option		*fmt_opt,
	const char			*name)

	{
	int				i;

	for (i = 0; fmt_opt[i].name != NULL; i++) if (string_equal(name, fmt_opt[i].name)) return (&fmt_opt[i]);
	return (NULL);
	}



/****************************************************************************
 * format_option_is_obsolete
 ****************************************************************************/
cw_bool_t
format_option_is_obsolete(
	struct format_option		*fmt_opt)

	{
	return (fmt_opt->flags & FORMAT_OPTION_FLAG_OBSOLETE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * format_compare2
 ****************************************************************************/
int
format_compare2(
	const char			*string,
	unsigned long			val1,
	unsigned long			val2)

	{
	verbose_message(GENERIC, 3, string, val1, val2);
	return ((val1 != val2) ? 1 : 0);
	}



/****************************************************************************
 * format_compare3
 ****************************************************************************/
int
format_compare3(
	const char			*string,
	unsigned long			val1,
	unsigned long			val2,
	unsigned long			val3)

	{
	verbose_message(GENERIC, 3, string, val1, val2, val3);
	return (((val1 != val2) && (val1 != val3)) ? 1 : 0);
	}
/******************************************************** Karsten Scheibler */

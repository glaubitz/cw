/****************************************************************************
 ****************************************************************************
 *
 * options.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "options.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




static struct options			opt =
	{
	.disk_track_start   = 0,
	.disk_track_end     = GLOBAL_NR_TRACKS - 1,
	.output_track_start = 0,
	.output_track_end   = GLOBAL_NR_TRACKS - 1,
	.track_size_limit   = GLOBAL_MAX_TRACK_SIZE
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * options_set_histogram_exponential
 ****************************************************************************/
cw_bool_t
options_set_histogram_exponential(
	cw_bool_t			value)

	{
	opt.histogram_exponential = (value != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_histogram_exponential
 ****************************************************************************/
cw_bool_t
options_get_histogram_exponential(
	cw_void_t)

	{
	return (opt.histogram_exponential);
	}



/****************************************************************************
 * options_set_histogram_context
 ****************************************************************************/
cw_bool_t
options_set_histogram_context(
	cw_bool_t			value)

	{
	opt.histogram_context = (value != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_histogram_context
 ****************************************************************************/
cw_bool_t
options_get_histogram_context(
	cw_void_t)

	{
	return (opt.histogram_context);
	}



/****************************************************************************
 * options_set_always_initialize
 ****************************************************************************/
cw_bool_t
options_set_always_initialize(
	cw_bool_t			value)

	{
	opt.always_initialize = (value != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_always_initialize
 ****************************************************************************/
cw_bool_t
options_get_always_initialize(
	cw_void_t)

	{
	return (opt.always_initialize);
	}



/****************************************************************************
 * options_set_clock_adjust
 ****************************************************************************/
cw_bool_t
options_set_clock_adjust(
	cw_bool_t			value)

	{
	opt.clock_adjust = (value != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_clock_adjust
 ****************************************************************************/
cw_bool_t
options_get_clock_adjust(
	cw_void_t)

	{
	return (opt.clock_adjust);
	}



/****************************************************************************
 * options_set_output
 ****************************************************************************/
cw_bool_t
options_set_output(
	cw_bool_t			value)

	{
	opt.output = (value != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_output
 ****************************************************************************/
cw_bool_t
options_get_output(
	cw_void_t)

	{
	return (opt.output);
	}



/****************************************************************************
 * options_set_output_track_start
 ****************************************************************************/
cw_bool_t
options_set_output_track_start(
	cw_count_t			track)

	{
	if ((track < 0) || (track > opt.output_track_end)) return (CW_BOOL_FAIL);
	opt.output_track_start = track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_output_track_start
 ****************************************************************************/
cw_count_t
options_get_output_track_start(
	cw_void_t)

	{
	return (opt.output_track_start);
	}



/****************************************************************************
 * options_set_output_track_end
 ****************************************************************************/
cw_bool_t
options_set_output_track_end(
	cw_count_t			track)

	{
	if ((track < opt.output_track_start) || (track >= GLOBAL_NR_TRACKS)) return (CW_BOOL_FAIL);
	opt.output_track_end = track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_output_track_end
 ****************************************************************************/
cw_count_t
options_get_output_track_end(
	cw_void_t)

	{
	return (opt.output_track_end);
	}



/****************************************************************************
 * options_set_disk_track_start
 ****************************************************************************/
cw_bool_t
options_set_disk_track_start(
	cw_count_t			track)

	{
	if ((track < 0) || (track > opt.disk_track_end)) return (CW_BOOL_FAIL);
	opt.disk_track_start = track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_disk_track_start
 ****************************************************************************/
cw_count_t
options_get_disk_track_start(
	cw_void_t)

	{
	return (opt.disk_track_start);
	}



/****************************************************************************
 * options_set_disk_track_end
 ****************************************************************************/
cw_bool_t
options_set_disk_track_end(
	cw_count_t			track)

	{
	if ((track < opt.disk_track_start) || (track >= GLOBAL_NR_TRACKS)) return (CW_BOOL_FAIL);
	opt.disk_track_end = track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_disk_track_end
 ****************************************************************************/
cw_count_t
options_get_disk_track_end(
	cw_void_t)

	{
	return (opt.disk_track_end);
	}



/****************************************************************************
 * options_set_track_size_limit
 ****************************************************************************/
cw_bool_t
options_set_track_size_limit(
	cw_count_t			limit)

	{
	if ((limit < GLOBAL_MIN_TRACK_SIZE) || (limit > GLOBAL_MAX_TRACK_SIZE)) return (CW_BOOL_FAIL);
	opt.track_size_limit = limit;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * options_get_track_size_limit
 ****************************************************************************/
cw_count_t
options_get_track_size_limit(
	cw_void_t)

	{
	return (opt.track_size_limit);
	}
/******************************************************** Karsten Scheibler */

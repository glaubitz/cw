/****************************************************************************
 ****************************************************************************
 *
 * options.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_OPTIONS_H
#define CWTOOL_OPTIONS_H

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct options
	{
	cw_bool_t			histogram_exponential;
	cw_bool_t			histogram_context;
	cw_bool_t			always_initialize;
	cw_bool_t			clock_adjust;
	cw_bool_t			output;
	cw_count_t			disk_track_start;
	cw_count_t			disk_track_end;
	cw_count_t			output_track_start;
	cw_count_t			output_track_end;
	cw_count_t			track_size_limit;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_bool_t
options_set_histogram_exponential(
	cw_bool_t			value);

extern cw_bool_t
options_get_histogram_exponential(
	cw_void_t);

extern cw_bool_t
options_set_histogram_context(
	cw_bool_t			value);

extern cw_bool_t
options_get_histogram_context(
	cw_void_t);

extern cw_bool_t
options_set_always_initialize(
	cw_bool_t			value);

extern cw_bool_t
options_get_always_initialize(
	cw_void_t);

extern cw_bool_t
options_set_clock_adjust(
	cw_bool_t			value);

extern cw_bool_t
options_get_clock_adjust(
	cw_void_t);

extern cw_bool_t
options_set_output(
	cw_bool_t			value);

extern cw_bool_t
options_get_output(
	cw_void_t);

extern cw_bool_t
options_set_disk_track_start(
	cw_count_t			track);

extern cw_count_t
options_get_disk_track_start(
	cw_void_t);

extern cw_bool_t
options_set_disk_track_end(
	cw_count_t			track);

extern cw_count_t
options_get_disk_track_end(
	cw_void_t);

extern cw_bool_t
options_set_output_track_start(
	cw_count_t			track);

extern cw_count_t
options_get_output_track_start(
	cw_void_t);

extern cw_bool_t
options_set_output_track_end(
	cw_count_t			track);

extern cw_count_t
options_get_output_track_end(
	cw_void_t);

extern cw_bool_t
options_set_track_size_limit(
	cw_count_t			limit);

extern cw_count_t
options_get_track_size_limit(
	cw_void_t);



#endif /* !CWTOOL_OPTIONS_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * format/histogram.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_HISTOGRAM_H
#define CWTOOL_FORMAT_HISTOGRAM_H

#include "types.h"
#include "../global.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




typedef cw_count_t			cw_hist_t[GLOBAL_NR_PULSE_LENGTHS];
typedef cw_hist_t			cw_hist2_t[GLOBAL_NR_PULSE_LENGTHS];

struct fifo;
struct bounds;




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_void_t
histogram_calculate(
	cw_raw8_t			*data,
	cw_size_t			size,
	cw_hist_t			histogram,
	cw_hist2_t			histogram2);

extern cw_void_t
histogram_blur(
	cw_hist_t			src,
	cw_hist_t			dst,
	cw_count_t			range);

extern cw_void_t
histogram_normal(
	struct fifo			*ffo,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side);

extern cw_void_t
histogram_postcomp_simple(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side);

extern cw_void_t
histogram_analyze_gcr(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side);

extern cw_void_t
histogram_analyze_mfm(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side);



#endif /* !CWTOOL_FORMAT_HISTOGRAM_H */
/******************************************************** Karsten Scheibler */

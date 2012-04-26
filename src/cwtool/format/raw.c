/****************************************************************************
 ****************************************************************************
 *
 * format/raw.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "raw.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "container.h"
#include "histogram.h"
#include "bounds.h"




/****************************************************************************
 *
 * functions for track handling
 *
 ****************************************************************************/




/****************************************************************************
 * raw_read_write_track
 ****************************************************************************/
static int
raw_read_write_track(
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst)

	{
	int				size = fifo_get_wr_ofs(ffo_src);

	debug_error_condition(fifo_get_limit(ffo_dst) < size);
	if (fifo_copy_block(ffo_src, ffo_dst, size) == -1) return (0);
	fifo_set_flags(ffo_dst, fifo_get_flags(ffo_src));
	return (1);
	}



/****************************************************************************
 * raw_statistics
 ****************************************************************************/
static int
raw_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(ffo_l0, cwtool_track, -1, -1);
	return (1);
	}



/****************************************************************************
 * raw_read_track
 ****************************************************************************/
static int
raw_read_track(
	union format			*fmt,
	struct container		*con,
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	return (raw_read_write_track(ffo_src, ffo_dst));
	}



/****************************************************************************
 * raw_write_track
 ****************************************************************************/
static int
raw_write_track(
	union format			*fmt,
	struct fifo			*ffo_src,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_dst,
	unsigned char			*data,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	return (raw_read_write_track(ffo_src, ffo_dst));
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




/****************************************************************************
 * raw_set_defaults
 ****************************************************************************/
static void
raw_set_defaults(
	union format			*fmt)

	{
	debug_message(GENERIC, 2, "setting defaults");
	}



/****************************************************************************
 * raw_set_dummy_option
 ****************************************************************************/
static int
raw_set_dummy_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * raw_get_sector_size
 ****************************************************************************/
static int
raw_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= 0);
	return (GLOBAL_MAX_TRACK_SIZE);
	}



/****************************************************************************
 * raw_get_sectors
 ****************************************************************************/
static int
raw_get_sectors(
	union format			*fmt)

	{
	return (0);
	}



/****************************************************************************
 * raw_get_flags
 ****************************************************************************/
static int
raw_get_flags(
	union format			*fmt)

	{
	return (FORMAT_FLAG_GREEDY);
	}



/****************************************************************************
 * raw_get_data_offset
 ****************************************************************************/
static int
raw_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * raw_get_data_size
 ****************************************************************************/
static int
raw_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * raw_dummy_options
 ****************************************************************************/
static struct format_option		raw_dummy_options[] =
	{
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * raw_format_desc
 ****************************************************************************/
struct format_desc			raw_format_desc =
	{
	.name             = "raw",
	.level            = 0,
	.set_defaults     = raw_set_defaults,
	.set_read_option  = raw_set_dummy_option,
	.set_write_option = raw_set_dummy_option,
	.set_rw_option    = raw_set_dummy_option,
	.get_sectors      = raw_get_sectors,
	.get_sector_size  = raw_get_sector_size,
	.get_flags        = raw_get_flags,
	.get_data_offset  = raw_get_data_offset,
	.get_data_size    = raw_get_data_size,
	.track_statistics = raw_statistics,
	.track_read       = raw_read_track,
	.track_write      = raw_write_track,
	.fmt_opt_rd       = raw_dummy_options,
	.fmt_opt_wr       = raw_dummy_options,
	.fmt_opt_rw       = raw_dummy_options
	};
/******************************************************** Karsten Scheibler */

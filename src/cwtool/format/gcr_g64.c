/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_g64.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "gcr_g64.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "container.h"
#include "bitstream.h"
#include "postcomp_simple.h"
#include "histogram.h"
#include "setvalue.h"




/****************************************************************************
 *
 * low level gcr functions
 *
 ****************************************************************************/




/****************************************************************************
 * gcr_read_sync
 ****************************************************************************/
static int
gcr_read_sync(
	struct fifo			*ffo_l1,
	int				size)

	{
	int				i, j, reg;

	for (i = 0; ; )
		{
		reg = fifo_read_bits(ffo_l1, 16);
		if (reg == -1) return (-1);
		i += 16;
		if (reg == 0xffff) continue;
		for (i -= 16, j = 16; j > 0; i++, j--, reg <<= 1)
			{
			if (reg & 0x8000) continue;

			/*
			 * the check above with 0xffff guarants that, this
			 * check is executed at least once
			 */

			if (i >= size) goto found;
			i = -1;
			}
		}
found:
	fifo_set_rd_bitofs(ffo_l1, fifo_get_rd_bitofs(ffo_l1) - j);
	verbose_message(GENERIC, 2, "got sync at bit offset %d with %d bits", fifo_get_rd_bitofs(ffo_l1) - i, i);
	return (i);
	}



/****************************************************************************
 * gcr_write_fill
 ****************************************************************************/
static int
gcr_write_fill(
	struct fifo			*ffo_l1,
	int				val,
	int				size)

	{
	verbose_message(GENERIC, 2, "writing fill at offset %d with value 0x%02x", fifo_get_wr_ofs(ffo_l1), val);
	while (size-- > 0) if (fifo_write_byte(ffo_l1, val) == -1) return (-1);
	return (0);
	}




/****************************************************************************
 *
 * functions for track handling
 *
 ****************************************************************************/




#define MAX_SYNCS			(4 * GLOBAL_NR_SECTORS)
#define FLAG_STRIP_TRACK		(1 << 0)
#define FLAG_ALIGN_TRACK		(1 << 1)
#define FLAG_POSTCOMP_SIMPLE		(1 << 2)



/****************************************************************************
 * gcr_g64_eliminate_gap
 ****************************************************************************/
static int
gcr_g64_eliminate_gap(
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	int				*start,
	int				*size,
	int				sectors)

	{
	int				i, j, s, max1, max2, max3;

	/*
	 * UGLY: improve current stripping, do more intelligent stripping.
	 *       also check if sectors have the same distance, strip only
	 *       bytes to guarantee that the track is not too long, this
	 *       does not mean to shorten sector distances to the average
	 *       size. also examine if there might be useful data in the
	 *       discarded bytes (but there is no clear criterion how to do
	 *       this)
	 */

	/*
	 * replace the two longest distances with the third longest to
	 * eliminate the track gap
	 */

	verbose_message(GENERIC, 3, "starting track gap elimination, found %d sectors", sectors);
	for (i = s = max1 = 0; s < sectors; s++) if (size[s] > max1) max1 = size[s], i = s;
	for (j = s = max2 = 0; s < sectors; s++) if ((s != i) && (size[s] > max2)) max2 = size[s], j = s;
	for (s = max3 = 0; s < sectors; s++) if ((s != i) && (s != j) && (size[s] > max3)) max3 = size[s];
	verbose_message(GENERIC, 3, "replacing size %d with %d at %d. sector", max1, max3, i + 1);
	size[i] = max3;
	verbose_message(GENERIC, 3, "replacing size %d with %d at %d. sector", max2, max3, j + 1);
	size[j] = max3;

	/* now copy each sector */

	for (s = 0; s < sectors; s++)
		{
		verbose_message(GENERIC, 3, "copying %d. sector at offset %d with size %d", s + 1, start[s], size[s]);
		if (fifo_write_block(ffo_dst, &fifo_get_data(ffo_src)[start[s]], size[s]) == -1) return (-1);
		}

	return (0);
	}



/****************************************************************************
 * gcr_g64_get_pattern
 ****************************************************************************/
static unsigned int
gcr_g64_get_pattern(
	struct fifo			*ffo)

	{
	unsigned int			pattern;
	int				val;

	val = fifo_read_bits(ffo, 16);
	if (val == -1) return (0);
	pattern = val << 16;
	val = fifo_read_bits(ffo, 16);
	if (val == -1) return (0);
	return (pattern | val);
	}



/****************************************************************************
 * gcr_g64_strip_track
 ****************************************************************************/
static int
gcr_g64_strip_track(
	struct fifo			*ffo_src,
	struct gcr_g64			*gcr_g64,
	struct fifo			*ffo_dst,
	int				limit)

	{
	unsigned int			pattern, p;
	int				start[GLOBAL_NR_SECTORS];
	int				size[GLOBAL_NR_SECTORS];
	int				l, s;

	/*
	 * strip track by eliminating track gap:
	 * - search for syncs and GCR encoded sector header ids
	 * - if the first found sector header is found again assume
	 *   one disk rotation is complete
	 * - calculate distance between sector headers, throw away the
	 *   two longest distances and replace it with the third longest,
	 *   assuming the two longest distances contain the track gap and
	 *   if first sector header was not found again also some epilog
	 *   bytes
	 */

	for (s = 0; s < GLOBAL_NR_SECTORS; s++) size[s] = limit;
	for (s = pattern = 0; ; )
		{
		l = gcr_read_sync(ffo_src, gcr_g64->rd.sync_length);
		if (l == -1) goto eliminate_gap;
		if (fifo_read_bits(ffo_src, 8) != gcr_g64->rd.header_raw_id) continue;
		if (s >= GLOBAL_NR_SECTORS) break;
		start[s] = fifo_get_rd_ofs(ffo_src) - ((l + 7) / 8) - 2;
		if (start[s] < 0) start[s] = 0;
		if (start[s] > limit) break;
		if (s > 0) size[s - 1] = start[s] - start[s - 1];
		p = gcr_g64_get_pattern(ffo_src);
		if (p == 0) goto eliminate_gap;
		verbose_message(GENERIC, 3, "found pattern 0x%08x around offset %d", p, start[s]);
		if (++s == 1) pattern = p;
		if ((s == 1) || (pattern != p)) continue;
		s--;
		verbose_message(GENERIC, 3, "disk rotation complete around offset %d", start[s]);

	eliminate_gap:
		if (s < 3) break;
		if (gcr_g64_eliminate_gap(ffo_src, ffo_dst, start, size, s) == -1) return (-1);
		goto track_done;
		}

	/* track gap postprocessing failed, we have to take track as it is */

	verbose_message(GENERIC, 1, "taking track as is, without any postprocessing");
	s = fifo_get_wr_ofs(ffo_src) - 1;
	if (s > limit)
		{
		verbose_message(GENERIC, 1, "track too large, needed to truncate it");
		s = limit;
		}
	if (fifo_write_block(ffo_dst, fifo_get_data(ffo_src), s) == -1) return (-1);

track_done:
	return (0);
	}



/****************************************************************************
 * gcr_g64_align_bits
 ****************************************************************************/
static int
gcr_g64_align_bits(
	struct fifo			*ffo,
	int				bits)

	{
	int				val = 0xaa;

	bits = -bits & 7;
	if (bits != 0)
		{
		val |= 1 << (bits - 1);
		val &= (1 << bits) - 1;
		verbose_message(GENERIC, 3, "inserting 0x%02x with %d bits", val, bits);
		if (fifo_write_bits(ffo, val, bits) == -1) return (-1);
		}
	return (bits);
	}



/****************************************************************************
 * gcr_g64_align_track
 ****************************************************************************/
static int
gcr_g64_align_track(
	struct fifo			*ffo,
	struct gcr_g64			*gcr_g64)

	{
	unsigned char			data[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_tmp = FIFO_INIT(data, sizeof (data));
	int				size[MAX_SYNCS];
	int				end[MAX_SYNCS];
	int				i, j, l, s;

	/* check if track is already aligned */

	verbose_message(GENERIC, 3, "checking track alignment");
	size[0] = 0;
	end[0]  = 0;
	for (i = l = 0, s = 1; (l != -1) && (s < MAX_SYNCS); i += j & 7, s++)
		{
		l = size[s] = gcr_read_sync(ffo, gcr_g64->rd.sync_length);
		j = end[s]  = fifo_get_rd_bitofs(ffo);
		if (l == -1) size[s] = 0;
		}
	fifo_set_rd_bitofs(ffo, 0);
	if (i == 0) return (0);

	/* align syncs so that bytes after it start on byte boundaries */

	verbose_message(GENERIC, 3, "starting track alignment");
	fifo_copy_block(ffo, &ffo_tmp, fifo_get_wr_ofs(ffo));
	fifo_reset(ffo);
	for (i = 1, j = 0; i < s; i++)
		{
		l = gcr_g64_align_bits(ffo, j + end[i - 1]);
		if (l == -1) return (-1);
		j += l;
		l = (end[i] - size[i]) - (end[i - 1] - size[i - 1]);
		verbose_message(GENERIC, 3, "copying %d bits", l);
		fifo_copy_bitblock(&ffo_tmp, ffo, l);
		}
	if (gcr_g64_align_bits(ffo, fifo_get_wr_bitofs(ffo)) == -1) return (-1);

	return (0);
	}



/****************************************************************************
 * gcr_g64_statistics
 ****************************************************************************/
static int
gcr_g64_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	int				speed = fmt->gcr_g64.rd.speed;

	histogram_normal(ffo_l0, cwtool_track, -1, -1);
	if (fmt->gcr_g64.rd.flags & FLAG_POSTCOMP_SIMPLE) histogram_postcomp_simple(ffo_l0, fmt->gcr_g64.rw.bnd[speed], 3, cwtool_track, -1, -1);
	return (1);
	}



/****************************************************************************
 * gcr_g64_read_track
 ****************************************************************************/
static int
gcr_g64_read_track(
	union format			*fmt,
	struct container		*con,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l1,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	int				length[4]   = { 6250, 6667, 7143, 7692 };
	unsigned char			data[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1_tmp  = FIFO_INIT(data, sizeof (data));
	int				pad_length2 = fmt->gcr_g64.rd.pad_length2;
	int				limit       = fifo_get_limit(ffo_l1) - fmt->gcr_g64.rd.pad_length1 - pad_length2;
	int				speed       = fmt->gcr_g64.rd.speed;

	/*
	 * to ensure that we have no more than two consecutive zeros,
	 * pad_value2 should be 0xaa instead of 0x55
	 */

	debug_error_condition((speed < 0) || (speed > 3));
	if (fmt->gcr_g64.rd.flags & FLAG_POSTCOMP_SIMPLE) postcomp_simple(ffo_l0, fmt->gcr_g64.rw.bnd[speed], 3);
	bitstream_read(ffo_l0, &ffo_l1_tmp, fmt->gcr_g64.rw.bnd[speed], 3);
	if (gcr_write_fill(ffo_l1, fmt->gcr_g64.rd.pad_value1, fmt->gcr_g64.rd.pad_length1) == -1) return (0);
	if (fmt->gcr_g64.rd.flags & FLAG_STRIP_TRACK) if (gcr_g64_strip_track(&ffo_l1_tmp, &fmt->gcr_g64, ffo_l1, limit) == -1) return (0);
	if (fmt->gcr_g64.rd.flags & FLAG_ALIGN_TRACK) if (gcr_g64_align_track(ffo_l1, &fmt->gcr_g64) == -1) return (0);

	/*
	 * if pad_length2 is zero, we try to fill up the track to a standard
	 * length for the selected speed
	 */

	if (pad_length2 == 0) pad_length2 = length[speed] - fifo_get_wr_ofs(ffo_l1);
	if (pad_length2 > 0) if (gcr_write_fill(ffo_l1, fmt->gcr_g64.rd.pad_value2, pad_length2) == -1) return (0);
	fifo_write_flush(ffo_l1);
	fifo_set_speed(ffo_l1, speed);
	return (1);
	}



/****************************************************************************
 * gcr_g64_write_track
 ****************************************************************************/
static int
gcr_g64_write_track(
	union format			*fmt,
	struct fifo			*ffo_l1,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	unsigned char			*data,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			data_l1_tmp[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1_tmp = FIFO_INIT(data_l1_tmp, sizeof (data_l1_tmp));
	int				limit      = fifo_get_limit(&ffo_l1_tmp) - fmt->gcr_g64.wr.prolog_length - fmt->gcr_g64.wr.epilog_length;
	int				speed      = fifo_get_speed(ffo_l1);

	/*
	 * to ensure that we have no more than two consecutive zeros,
	 * epilog_value should be 0xaa instead of 0x55
	 */

	debug_error_condition((speed < 0) || (speed > 3));
	if (gcr_write_fill(&ffo_l1_tmp, fmt->gcr_g64.wr.prolog_value, fmt->gcr_g64.wr.prolog_length) == -1) return (0);
	if (fmt->gcr_g64.wr.flags & FLAG_STRIP_TRACK) if (gcr_g64_strip_track(ffo_l1, &fmt->gcr_g64, &ffo_l1_tmp, limit) == -1) return (0);
	if (gcr_write_fill(&ffo_l1_tmp, fmt->gcr_g64.wr.epilog_value, fmt->gcr_g64.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1_tmp);
	if (bitstream_write(&ffo_l1_tmp, ffo_l0, fmt->gcr_g64.rw.bnd[speed], fmt->gcr_g64.wr.precomp[speed], 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_STRIP_TRACK		1
#define MAGIC_ALIGN_TRACK		2
#define MAGIC_POSTCOMP_SIMPLE		3
#define MAGIC_SYNC_LENGTH		4
#define MAGIC_PAD_LENGTH1		5
#define MAGIC_PAD_LENGTH2		6
#define MAGIC_HEADER_RAW_ID		7
#define MAGIC_SPEED			8
#define MAGIC_PROLOG_LENGTH		9
#define MAGIC_EPILOG_LENGTH		10
#define MAGIC_PRECOMP0			11
#define MAGIC_PRECOMP1			12
#define MAGIC_PRECOMP2			13
#define MAGIC_PRECOMP3			14
#define MAGIC_BOUNDS0_OLD		15
#define MAGIC_BOUNDS1_OLD		16
#define MAGIC_BOUNDS2_OLD		17
#define MAGIC_BOUNDS3_OLD		18
#define MAGIC_BOUNDS0_NEW		19
#define MAGIC_BOUNDS1_NEW		20
#define MAGIC_BOUNDS2_NEW		21
#define MAGIC_BOUNDS3_NEW		22



/****************************************************************************
 * gcr_g64_set_defaults
 ****************************************************************************/
static void
gcr_g64_set_defaults(
	union format			*fmt)

	{
	const static struct gcr_g64	gcr_g64 =
		{
		.rd =
			{
			.flags         = 0,
			.sync_length   = 40,
			.pad_length1   = 8,
			.pad_value1    = 0x55,
			.pad_length2   = 0,
			.pad_value2    = 0xaa,
			.header_raw_id = 0x52,
			.speed         = 3
			},
		.wr =
			{
			.prolog_length = 0,
			.epilog_length = 64,
			.prolog_value  = 0x55,
			.epilog_value  = 0xaa,
			.precomp       = { }
			},
		.rw =
			{
			.bnd =
				{
					{
					BOUNDS_NEW(0x1300, 0x1600, 0x2200, 0),
					BOUNDS_NEW(0x2300, 0x2d00, 0x3900, 1),
					BOUNDS_NEW(0x3a00, 0x4400, 0x5000, 2)
					},
					{
					BOUNDS_NEW(0x1200, 0x1500, 0x2000, 0),
					BOUNDS_NEW(0x2100, 0x2b00, 0x3700, 1),
					BOUNDS_NEW(0x3800, 0x4100, 0x5000, 2)
					},
					{
					BOUNDS_NEW(0x1100, 0x1400, 0x1f00, 0),
					BOUNDS_NEW(0x2000, 0x2900, 0x3400, 1),
					BOUNDS_NEW(0x3500, 0x3e00, 0x5000, 2)
					},
					{
					BOUNDS_NEW(0x0f00, 0x1200, 0x1c00, 0),
					BOUNDS_NEW(0x1d00, 0x2500, 0x2f00, 1),
					BOUNDS_NEW(0x3000, 0x3800, 0x4800, 2)
					}
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->gcr_g64 = gcr_g64;
	}



/****************************************************************************
 * gcr_g64_set_read_option
 ****************************************************************************/
static int
gcr_g64_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_STRIP_TRACK)     return (setvalue_uchar_bit(&fmt->gcr_g64.rd.flags, val, FLAG_STRIP_TRACK));
	if (magic == MAGIC_ALIGN_TRACK)     return (setvalue_uchar_bit(&fmt->gcr_g64.rd.flags, val, FLAG_ALIGN_TRACK));
	if (magic == MAGIC_POSTCOMP_SIMPLE) return (setvalue_uchar_bit(&fmt->gcr_g64.rd.flags, val, FLAG_POSTCOMP_SIMPLE));
	if (magic == MAGIC_SYNC_LENGTH)     return (setvalue_uchar(&fmt->gcr_g64.rd.sync_length, val, 9, 0xff));
	if (magic == MAGIC_PAD_LENGTH1)     return (setvalue_uchar(&fmt->gcr_g64.rd.pad_length1, val, 0, 0xff));
	if (magic == MAGIC_PAD_LENGTH2)     return (setvalue_uchar(&fmt->gcr_g64.rd.pad_length2, val, 0, 0xff));
	if (magic == MAGIC_HEADER_RAW_ID)   return (setvalue_uchar(&fmt->gcr_g64.rd.header_raw_id, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_SPEED);
	return (setvalue_uchar(&fmt->gcr_g64.rd.speed, val, 0, 3));
	}



/****************************************************************************
 * gcr_g64_set_write_option
 ****************************************************************************/
static int
gcr_g64_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->gcr_g64.wr.prolog_length, val, 0, 0xffff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->gcr_g64.wr.epilog_length, val, 8, 0xffff));
	if (magic == MAGIC_STRIP_TRACK)   return (setvalue_uchar_bit(&fmt->gcr_g64.wr.flags, val, FLAG_STRIP_TRACK));
	if (magic == MAGIC_PRECOMP0)      return (setvalue_short(&fmt->gcr_g64.wr.precomp[0][ofs], val, -0x4000, 0x4000));
	if (magic == MAGIC_PRECOMP1)      return (setvalue_short(&fmt->gcr_g64.wr.precomp[1][ofs], val, -0x4000, 0x4000));
	if (magic == MAGIC_PRECOMP2)      return (setvalue_short(&fmt->gcr_g64.wr.precomp[2][ofs], val, -0x4000, 0x4000));
	debug_error_condition(magic != MAGIC_PRECOMP3);
	return (setvalue_short(&fmt->gcr_g64.wr.precomp[3][ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * gcr_g64_set_rw_option
 ****************************************************************************/
static int
gcr_g64_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_BOUNDS0_OLD) return (setvalue_bounds_old(fmt->gcr_g64.rw.bnd[0], val, ofs));
	if (magic == MAGIC_BOUNDS1_OLD) return (setvalue_bounds_old(fmt->gcr_g64.rw.bnd[1], val, ofs));
	if (magic == MAGIC_BOUNDS2_OLD) return (setvalue_bounds_old(fmt->gcr_g64.rw.bnd[2], val, ofs));
	if (magic == MAGIC_BOUNDS3_OLD) return (setvalue_bounds_old(fmt->gcr_g64.rw.bnd[3], val, ofs));
	if (magic == MAGIC_BOUNDS0_NEW) return (setvalue_bounds_new(fmt->gcr_g64.rw.bnd[0], val, ofs));
	if (magic == MAGIC_BOUNDS1_NEW) return (setvalue_bounds_new(fmt->gcr_g64.rw.bnd[1], val, ofs));
	if (magic == MAGIC_BOUNDS2_NEW) return (setvalue_bounds_new(fmt->gcr_g64.rw.bnd[2], val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS3_NEW);
	return (setvalue_bounds_new(fmt->gcr_g64.rw.bnd[3], val, ofs));
	}



/****************************************************************************
 * gcr_g64_get_sector_size
 ****************************************************************************/
static int
gcr_g64_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= 0);
	return (0xffff);
	}



/****************************************************************************
 * gcr_g64_get_sectors
 ****************************************************************************/
static int
gcr_g64_get_sectors(
	union format			*fmt)

	{
	return (0);
	}



/****************************************************************************
 * gcr_g64_get_flags
 ****************************************************************************/
static int
gcr_g64_get_flags(
	union format			*fmt)

	{
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * gcr_g64_get_data_offset
 ****************************************************************************/
static int
gcr_g64_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * gcr_g64_get_data_size
 ****************************************************************************/
static int
gcr_g64_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * gcr_g64_read_options
 ****************************************************************************/
static struct format_option		gcr_g64_read_options[] =
	{
	FORMAT_OPTION_BOOLEAN("strip_track",     MAGIC_STRIP_TRACK,     1),
	FORMAT_OPTION_BOOLEAN("align_track",     MAGIC_ALIGN_TRACK,     1),
	FORMAT_OPTION_BOOLEAN_COMPAT("postcomp",        MAGIC_POSTCOMP_SIMPLE, 1),
	FORMAT_OPTION_BOOLEAN("postcomp_simple", MAGIC_POSTCOMP_SIMPLE, 1),
	FORMAT_OPTION_INTEGER("sync_length",     MAGIC_SYNC_LENGTH,     1),
	FORMAT_OPTION_INTEGER("pad_length1",     MAGIC_PAD_LENGTH1,     1),
	FORMAT_OPTION_INTEGER("pad_length2",     MAGIC_PAD_LENGTH2,     1),
	FORMAT_OPTION_INTEGER("header_raw_id",   MAGIC_HEADER_RAW_ID,   1),
	FORMAT_OPTION_INTEGER("speed",           MAGIC_SPEED,           1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_g64_write_options
 ****************************************************************************/
static struct format_option		gcr_g64_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION_BOOLEAN("strip_track",   MAGIC_STRIP_TRACK,   1),
	FORMAT_OPTION_INTEGER("precomp0",      MAGIC_PRECOMP0,      9),
	FORMAT_OPTION_INTEGER("precomp1",      MAGIC_PRECOMP1,      9),
	FORMAT_OPTION_INTEGER("precomp2",      MAGIC_PRECOMP2,      9),
	FORMAT_OPTION_INTEGER("precomp3",      MAGIC_PRECOMP3,      9),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_g64_rw_options
 ****************************************************************************/
static struct format_option		gcr_g64_rw_options[] =
	{
	FORMAT_OPTION_INTEGER_COMPAT("bounds0",     MAGIC_BOUNDS0_OLD, 9),
	FORMAT_OPTION_INTEGER_COMPAT("bounds1",     MAGIC_BOUNDS1_OLD, 9),
	FORMAT_OPTION_INTEGER_COMPAT("bounds2",     MAGIC_BOUNDS2_OLD, 9),
	FORMAT_OPTION_INTEGER_COMPAT("bounds3",     MAGIC_BOUNDS3_OLD, 9),
	FORMAT_OPTION_INTEGER("bounds0_old", MAGIC_BOUNDS0_OLD, 9),
	FORMAT_OPTION_INTEGER("bounds1_old", MAGIC_BOUNDS1_OLD, 9),
	FORMAT_OPTION_INTEGER("bounds2_old", MAGIC_BOUNDS2_OLD, 9),
	FORMAT_OPTION_INTEGER("bounds3_old", MAGIC_BOUNDS3_OLD, 9),
	FORMAT_OPTION_INTEGER("bounds0_new", MAGIC_BOUNDS0_NEW, 9),
	FORMAT_OPTION_INTEGER("bounds1_new", MAGIC_BOUNDS1_NEW, 9),
	FORMAT_OPTION_INTEGER("bounds2_new", MAGIC_BOUNDS2_NEW, 9),
	FORMAT_OPTION_INTEGER("bounds3_new", MAGIC_BOUNDS3_NEW, 9),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * gcr_g64_format_desc
 ****************************************************************************/
struct format_desc			gcr_g64_format_desc =
	{
	.name             = "gcr_g64",
	.level            = 1,
	.set_defaults     = gcr_g64_set_defaults,
	.set_read_option  = gcr_g64_set_read_option,
	.set_write_option = gcr_g64_set_write_option,
	.set_rw_option    = gcr_g64_set_rw_option,
	.get_sectors      = gcr_g64_get_sectors,
	.get_sector_size  = gcr_g64_get_sector_size,
	.get_flags        = gcr_g64_get_flags,
	.get_data_offset  = gcr_g64_get_data_offset,
	.get_data_size    = gcr_g64_get_data_size,
	.track_statistics = gcr_g64_statistics,
	.track_read       = gcr_g64_read_track,
	.track_write      = gcr_g64_write_track,
	.fmt_opt_rd       = gcr_g64_read_options,
	.fmt_opt_wr       = gcr_g64_write_options,
	.fmt_opt_rw       = gcr_g64_rw_options
	};
/******************************************************** Karsten Scheibler */

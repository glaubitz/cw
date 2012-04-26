/****************************************************************************
 ****************************************************************************
 *
 * format/mfm_amiga.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "mfm_amiga.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "mfm.h"
#include "range.h"
#include "bitstream.h"
#include "container.h"
#include "match_simple.h"
#include "postcomp_simple.h"
#include "histogram.h"
#include "setvalue.h"




/****************************************************************************
 *
 * functions for sector and track handling
 *
 ****************************************************************************/




#define DATA_SIZE			540
#define FLAG_IGNORE_CHECKSUMS		(1 << 0)
#define FLAG_IGNORE_TRACK_MISMATCH	(1 << 1)
#define FLAG_IGNORE_FORMAT_BYTE		(1 << 2)
#define FLAG_MATCH_SIMPLE		(1 << 3)
#define FLAG_MATCH_SIMPLE_FIXUP		(1 << 4)
#define FLAG_POSTCOMP_SIMPLE		(1 << 5)



/****************************************************************************
 * mfm_amiga_shuffle
 ****************************************************************************/
static void
mfm_amiga_shuffle(
	unsigned char			*data,
	int				len)

	{
	unsigned char			tmp[DATA_SIZE];
	int				i, j, k;

	debug_error_condition((len % 2) != 0);
	debug_error_condition(len > sizeof (tmp));
	for (i = j = 0, k = len / 2; i < len; i += 2, j++, k++)
		{
		tmp[j] = (mfm_decode_table[(data[i] >> 1) & 0x55] << 4) |
			mfm_decode_table[(data[i + 1] >> 1) & 0x55];
		tmp[k] = (mfm_decode_table[data[i]        & 0x55] << 4) |
			mfm_decode_table[data[i + 1]        & 0x55];
		}
	for (i = 0; i < len; i++) data[i] = tmp[i];
	}



/****************************************************************************
 * mfm_amiga_unshuffle
 ****************************************************************************/
static void
mfm_amiga_unshuffle(
	unsigned char			*data,
	int				len)

	{
	unsigned char			tmp[DATA_SIZE];
	int				i, j, k;

	debug_error_condition((len % 2) != 0);
	debug_error_condition(len > sizeof (tmp));
	for (i = k = 0, j = len / 2; k < len; i++, j++)
		{
		tmp[k++] = (mfm_encode_table[data[i] >>   4] << 1) | mfm_encode_table[data[j] >>   4];
		tmp[k++] = (mfm_encode_table[data[i] & 0x0f] << 1) | mfm_encode_table[data[j] & 0x0f];
		}
	for (i = 0; i < len; i++) data[i] = tmp[i];
	}



/****************************************************************************
 * mfm_amiga_checksum
 ****************************************************************************/
static unsigned long
mfm_amiga_checksum(
	unsigned char			*data,
	int				len)

	{
	int				i;
	unsigned long			val;

	debug_error_condition((len % 4) != 0);
	for (val = i = 0; i < len; i += 4) val ^= mfm_read_u32_le(&data[i]);
	val = ((val >> 1) & 0x55555555) ^ (val & 0x55555555);
	return (val);
	}



/****************************************************************************
 * mfm_amiga_track_number
 ****************************************************************************/
static cw_count_t
mfm_amiga_track_number(
	struct mfm_amiga		*mfm_amg,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if (format_track == -1) return (cwtool_track);
	return (format_track);
	}



/****************************************************************************
 * mfm_amiga_read_sector2
 ****************************************************************************/
static int
mfm_amiga_read_sector2(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct disk_error		*dsk_err,
	struct range_sector		*rng_sec,
	unsigned char			*data)

	{
	int				bitofs;

	*dsk_err = (struct disk_error) { };
	if (mfm_read_sync(ffo_l1, range_sector_data(rng_sec), mfm_amg->rw.sync_value, mfm_amg->rw.sync_length) == -1) return (-1);
	bitofs = fifo_get_rd_bitofs(ffo_l1);
	if (mfm_read_bytes(ffo_l1, dsk_err, data, DATA_SIZE) == -1) return (-1);
	range_set_end(range_sector_data(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	mfm_amiga_unshuffle(data, 4);
	mfm_amiga_unshuffle(&data[4], 16);
	mfm_amiga_unshuffle(&data[20], 4);
	mfm_amiga_unshuffle(&data[24], 4);
	mfm_amiga_unshuffle(&data[28], 512);
	verbose_message(GENERIC, 2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_sector2
 ****************************************************************************/
static int
mfm_amiga_write_sector2(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	unsigned char			*data)

	{
	if (mfm_write_fill(ffo_l1, mfm_amg->wr.fill_value, mfm_amg->wr.fill_length) == -1) return (-1);
	if (mfm_write_sync(ffo_l1, mfm_amg->rw.sync_value, mfm_amg->rw.sync_length) == -1) return (-1);
	mfm_amiga_shuffle(data, 4);
	mfm_amiga_shuffle(&data[4], 16);
	mfm_amiga_shuffle(&data[20], 4);
	mfm_amiga_shuffle(&data[24], 4);
	mfm_amiga_shuffle(&data[28], 512);
	if (mfm_write_bytes(ffo_l1, data, DATA_SIZE) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_read_sector
 ****************************************************************************/
static int
mfm_amiga_read_sector(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct container		*con,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	struct disk_error		dsk_err;
	struct range_sector		rng_sec = RANGE_SECTOR_INIT;
	unsigned char			data[DATA_SIZE];
	int				result, track, sector;

	if (mfm_amiga_read_sector2(ffo_l1, mfm_amg, &dsk_err, &rng_sec, data) == -1) return (-1);

	/* accept only valid sector numbers */

	sector = data[2];
	track  = mfm_amiga_track_number(mfm_amg, cwtool_track, format_track, format_side);
	if (sector >= mfm_amg->rw.sectors)
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	result = format_compare2("header xor checksum: got 0x%08x, expected: 0x%08x", mfm_read_u32_le(&data[20]), mfm_amiga_checksum(data, 20));
	result += format_compare2("data xor checksum: got 0x%08x, expected: 0x%08x", mfm_read_u32_le(&data[24]), mfm_amiga_checksum(&data[28], 512));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", data[1], track);
	if (result > 0) verbose_message(GENERIC, 2, "track mismatch on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare2("format_byte: got 0x%02x, expected 0x%02x", data[0], mfm_amg->rw.format_byte);
	if (result > 0) verbose_message(GENERIC, 2, "wrong format_byte on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_FORMAT_BYTE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_ID, result);

	/*
	 * take the data if the found sector is of better quality than the
	 * current one
	 */

	range_sector_set_number(&rng_sec, sector);
	if (con != NULL) container_append_range_sector(con, &rng_sec);
	disk_set_sector_number(&dsk_sct[sector], sector);
	disk_sector_read(&dsk_sct[sector], &dsk_err, &data[28]);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_sector
 ****************************************************************************/
static int
mfm_amiga_write_sector(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			data[DATA_SIZE];
	int				i, sector = disk_get_sector_number(dsk_sct);

	verbose_message(GENERIC, 1, "writing sector %d", sector);
	data[0] = mfm_amg->rw.format_byte;
	data[1] = mfm_amiga_track_number(mfm_amg, cwtool_track, format_track, format_side);
	data[2] = sector;
	data[3] = mfm_amg->rw.sectors - sector;
	for (i = 4; i < 20; i++) data[i] = 0;
	mfm_write_u32_le(&data[20], mfm_amiga_checksum(data, 20));
	disk_sector_write(&data[28], dsk_sct);
	mfm_write_u32_le(&data[24], mfm_amiga_checksum(&data[28], 512));
	return (mfm_amiga_write_sector2(ffo_l1, mfm_amg, data));
	}



/****************************************************************************
 * mfm_amiga_statistics
 ****************************************************************************/
static int
mfm_amiga_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(
		ffo_l0,
		cwtool_track,
		mfm_amiga_track_number(&fmt->mfm_amg, cwtool_track, format_track, format_side),
		-1);
	if (fmt->mfm_amg.rd.flags & FLAG_POSTCOMP_SIMPLE) histogram_postcomp_simple(
		ffo_l0,
		fmt->mfm_amg.rw.bnd,
		3,
		cwtool_track,
		mfm_amiga_track_number(&fmt->mfm_amg, cwtool_track, format_track, format_side),
		-1);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_read_track2
 ****************************************************************************/
static void
mfm_amiga_read_track2(
	union format			*fmt,
	struct container		*con,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			data[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1 = FIFO_INIT(data, sizeof (data));

	if (fmt->mfm_amg.rd.flags & FLAG_POSTCOMP_SIMPLE) postcomp_simple(ffo_l0, fmt->mfm_amg.rw.bnd, 3);
	bitstream_read(ffo_l0, &ffo_l1, fmt->mfm_amg.rw.bnd, 3);
	while (mfm_amiga_read_sector(&ffo_l1, &fmt->mfm_amg, con, dsk_sct, cwtool_track, format_track, format_side) != -1) ;
	}



/****************************************************************************
 * mfm_amiga_read_track
 ****************************************************************************/
static int
mfm_amiga_read_track(
	union format			*fmt,
	struct container		*con,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	struct match_simple_info	mat_sim_nfo =
		{
		.con          = con,
		.fmt          = fmt,
		.ffo_l0       = ffo_l0,
		.ffo_l3       = ffo_l3,
		.dsk_sct      = dsk_sct,
		.cwtool_track = cwtool_track,
		.format_track = format_track,
		.format_side  = format_side,
		.bnd          = fmt->mfm_amg.rw.bnd,
		.bnd_size     = 3,
		.callback     = mfm_amiga_read_track2,
		.merge_two    = fmt->mfm_amg.rd.flags & FLAG_MATCH_SIMPLE,
		.merge_all    = fmt->mfm_amg.rd.flags & FLAG_MATCH_SIMPLE,
		.fixup        = fmt->mfm_amg.rd.flags & FLAG_MATCH_SIMPLE_FIXUP
		};

	if ((fmt->mfm_amg.rd.flags & FLAG_MATCH_SIMPLE) || (options_get_output())) match_simple(&mat_sim_nfo);
	else mfm_amiga_read_track2(fmt, NULL, ffo_l0, ffo_l3, dsk_sct, cwtool_track, format_track, format_side);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_track
 ****************************************************************************/
static int
mfm_amiga_write_track(
	union format			*fmt,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	unsigned char			*data,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			data_l1[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1 = FIFO_INIT(data_l1, sizeof (data_l1));
	int				i;

	if (mfm_write_fill(&ffo_l1, fmt->mfm_amg.wr.prolog_value, fmt->mfm_amg.wr.prolog_length) == -1) return (0);
	for (i = 0; i < fmt->mfm_amg.rw.sectors; i++) if (mfm_amiga_write_sector(&ffo_l1, &fmt->mfm_amg, &dsk_sct[i], cwtool_track, format_track, format_side) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (mfm_write_fill(&ffo_l1, fmt->mfm_amg.wr.epilog_value, fmt->mfm_amg.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (bitstream_write(&ffo_l1, ffo_l0, fmt->mfm_amg.rw.bnd, fmt->mfm_amg.wr.precomp, 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_IGNORE_CHECKSUMS		1
#define MAGIC_IGNORE_TRACK_MISMATCH	2
#define MAGIC_IGNORE_FORMAT_BYTE	3
#define MAGIC_MATCH_SIMPLE		4
#define MAGIC_MATCH_SIMPLE_FIXUP	5
#define MAGIC_POSTCOMP_SIMPLE		6
#define MAGIC_PROLOG_LENGTH		7
#define MAGIC_PROLOG_VALUE		8
#define MAGIC_EPILOG_LENGTH		9
#define MAGIC_EPILOG_VALUE		10
#define MAGIC_FILL_LENGTH		11
#define MAGIC_FILL_VALUE		12
#define MAGIC_PRECOMP			13
#define MAGIC_SECTORS			14
#define MAGIC_SYNC_LENGTH		15
#define MAGIC_SYNC_VALUE		16
#define MAGIC_FORMAT_BYTE		17
#define MAGIC_BOUNDS_OLD		18
#define MAGIC_BOUNDS_NEW		19



/****************************************************************************
 * mfm_amiga_set_defaults
 ****************************************************************************/
static void
mfm_amiga_set_defaults(
	union format			*fmt)

	{
	const static struct mfm_amiga	mfm_amg =
		{
		.rd =
			{
			.flags = 0
			},
		.wr =
			{
			.prolog_length = 0,
			.prolog_value  = 0x00,
			.epilog_length = 1,
			.epilog_value  = 0x00,
			.fill_length   = 2,
			.fill_value    = 0x00,
			.precomp       = { }
			},
		.rw =
			{
			.sectors     = 11,
			.sync_length = 2,
			.sync_value  = 0x4489,
			.format_byte = 0xff,
			.bnd         =
				{
				BOUNDS_NEW(0x1600, 0x1a52, 0x2300, 1),
				BOUNDS_NEW(0x2400, 0x287c, 0x3000, 2),
				BOUNDS_NEW(0x3100, 0x36a5, 0x4000, 3)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->mfm_amg = mfm_amg;
	}



/****************************************************************************
 * mfm_amiga_set_read_option
 ****************************************************************************/
static int
mfm_amiga_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_TRACK_MISMATCH));
	if (magic == MAGIC_IGNORE_FORMAT_BYTE)    return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_FORMAT_BYTE));
	if (magic == MAGIC_MATCH_SIMPLE)          return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_MATCH_SIMPLE));
	if (magic == MAGIC_MATCH_SIMPLE_FIXUP)    return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_MATCH_SIMPLE_FIXUP));
	debug_error_condition(magic != MAGIC_POSTCOMP_SIMPLE);
	return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_POSTCOMP_SIMPLE));
	}



/****************************************************************************
 * mfm_amiga_set_write_option
 ****************************************************************************/
static int
mfm_amiga_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->mfm_amg.wr.prolog_length, val, 0, 0xffff));
	if (magic == MAGIC_PROLOG_VALUE)  return (setvalue_uchar(&fmt->mfm_amg.wr.prolog_value, val, 0, 0xff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->mfm_amg.wr.epilog_length, val, 1, 0xffff));
	if (magic == MAGIC_EPILOG_VALUE)  return (setvalue_uchar(&fmt->mfm_amg.wr.epilog_value, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH)   return (setvalue_uchar(&fmt->mfm_amg.wr.fill_length, val, 1, 0xff));
	if (magic == MAGIC_FILL_VALUE)    return (setvalue_uchar(&fmt->mfm_amg.wr.fill_value, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->mfm_amg.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * mfm_amiga_set_rw_option
 ****************************************************************************/
static int
mfm_amiga_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)     return (setvalue_uchar(&fmt->mfm_amg.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
	if (magic == MAGIC_SYNC_LENGTH) return (setvalue_uchar(&fmt->mfm_amg.rw.sync_length, val, 0, 0xff));
	if (magic == MAGIC_SYNC_VALUE)  return (setvalue_ushort(&fmt->mfm_amg.rw.sync_value, val, 0, 0xffff));
	if (magic == MAGIC_FORMAT_BYTE) return (setvalue_uchar(&fmt->mfm_amg.rw.format_byte, val, 0, 0xff));
	if (magic == MAGIC_BOUNDS_OLD)  return (setvalue_bounds_old(fmt->mfm_amg.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->mfm_amg.rw.bnd, val, ofs));
	}



/****************************************************************************
 * mfm_amiga_get_sector_size
 ****************************************************************************/
static int
mfm_amiga_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= fmt->mfm_amg.rw.sectors);
	if (sector < 0) return (512 * fmt->mfm_amg.rw.sectors);
	return (512);
	}



/****************************************************************************
 * mfm_amiga_get_sectors
 ****************************************************************************/
static int
mfm_amiga_get_sectors(
	union format			*fmt)

	{
	return (fmt->mfm_amg.rw.sectors);
	}



/****************************************************************************
 * mfm_amiga_get_flags
 ****************************************************************************/
static int
mfm_amiga_get_flags(
	union format			*fmt)

	{
	if (options_get_output()) return (FORMAT_FLAG_OUTPUT);
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * mfm_amiga_get_data_offset
 ****************************************************************************/
static int
mfm_amiga_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * mfm_amiga_get_data_size
 ****************************************************************************/
static int
mfm_amiga_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * mfm_amiga_read_options
 ****************************************************************************/
static struct format_option		mfm_amiga_read_options[] =
	{
	FORMAT_OPTION_BOOLEAN("ignore_checksums",      MAGIC_IGNORE_CHECKSUMS,      1),
	FORMAT_OPTION_BOOLEAN("ignore_track_mismatch", MAGIC_IGNORE_TRACK_MISMATCH, 1),
	FORMAT_OPTION_BOOLEAN("ignore_format_byte",    MAGIC_IGNORE_FORMAT_BYTE,    1),
	FORMAT_OPTION_BOOLEAN("match_simple",          MAGIC_MATCH_SIMPLE,          1),
	FORMAT_OPTION_BOOLEAN("match_simple_fixup",    MAGIC_MATCH_SIMPLE_FIXUP,    1),
	FORMAT_OPTION_BOOLEAN_COMPAT("postcomp",              MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_BOOLEAN("postcomp_simple",       MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * mfm_amiga_write_options
 ****************************************************************************/
static struct format_option		mfm_amiga_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("prolog_value",  MAGIC_PROLOG_VALUE,  1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("epilog_value",  MAGIC_EPILOG_VALUE,  1),
	FORMAT_OPTION_INTEGER("fill_length",   MAGIC_FILL_LENGTH,   1),
	FORMAT_OPTION_INTEGER("fill_value",    MAGIC_FILL_VALUE,    1),
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       9),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * mfm_amiga_rw_options
 ****************************************************************************/
static struct format_option		mfm_amiga_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",     MAGIC_SECTORS,     1),
	FORMAT_OPTION_INTEGER("sync_length", MAGIC_SYNC_LENGTH, 1),
	FORMAT_OPTION_INTEGER("sync_value",  MAGIC_SYNC_LENGTH, 1),
	FORMAT_OPTION_INTEGER("format_byte", MAGIC_FORMAT_BYTE, 1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",      MAGIC_BOUNDS_OLD,  9),
	FORMAT_OPTION_INTEGER("bounds_old",  MAGIC_BOUNDS_OLD,  9),
	FORMAT_OPTION_INTEGER("bounds_new",  MAGIC_BOUNDS_NEW,  9),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * mfm_amiga_format_desc
 ****************************************************************************/
struct format_desc			mfm_amiga_format_desc =
	{
	.name             = "mfm_amiga",
	.level            = 3,
	.set_defaults     = mfm_amiga_set_defaults,
	.set_read_option  = mfm_amiga_set_read_option,
	.set_write_option = mfm_amiga_set_write_option,
	.set_rw_option    = mfm_amiga_set_rw_option,
	.get_sectors      = mfm_amiga_get_sectors,
	.get_sector_size  = mfm_amiga_get_sector_size,
	.get_flags        = mfm_amiga_get_flags,
	.get_data_offset  = mfm_amiga_get_data_offset,
	.get_data_size    = mfm_amiga_get_data_size,
	.track_statistics = mfm_amiga_statistics,
	.track_read       = mfm_amiga_read_track,
	.track_write      = mfm_amiga_write_track,
	.fmt_opt_rd       = mfm_amiga_read_options,
	.fmt_opt_wr       = mfm_amiga_write_options,
	.fmt_opt_rw       = mfm_amiga_rw_options
	};
/******************************************************** Karsten Scheibler */

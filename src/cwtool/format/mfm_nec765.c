/****************************************************************************
 ****************************************************************************
 *
 * format/mfm_nec765.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "mfm_nec765.h"
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




#define HEADER_SIZE			7
#define DATA_SIZE			16387
#define FLAG_RD_IGNORE_SECTOR_SIZE	(1 << 0)
#define FLAG_RD_IGNORE_CHECKSUMS	(1 << 1)
#define FLAG_RD_IGNORE_TRACK_MISMATCH	(1 << 2)
#define FLAG_RD_IGNORE_FORMAT_BYTE	(1 << 3)
#define FLAG_RD_MATCH_SIMPLE		(1 << 4)
#define FLAG_RD_MATCH_SIMPLE_FIXUP	(1 << 5)
#define FLAG_RD_POSTCOMP_SIMPLE		(1 << 6)
#define FLAG_RW_CRC16_INIT_VALUE_SET	(1 << 0)



/****************************************************************************
 * mfm_nec765_track_number
 ****************************************************************************/
static cw_count_t
mfm_nec765_track_number(
	struct mfm_nec765		*mfm_nec,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if ((format_track == -1) && (format_side == -1)) return (cwtool_track / (2 * mfm_nec->rw.track_step));
	return (format_track);
	}



/****************************************************************************
 * mfm_nec765_side_number
 ****************************************************************************/
static cw_count_t
mfm_nec765_side_number(
	struct mfm_nec765		*mfm_nec,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if ((format_track == -1) && (format_side == -1)) return (cwtool_track % 2);
	return (format_side);
	}



/****************************************************************************
 * mfm_nec765_sector_shift
 ****************************************************************************/
static int
mfm_nec765_sector_shift(
	struct mfm_nec765		*mfm_nec,
	int				sector)

	{
	return (mfm_get_sector_shift(mfm_nec->rw.pshift, sector, GLOBAL_NR_SECTORS));
	}



/****************************************************************************
 * mfm_nec765_sector_size
 ****************************************************************************/
static int
mfm_nec765_sector_size(
	struct mfm_nec765		*mfm_nec,
	int				sector)

	{
	int				shift = mfm_nec765_sector_shift(mfm_nec, sector);

	if (shift == -1) shift = 2;
	return (0x80 << shift);
	}



/****************************************************************************
 * mfm_nec765_read_sector2
 ****************************************************************************/
static int
mfm_nec765_read_sector2(
	struct fifo			*ffo_l1,
	struct mfm_nec765		*mfm_nec,
	struct disk_error		*dsk_err,
	struct range_sector		*rng_sec,
	unsigned char			*header,
	unsigned char			*data)

	{
	int				bitofs, data_size;

	while (1)
		{
		*dsk_err = (struct disk_error) { };
		if (mfm_read_sync(ffo_l1, range_sector_header(rng_sec), mfm_nec->rw.sync_value, mfm_nec->rw.sync_length) == -1) return (-1);
		bitofs = fifo_get_rd_bitofs(ffo_l1);
		if (mfm_read_bytes(ffo_l1, dsk_err, header, HEADER_SIZE) == -1) return (-1);
		range_set_end(range_sector_header(rng_sec), fifo_get_rd_bitofs(ffo_l1));
		fifo_set_rd_bitofs(ffo_l1, bitofs);
		if (format_compare2("id_address_mark: got 0x%02x, expected 0x%02x", header[0], mfm_nec->rw.id_address_mark) == 0) break;
		}
	data_size = mfm_nec765_sector_size(mfm_nec, header[3] - 1);
	if (mfm_read_sync(ffo_l1, range_sector_data(rng_sec), mfm_nec->rw.sync_value, mfm_nec->rw.sync_length) == -1) return (-1);
	if (mfm_read_bytes(ffo_l1, dsk_err, data, data_size + 3) == -1) return (-1);
	range_set_end(range_sector_data(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	verbose_message(GENERIC, 2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (1);
	}



/****************************************************************************
 * mfm_nec765_write_sector2
 ****************************************************************************/
static int
mfm_nec765_write_sector2(
	struct fifo			*ffo_l1,
	struct mfm_nec765		*mfm_nec,
	unsigned char			*header,
	unsigned char			*data,
	int				data_size)

	{
	if (mfm_write_fill(ffo_l1, mfm_nec->wr.fill_value2, mfm_nec->wr.fill_length2) == -1) return (-1);
	if (mfm_write_fill(ffo_l1, mfm_nec->wr.fill_value3, mfm_nec->wr.fill_length3) == -1) return (-1);
	if (mfm_write_sync(ffo_l1, mfm_nec->rw.sync_value, mfm_nec->rw.sync_length) == -1) return (-1);
	if (mfm_write_bytes(ffo_l1, header, HEADER_SIZE) == -1) return (-1);
	if (mfm_write_fill(ffo_l1, mfm_nec->wr.fill_value4, mfm_nec->wr.fill_length4) == -1) return (-1);
	if (mfm_write_fill(ffo_l1, mfm_nec->wr.fill_value5, mfm_nec->wr.fill_length5) == -1) return (-1);
	if (mfm_write_sync(ffo_l1, mfm_nec->rw.sync_value, mfm_nec->rw.sync_length) == -1) return (-1);
	if (mfm_write_bytes(ffo_l1, data, data_size) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * mfm_nec765_read_sector
 ****************************************************************************/
static int
mfm_nec765_read_sector(
	struct fifo			*ffo_l1,
	struct mfm_nec765		*mfm_nec,
	struct container		*con,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	struct disk_error		dsk_err;
	struct range_sector		rng_sec = RANGE_SECTOR_INIT;
	unsigned char			header[HEADER_SIZE];
	unsigned char			data[DATA_SIZE];
	int				result, track, side, sector, data_size;

	if (mfm_nec765_read_sector2(ffo_l1, mfm_nec, &dsk_err, &rng_sec, header, data) == -1) return (-1);

	/* accept only valid sector numbers */

	track  = mfm_nec765_track_number(mfm_nec, cwtool_track, format_track, format_side);
	side   = mfm_nec765_side_number(mfm_nec, cwtool_track, format_track, format_side);
	sector = header[3] - 1;
	if ((sector < 0) || (sector >= mfm_nec->rw.sectors))
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	data_size = mfm_nec765_sector_size(mfm_nec, sector);
	result = format_compare2("sector size: got %d, expected %d", 1 << (header[4] + 7), data_size);
	if (result > 0) verbose_message(GENERIC, 2, "wrong sector size on sector %d", sector);
	if (mfm_nec->rd.flags & FLAG_RD_IGNORE_SECTOR_SIZE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_SIZE, result);

	result = format_compare2("header crc16 checksum: got 0x%04x, expected 0x%04x", mfm_read_u16_be(&header[5]), mfm_crc16(mfm_nec->rw.crc16_init_value, header, 5));
	result += format_compare2("data crc16 checksum: got 0x%04x, expected 0x%04x", mfm_read_u16_be(&data[data_size + 1]), mfm_crc16(mfm_nec->rw.crc16_init_value, data, data_size + 1));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (mfm_nec->rd.flags & FLAG_RD_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", header[1], track);
	result += format_compare2("side: got %d, expected %d", header[2], side);
	if (result > 0) verbose_message(GENERIC, 2, "track or side mismatch on sector %d", sector);
	if (mfm_nec->rd.flags & FLAG_RD_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare3("data_address_mark: got 0x%02x, expected 0x%02x or 0x%02x", data[0], mfm_nec->rw.data_address_mark1, mfm_nec->rw.data_address_mark2);
	if (result > 0) verbose_message(GENERIC, 2, "wrong data_address_mark on sector %d", sector);
	if (mfm_nec->rd.flags & FLAG_RD_IGNORE_FORMAT_BYTE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_ID, result);

	/*
	 * take the data if the found sector is of better quality than the
	 * current one
	 */

	range_sector_set_number(&rng_sec, sector);
	if (con != NULL) container_append_range_sector(con, &rng_sec);
	disk_set_sector_number(&dsk_sct[sector], sector);
	disk_sector_read(&dsk_sct[sector], &dsk_err, &data[1]);
	return (1);
	}



/****************************************************************************
 * mfm_nec765_write_sector
 ****************************************************************************/
static int
mfm_nec765_write_sector(
	struct fifo			*ffo_l1,
	struct mfm_nec765		*mfm_nec,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			header[HEADER_SIZE];
	unsigned char			data[DATA_SIZE];
	int				sector = disk_get_sector_number(dsk_sct);
	int				data_size;

	verbose_message(GENERIC, 1, "writing sector %d", sector);
	header[0] = mfm_nec->rw.id_address_mark;
	header[1] = mfm_nec765_track_number(mfm_nec, cwtool_track, format_track, format_side);
	header[2] = mfm_nec765_side_number(mfm_nec, cwtool_track, format_track, format_side);
	header[3] = sector + 1;
	header[4] = mfm_nec765_sector_shift(mfm_nec, sector);
	data[0]   = mfm_nec->rw.data_address_mark1;
	data_size = mfm_nec765_sector_size(mfm_nec, sector);
	mfm_write_u16_be(&header[5], mfm_crc16(mfm_nec->rw.crc16_init_value, header, 5));
	disk_sector_write(&data[1], dsk_sct);
	mfm_write_u16_be(&data[data_size + 1], mfm_crc16(mfm_nec->rw.crc16_init_value, data, data_size + 1));
	return (mfm_nec765_write_sector2(ffo_l1, mfm_nec, header, data, data_size + 3));
	}



/****************************************************************************
 * mfm_nec765_statistics
 ****************************************************************************/
static int
mfm_nec765_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(
		ffo_l0,
		cwtool_track,
		mfm_nec765_track_number(&fmt->mfm_nec, cwtool_track, format_track, format_side),
		mfm_nec765_side_number(&fmt->mfm_nec, cwtool_track, format_track, format_side));
	if (fmt->mfm_nec.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) histogram_postcomp_simple(
		ffo_l0,
		fmt->mfm_nec.rw.bnd,
		3,
		cwtool_track,
		mfm_nec765_track_number(&fmt->mfm_nec, cwtool_track, format_track, format_side),
		mfm_nec765_side_number(&fmt->mfm_nec, cwtool_track, format_track, format_side));
	return (1);
	}



/****************************************************************************
 * mfm_nec765_read_track2
 ****************************************************************************/
static void
mfm_nec765_read_track2(
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

	if (fmt->mfm_nec.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) postcomp_simple(ffo_l0, fmt->mfm_nec.rw.bnd, 3);
	bitstream_read(ffo_l0, &ffo_l1, fmt->mfm_nec.rw.bnd, 3);
	while (mfm_nec765_read_sector(&ffo_l1, &fmt->mfm_nec, con, dsk_sct, cwtool_track, format_track, format_side) != -1) ;
	}



/****************************************************************************
 * mfm_nec765_read_track
 ****************************************************************************/
static int
mfm_nec765_read_track(
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
		.bnd          = fmt->mfm_nec.rw.bnd,
		.bnd_size     = 3,
		.callback     = mfm_nec765_read_track2,
		.merge_two    = fmt->mfm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.merge_all    = fmt->mfm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.fixup        = fmt->mfm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE_FIXUP
		};

	if ((fmt->mfm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE) || (options_get_output())) match_simple(&mat_sim_nfo);
	else mfm_nec765_read_track2(fmt, NULL, ffo_l0, ffo_l3, dsk_sct, cwtool_track, format_track, format_side);
	return (1);
	}



/****************************************************************************
 * mfm_nec765_write_track
 ****************************************************************************/
static int
mfm_nec765_write_track(
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

	if (mfm_write_fill(&ffo_l1, fmt->mfm_nec.wr.prolog_value, fmt->mfm_nec.wr.prolog_length) == -1) return (0);
	if (mfm_write_fill(&ffo_l1, fmt->mfm_nec.wr.fill_value1, fmt->mfm_nec.wr.fill_length1)   == -1) return (0);
	if (mfm_write_sync(&ffo_l1, 0x5224, 3) == -1) return (0);
	if (mfm_write_fill(&ffo_l1, 0xfc, 1)   == -1) return (0);
	for (i = 0; i < fmt->mfm_nec.rw.sectors; i++) if (mfm_nec765_write_sector(&ffo_l1, &fmt->mfm_nec, &dsk_sct[i], cwtool_track, format_track, format_side) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (mfm_write_fill(&ffo_l1, fmt->mfm_nec.wr.fill_value6, fmt->mfm_nec.wr.fill_length6)   == -1) return (0);
	if (mfm_write_fill(&ffo_l1, fmt->mfm_nec.wr.fill_value7, fmt->mfm_nec.wr.fill_length7)   == -1) return (0);
	if (mfm_write_sync(&ffo_l1, 0x4489, 3) == -1) return (0);
	if (mfm_write_fill(&ffo_l1, fmt->mfm_nec.wr.epilog_value, fmt->mfm_nec.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (bitstream_write(&ffo_l1, ffo_l0, fmt->mfm_nec.rw.bnd, fmt->mfm_nec.wr.precomp, 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_IGNORE_SECTOR_SIZE	1
#define MAGIC_IGNORE_CHECKSUMS		2
#define MAGIC_IGNORE_TRACK_MISMATCH	3
#define MAGIC_IGNORE_FORMAT_BYTE	4
#define MAGIC_MATCH_SIMPLE		5
#define MAGIC_MATCH_SIMPLE_FIXUP	6
#define MAGIC_POSTCOMP_SIMPLE		7
#define MAGIC_PROLOG_LENGTH		8
#define MAGIC_PROLOG_VALUE		9
#define MAGIC_EPILOG_LENGTH		10
#define MAGIC_EPILOG_VALUE		11
#define MAGIC_FILL_LENGTH1		12
#define MAGIC_FILL_VALUE1		13
#define MAGIC_FILL_LENGTH2		14
#define MAGIC_FILL_VALUE2		15
#define MAGIC_FILL_LENGTH3		16
#define MAGIC_FILL_VALUE3		17
#define MAGIC_FILL_LENGTH4		18
#define MAGIC_FILL_VALUE4		19
#define MAGIC_FILL_LENGTH5		20
#define MAGIC_FILL_VALUE5		21
#define MAGIC_FILL_LENGTH6		22
#define MAGIC_FILL_VALUE6		23
#define MAGIC_FILL_LENGTH7		24
#define MAGIC_FILL_VALUE7		25
#define MAGIC_PRECOMP			26
#define MAGIC_SECTORS			27
#define MAGIC_SYNC_LENGTH		28
#define MAGIC_SYNC_VALUE		29
#define MAGIC_CRC16_INIT_VALUE		30
#define MAGIC_ID_ADDRESS_MARK		31
#define MAGIC_DATA_ADDRESS_MARK1	32
#define MAGIC_DATA_ADDRESS_MARK2	33
#define MAGIC_TRACK_STEP		34
#define MAGIC_SECTOR_SIZES		35
#define MAGIC_BOUNDS_OLD		36
#define MAGIC_BOUNDS_NEW		37



/****************************************************************************
 * mfm_nec765_set_crc16_init_value
 ****************************************************************************/
static int
mfm_nec765_set_crc16_init_value(
	struct mfm_nec765		*mfm_nec)

	{
	unsigned char			data[256];
	int				d, i;

	if (mfm_nec->rw.flags & FLAG_RW_CRC16_INIT_VALUE_SET) return (1);
	for (d = 0, i = 0x4000; i > 0; i >>= 2) d = (d << 1) | ((mfm_nec->rw.sync_value & i) ? 1 : 0);
	for (i = 0; i < mfm_nec->rw.sync_length; i++) data[i] = d;
	mfm_nec->rw.crc16_init_value = mfm_crc16(0xffff, data, i);
	debug_message(GENERIC, 2, "calculated crc16_init_value = 0x%04x", mfm_nec->rw.crc16_init_value);
	return (1);
	}



/****************************************************************************
 * mfm_nec765_set_defaults
 ****************************************************************************/
static void
mfm_nec765_set_defaults(
	union format			*fmt)

	{
	const static struct mfm_nec765	mfm_nec =
		{
		.rd =
			{
			.flags = 0
			},
		.wr =
			{
			.prolog_length = 80,
			.epilog_length = 652,
			.prolog_value  = 0x4e,
			.epilog_value  = 0x4e,
			.fill_length1  = 12,
			.fill_value1   = 0x00,
			.fill_length2  = 50,
			.fill_value2   = 0x4e,
			.fill_length3  = 12,
			.fill_value3   = 0x00,
			.fill_length4  = 22,
			.fill_value4   = 0x4e,
			.fill_length5  = 12,
			.fill_value5   = 0x00,
			.fill_length6  = 54,
			.fill_value6   = 0x4e,
			.fill_length7  = 12,
			.fill_value7   = 0x00,
			.precomp       = { }
			},
		.rw =
			{
			.sectors            = 9,
			.sync_length        = 3,
			.sync_value         = 0x4489,
			.crc16_init_value   = 0,
			.flags              = 0,
			.id_address_mark    = 0xfe,
			.data_address_mark1 = 0xfb,
			.data_address_mark2 = 0xf8,
			.track_step         = 1,
			.bnd                =
				{
				BOUNDS_NEW(0x1600, 0x1a52, 0x2300, 1),
				BOUNDS_NEW(0x2400, 0x287c, 0x3000, 2),
				BOUNDS_NEW(0x3100, 0x36a5, 0x4000, 3)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->mfm_nec = mfm_nec;
	mfm_nec765_set_crc16_init_value(&fmt->mfm_nec);
	mfm_fill_sector_shift(fmt->mfm_nec.rw.pshift, 0, GLOBAL_NR_SECTORS, 2);
	}



/****************************************************************************
 * mfm_nec765_set_read_option
 ****************************************************************************/
static int
mfm_nec765_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_IGNORE_SECTOR_SIZE)    return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_IGNORE_SECTOR_SIZE));
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_IGNORE_TRACK_MISMATCH));
	if (magic == MAGIC_IGNORE_FORMAT_BYTE)    return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_IGNORE_FORMAT_BYTE));
	if (magic == MAGIC_MATCH_SIMPLE)          return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_MATCH_SIMPLE));
	if (magic == MAGIC_MATCH_SIMPLE_FIXUP)    return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_MATCH_SIMPLE_FIXUP));
	debug_error_condition(magic != MAGIC_POSTCOMP_SIMPLE);
	return (setvalue_uchar_bit(&fmt->mfm_nec.rd.flags, val, FLAG_RD_POSTCOMP_SIMPLE));
	}



/****************************************************************************
 * mfm_nec765_set_write_option
 ****************************************************************************/
static int
mfm_nec765_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->mfm_nec.wr.prolog_length, val, 1, 0xffff));
	if (magic == MAGIC_PROLOG_VALUE)  return (setvalue_uchar(&fmt->mfm_nec.wr.prolog_value, val, 0, 0xff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->mfm_nec.wr.epilog_length, val, 1, 0xffff));
	if (magic == MAGIC_EPILOG_VALUE)  return (setvalue_uchar(&fmt->mfm_nec.wr.epilog_value, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH1)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length1, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE1)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value1, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH2)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length2, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE2)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value2, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH3)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length3, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE3)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value3, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH4)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length4, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE4)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value4, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH5)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length5, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE5)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value5, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH6)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length6, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE6)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value6, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH7)  return (setvalue_uchar(&fmt->mfm_nec.wr.fill_length7, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE7)   return (setvalue_uchar(&fmt->mfm_nec.wr.fill_value7, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->mfm_nec.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * mfm_nec765_set_rw_option
 ****************************************************************************/
static int
mfm_nec765_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)
		{
		mfm_fill_sector_shift(fmt->mfm_nec.rw.pshift, fmt->mfm_nec.rw.sectors, GLOBAL_NR_SECTORS, 2);
		return (setvalue_uchar(&fmt->mfm_nec.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
		}
	if (magic == MAGIC_SYNC_LENGTH)
		{
		if (! setvalue_uchar(&fmt->mfm_nec.rw.sync_length, val, 0, 0xff)) return (0);
		return (mfm_nec765_set_crc16_init_value(&fmt->mfm_nec));
		}
	if (magic == MAGIC_SYNC_VALUE)
		{
		if (! setvalue_ushort(&fmt->mfm_nec.rw.sync_value, val, 0, 0xffff)) return (0);
		return (mfm_nec765_set_crc16_init_value(&fmt->mfm_nec));
		}
	if (magic == MAGIC_CRC16_INIT_VALUE)
		{
		setvalue_uchar_bit(&fmt->mfm_nec.rw.flags, (val < 0) ? 0 : 1, FLAG_RW_CRC16_INIT_VALUE_SET);
		if (val < 0) return (mfm_nec765_set_crc16_init_value(&fmt->mfm_nec));
		return (setvalue_ushort(&fmt->mfm_nec.rw.crc16_init_value, val, 0, 0xffff));
		}
	if (magic == MAGIC_ID_ADDRESS_MARK)    return (setvalue_uchar(&fmt->mfm_nec.rw.id_address_mark, val, 0, 0xff));
	if (magic == MAGIC_DATA_ADDRESS_MARK1) return (setvalue_uchar(&fmt->mfm_nec.rw.data_address_mark1, val, 0, 0xff));
	if (magic == MAGIC_DATA_ADDRESS_MARK2) return (setvalue_uchar(&fmt->mfm_nec.rw.data_address_mark2, val, 0, 0xff));
	if (magic == MAGIC_TRACK_STEP)         return (setvalue_uchar(&fmt->mfm_nec.rw.track_step, val, 1, 2));
	if (magic == MAGIC_SECTOR_SIZES)       return (mfm_set_sector_size(fmt->mfm_nec.rw.pshift, ofs, GLOBAL_NR_SECTORS, val));
	if (magic == MAGIC_BOUNDS_OLD)         return (setvalue_bounds_old(fmt->mfm_nec.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->mfm_nec.rw.bnd, val, ofs));
	}



/****************************************************************************
 * mfm_nec765_get_sector_size
 ****************************************************************************/
static int
mfm_nec765_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	int				i, s;

	debug_error_condition(sector >= fmt->mfm_nec.rw.sectors);
	if (sector >= 0) return (mfm_nec765_sector_size(&fmt->mfm_nec, sector));
	for (i = s = 0; i < fmt->mfm_nec.rw.sectors; i++) s += mfm_nec765_sector_size(&fmt->mfm_nec, i);
	return (s);
	}



/****************************************************************************
 * mfm_nec765_get_sectors
 ****************************************************************************/
static int
mfm_nec765_get_sectors(
	union format			*fmt)

	{
	return (fmt->mfm_nec.rw.sectors);
	}



/****************************************************************************
 * mfm_nec765_get_flags
 ****************************************************************************/
static int
mfm_nec765_get_flags(
	union format			*fmt)

	{
	if (options_get_output()) return (FORMAT_FLAG_OUTPUT);
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * mfm_nec765_get_data_offset
 ****************************************************************************/
static int
mfm_nec765_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * mfm_nec765_get_data_size
 ****************************************************************************/
static int
mfm_nec765_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * mfm_nec765_read_options
 ****************************************************************************/
static struct format_option		mfm_nec765_read_options[] =
	{
	FORMAT_OPTION_BOOLEAN("ignore_sector_size",    MAGIC_IGNORE_SECTOR_SIZE,    1),
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
 * mfm_nec765_write_options
 ****************************************************************************/
static struct format_option		mfm_nec765_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("prolog_value",  MAGIC_PROLOG_VALUE,  1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("epilog_value",  MAGIC_EPILOG_VALUE,  1),
	FORMAT_OPTION_INTEGER("fill_length1",  MAGIC_FILL_LENGTH1,  1),
	FORMAT_OPTION_INTEGER("fill_value1",   MAGIC_FILL_VALUE1,   1),
	FORMAT_OPTION_INTEGER("fill_length2",  MAGIC_FILL_LENGTH2,  1),
	FORMAT_OPTION_INTEGER("fill_value2",   MAGIC_FILL_VALUE2,   1),
	FORMAT_OPTION_INTEGER("fill_length3",  MAGIC_FILL_LENGTH3,  1),
	FORMAT_OPTION_INTEGER("fill_value3",   MAGIC_FILL_VALUE3,   1),
	FORMAT_OPTION_INTEGER("fill_length4",  MAGIC_FILL_LENGTH4,  1),
	FORMAT_OPTION_INTEGER("fill_value4",   MAGIC_FILL_VALUE4,   1),
	FORMAT_OPTION_INTEGER("fill_length5",  MAGIC_FILL_LENGTH5,  1),
	FORMAT_OPTION_INTEGER("fill_value5",   MAGIC_FILL_VALUE5,   1),
	FORMAT_OPTION_INTEGER("fill_length6",  MAGIC_FILL_LENGTH6,  1),
	FORMAT_OPTION_INTEGER("fill_value6",   MAGIC_FILL_VALUE6,   1),
	FORMAT_OPTION_INTEGER("fill_length7",  MAGIC_FILL_LENGTH7,  1),
	FORMAT_OPTION_INTEGER("fill_value7",   MAGIC_FILL_VALUE7,   1),
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       9),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * mfm_nec765_rw_options
 ****************************************************************************/
static struct format_option		mfm_nec765_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",            MAGIC_SECTORS,             1),
	FORMAT_OPTION_INTEGER("sync_length",        MAGIC_SYNC_LENGTH,         1),
	FORMAT_OPTION_INTEGER("sync_value",         MAGIC_SYNC_LENGTH,         1),
	FORMAT_OPTION_INTEGER("crc16_init_value",   MAGIC_CRC16_INIT_VALUE,    1),
	FORMAT_OPTION_INTEGER("id_address_mark",    MAGIC_ID_ADDRESS_MARK,     1),
	FORMAT_OPTION_INTEGER("data_address_mark1", MAGIC_DATA_ADDRESS_MARK1,  1),
	FORMAT_OPTION_INTEGER("data_address_mark2", MAGIC_DATA_ADDRESS_MARK2,  1),
	FORMAT_OPTION_INTEGER_OBSOLETE("track_step",         MAGIC_TRACK_STEP,          1),
	FORMAT_OPTION_INTEGER("sector_sizes",       MAGIC_SECTOR_SIZES,       -1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",             MAGIC_BOUNDS_OLD,          9),
	FORMAT_OPTION_INTEGER("bounds_old",         MAGIC_BOUNDS_OLD,          9),
	FORMAT_OPTION_INTEGER("bounds_new",         MAGIC_BOUNDS_NEW,          9),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * mfm_nec765_format_desc
 ****************************************************************************/
struct format_desc			mfm_nec765_format_desc =
	{
	.name             = "mfm_nec765",
	.level            = 3,
	.set_defaults     = mfm_nec765_set_defaults,
	.set_read_option  = mfm_nec765_set_read_option,
	.set_write_option = mfm_nec765_set_write_option,
	.set_rw_option    = mfm_nec765_set_rw_option,
	.get_sectors      = mfm_nec765_get_sectors,
	.get_sector_size  = mfm_nec765_get_sector_size,
	.get_flags        = mfm_nec765_get_flags,
	.get_data_offset  = mfm_nec765_get_data_offset,
	.get_data_size    = mfm_nec765_get_data_size,
	.track_statistics = mfm_nec765_statistics,
	.track_read       = mfm_nec765_read_track,
	.track_write      = mfm_nec765_write_track,
	.fmt_opt_rd       = mfm_nec765_read_options,
	.fmt_opt_wr       = mfm_nec765_write_options,
	.fmt_opt_rw       = mfm_nec765_rw_options
	};
/******************************************************** Karsten Scheibler */

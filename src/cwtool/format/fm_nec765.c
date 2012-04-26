/****************************************************************************
 ****************************************************************************
 *
 * format/fm_nec765.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "fm_nec765.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "fm.h"
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




#define HEADER_SIZE			6
#define DATA_SIZE			16386
#define FLAG_RD_IGNORE_SECTOR_SIZE	(1 << 0)
#define FLAG_RD_IGNORE_CHECKSUMS	(1 << 1)
#define FLAG_RD_IGNORE_TRACK_MISMATCH	(1 << 2)
#define FLAG_RD_MATCH_SIMPLE		(1 << 3)
#define FLAG_RD_MATCH_SIMPLE_FIXUP	(1 << 4)
#define FLAG_RD_POSTCOMP_SIMPLE		(1 << 5)
#define FLAG_RW_CRC16_INIT_VALUE1_SET	(1 << 0)
#define FLAG_RW_CRC16_INIT_VALUE2_SET	(1 << 1)
#define FLAG_RW_CRC16_INIT_VALUE3_SET	(1 << 2)



/****************************************************************************
 * fm_nec765_track_number
 ****************************************************************************/
static cw_count_t
fm_nec765_track_number(
	struct fm_nec765		*fm_nec,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if ((format_track == -1) && (format_side == -1)) return (cwtool_track / 2);
	return (format_track);
	}



/****************************************************************************
 * fm_nec765_side_number
 ****************************************************************************/
static cw_count_t
fm_nec765_side_number(
	struct fm_nec765		*fm_nec,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if ((format_track == -1) && (format_side == -1)) return (cwtool_track % 2);
	return (format_side);
	}



/****************************************************************************
 * fm_nec765_sector_shift
 ****************************************************************************/
static int
fm_nec765_sector_shift(
	struct fm_nec765		*fm_nec,
	int				sector)

	{
	return (fm_get_sector_shift(fm_nec->rw.pshift, sector, GLOBAL_NR_SECTORS));
	}



/****************************************************************************
 * fm_nec765_sector_size
 ****************************************************************************/
static int
fm_nec765_sector_size(
	struct fm_nec765		*fm_nec,
	int				sector)

	{
	int				shift = fm_nec765_sector_shift(fm_nec, sector);

	if (shift == -1) shift = 0;
	return (0x80 << shift);
	}



/****************************************************************************
 * fm_nec765_read_sector2
 ****************************************************************************/
static int
fm_nec765_read_sector2(
	struct fifo			*ffo_l1,
	struct fm_nec765		*fm_nec,
	struct disk_error		*dsk_err,
	struct range_sector		*rng_sec,
	unsigned char			*header,
	unsigned char			*data)

	{
	int				bitofs, data_size, result;

	*dsk_err = (struct disk_error) { };
	if (fm_read_sync(ffo_l1, range_sector_header(rng_sec), fm_nec->rw.sync_value1, fm_nec->rw.sync_value1) == -1) return (-1);
	bitofs = fifo_get_rd_bitofs(ffo_l1);
	if (fm_read_bytes(ffo_l1, dsk_err, header, HEADER_SIZE) == -1) return (-1);
	range_set_end(range_sector_header(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	data_size = fm_nec765_sector_size(fm_nec, header[2] - 1);
	result = fm_read_sync(ffo_l1, range_sector_data(rng_sec), fm_nec->rw.sync_value2, fm_nec->rw.sync_value3);
	if (result == -1) return (-1);
	if (fm_read_bytes(ffo_l1, dsk_err, data, data_size + 2) == -1) return (-1);
	range_set_end(range_sector_data(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	verbose_message(GENERIC, 2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (result);
	}



/****************************************************************************
 * fm_nec765_write_sector2
 ****************************************************************************/
static int
fm_nec765_write_sector2(
	struct fifo			*ffo_l1,
	struct fm_nec765		*fm_nec,
	unsigned char			*header,
	unsigned char			*data,
	int				data_size)

	{
	if (fm_write_fill(ffo_l1, fm_nec->wr.fill_value2, fm_nec->wr.fill_length2) == -1) return (-1);
	if (fm_write_fill(ffo_l1, fm_nec->wr.fill_value3, fm_nec->wr.fill_length3) == -1) return (-1);
	if (fm_write_sync(ffo_l1, fm_nec->rw.sync_value1, 1) == -1) return (-1);
	if (fm_write_bytes(ffo_l1, header, HEADER_SIZE) == -1) return (-1);
	if (fm_write_fill(ffo_l1, fm_nec->wr.fill_value4, fm_nec->wr.fill_length4) == -1) return (-1);
	if (fm_write_fill(ffo_l1, fm_nec->wr.fill_value5, fm_nec->wr.fill_length5) == -1) return (-1);
	if (fm_write_sync(ffo_l1, fm_nec->rw.sync_value2, 1) == -1) return (-1);
	if (fm_write_bytes(ffo_l1, data, data_size) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * fm_nec765_read_sector
 ****************************************************************************/
static int
fm_nec765_read_sector(
	struct fifo			*ffo_l1,
	struct fm_nec765		*fm_nec,
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
	int				init = fm_nec->rw.crc16_init_value2;

	result = fm_nec765_read_sector2(ffo_l1, fm_nec, &dsk_err, &rng_sec, header, data);
	if (result == -1) return (-1);
	if (result == 1) init = fm_nec->rw.crc16_init_value3;

	/* accept only valid sector numbers */

	track  = fm_nec765_track_number(fm_nec, cwtool_track, format_track, format_side);
	side   = fm_nec765_side_number(fm_nec, cwtool_track, format_track, format_side);
	sector = header[2] - 1;
	if ((sector < 0) || (sector >= fm_nec->rw.sectors))
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	data_size = 1 << (header[3] + 7);
	result = format_compare2("sector size: got %d, expected %d", data_size, fm_nec765_sector_size(fm_nec, sector));
	if (result > 0) verbose_message(GENERIC, 2, "wrong sector size on sector %d", sector);
	if (fm_nec->rd.flags & FLAG_RD_IGNORE_SECTOR_SIZE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_SIZE, result);
	data_size = fm_nec765_sector_size(fm_nec, sector);

	result = format_compare2("header crc16 checksum: got 0x%04x, expected 0x%04x", fm_read_u16_be(&header[4]), fm_crc16(fm_nec->rw.crc16_init_value1, header, 4));
	result += format_compare2("data crc16 checksum: got 0x%04x, expected 0x%04x", fm_read_u16_be(&data[data_size]), fm_crc16(init, data, data_size));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (fm_nec->rd.flags & FLAG_RD_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", header[0], track);
	result += format_compare2("side: got %d, expected %d", header[1], side);
	if (result > 0) verbose_message(GENERIC, 2, "track or side mismatch on sector %d", sector);
	if (fm_nec->rd.flags & FLAG_RD_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	/*
	 * take the data if the found sector is of better quality than the
	 * current one
	 */

	range_sector_set_number(&rng_sec, sector);
	if (con != NULL) container_append_range_sector(con, &rng_sec);
	disk_set_sector_number(&dsk_sct[sector], sector);
	disk_sector_read(&dsk_sct[sector], &dsk_err, data);
	return (1);
	}



/****************************************************************************
 * fm_nec765_write_sector
 ****************************************************************************/
static int
fm_nec765_write_sector(
	struct fifo			*ffo_l1,
	struct fm_nec765		*fm_nec,
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
	header[0] = fm_nec765_track_number(fm_nec, cwtool_track, format_track, format_side);
	header[1] = fm_nec765_side_number(fm_nec, cwtool_track, format_track, format_side);
	header[2] = sector + 1;
	header[3] = fm_nec765_sector_shift(fm_nec, sector);
	data_size = fm_nec765_sector_size(fm_nec, sector);
	fm_write_u16_be(&header[4], fm_crc16(fm_nec->rw.crc16_init_value1, header, 4));
	disk_sector_write(data, dsk_sct);
	fm_write_u16_be(&data[data_size], fm_crc16(fm_nec->rw.crc16_init_value2, data, data_size));
	return (fm_nec765_write_sector2(ffo_l1, fm_nec, header, data, data_size + 2));
	}



/****************************************************************************
 * fm_nec765_statistics
 ****************************************************************************/
static int
fm_nec765_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(
		ffo_l0,
		cwtool_track,
		fm_nec765_track_number(&fmt->fm_nec, cwtool_track, format_track, format_side),
		fm_nec765_side_number(&fmt->fm_nec, cwtool_track, format_track, format_side));
	if (fmt->fm_nec.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) histogram_postcomp_simple(
		ffo_l0,
		fmt->fm_nec.rw.bnd,
		2,
		cwtool_track,
		fm_nec765_track_number(&fmt->fm_nec, cwtool_track, format_track, format_side),
		fm_nec765_side_number(&fmt->fm_nec, cwtool_track, format_track, format_side));
	return (1);
	}



/****************************************************************************
 * fm_nec765_read_track2
 ****************************************************************************/
static void
fm_nec765_read_track2(
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

	if (fmt->fm_nec.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) postcomp_simple(ffo_l0, fmt->fm_nec.rw.bnd, 2);
	bitstream_read(ffo_l0, &ffo_l1, fmt->fm_nec.rw.bnd, 2);
	while (fm_nec765_read_sector(&ffo_l1, &fmt->fm_nec, con, dsk_sct, cwtool_track, format_track, format_side) != -1) ;
	}



/****************************************************************************
 * fm_nec765_read_track
 ****************************************************************************/
static int
fm_nec765_read_track(
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
		.bnd          = fmt->fm_nec.rw.bnd,
		.bnd_size     = 2,
		.callback     = fm_nec765_read_track2,
		.merge_two    = fmt->fm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.merge_all    = fmt->fm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.fixup        = fmt->fm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE_FIXUP
		};

	if ((fmt->fm_nec.rd.flags & FLAG_RD_MATCH_SIMPLE) || (options_get_output())) match_simple(&mat_sim_nfo);
	else fm_nec765_read_track2(fmt, NULL, ffo_l0, ffo_l3, dsk_sct, cwtool_track, format_track, format_side);
	return (1);
	}



/****************************************************************************
 * fm_nec765_write_track
 ****************************************************************************/
static int
fm_nec765_write_track(
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

	if (fm_write_fill(&ffo_l1, fmt->fm_nec.wr.prolog_value, fmt->fm_nec.wr.prolog_length) == -1) return (0);
	if (fm_write_fill(&ffo_l1, fmt->fm_nec.wr.fill_value1, fmt->fm_nec.wr.fill_length1)   == -1) return (0);
	if (fm_write_sync(&ffo_l1, 0xf77a, 1) == -1) return (0);
	for (i = 0; i < fmt->fm_nec.rw.sectors; i++) if (fm_nec765_write_sector(&ffo_l1, &fmt->fm_nec, &dsk_sct[i], cwtool_track, format_track, format_side) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (fm_write_fill(&ffo_l1, fmt->fm_nec.wr.fill_value6, fmt->fm_nec.wr.fill_length6)   == -1) return (0);
	if (fm_write_fill(&ffo_l1, fmt->fm_nec.wr.fill_value7, fmt->fm_nec.wr.fill_length7)   == -1) return (0);
	if (fm_write_fill(&ffo_l1, fmt->fm_nec.wr.epilog_value, fmt->fm_nec.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (bitstream_write(&ffo_l1, ffo_l0, fmt->fm_nec.rw.bnd, fmt->fm_nec.wr.precomp, 2) == -1) return (0);
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
#define MAGIC_MATCH_SIMPLE		4
#define MAGIC_MATCH_SIMPLE_FIXUP	5
#define MAGIC_POSTCOMP_SIMPLE		6
#define MAGIC_PROLOG_LENGTH		7
#define MAGIC_PROLOG_VALUE		8
#define MAGIC_EPILOG_LENGTH		9
#define MAGIC_EPILOG_VALUE		10
#define MAGIC_FILL_LENGTH1		11
#define MAGIC_FILL_VALUE1		12
#define MAGIC_FILL_LENGTH2		13
#define MAGIC_FILL_VALUE2		14
#define MAGIC_FILL_LENGTH3		15
#define MAGIC_FILL_VALUE3		16
#define MAGIC_FILL_LENGTH4		17
#define MAGIC_FILL_VALUE4		18
#define MAGIC_FILL_LENGTH5		19
#define MAGIC_FILL_VALUE5		20
#define MAGIC_FILL_LENGTH6		21
#define MAGIC_FILL_VALUE6		22
#define MAGIC_FILL_LENGTH7		23
#define MAGIC_FILL_VALUE7		24
#define MAGIC_PRECOMP			25
#define MAGIC_SECTORS			26
#define MAGIC_SYNC_VALUE1		27
#define MAGIC_SYNC_VALUE2		28
#define MAGIC_SYNC_VALUE3		29
#define MAGIC_CRC16_INIT_VALUE1		30
#define MAGIC_CRC16_INIT_VALUE2		31
#define MAGIC_CRC16_INIT_VALUE3		32
#define MAGIC_SECTOR_SIZES		33
#define MAGIC_BOUNDS_OLD		34
#define MAGIC_BOUNDS_NEW		35



/****************************************************************************
 * fm_nec765_set_crc16_init_value
 ****************************************************************************/
static int
fm_nec765_set_crc16_init_value(
	struct fm_nec765		*fm_nec,
	int				mask,
	int				sync_value,
	unsigned short			*init_value)

	{
	unsigned char			data;
	int				i;

	if (fm_nec->rw.flags & mask) return (1);
	for (data = 0, i = 0x4000; i > 0; i >>= 2) data = (data << 1) | ((sync_value & i) ? 1 : 0);
	*init_value = fm_crc16(0xffff, &data, 1);
	debug_message(GENERIC, 2, "calculated crc16_init_value1 = 0x%04x", *init_value);
	return (1);
	}



/****************************************************************************
 * fm_nec765_set_defaults
 ****************************************************************************/
static void
fm_nec765_set_defaults(
	union format			*fmt)

	{
	const static struct fm_nec765	fm_nec =
		{
		.rd =
			{
			.flags = 0
			},
		.wr =
			{
			.prolog_length = 40,
			.epilog_length = 274,
			.prolog_value  = 0x4e,
			.epilog_value  = 0x4e,
			.fill_length1  = 6,
			.fill_value1   = 0x00,
			.fill_length2  = 26,
			.fill_value2   = 0x4e,
			.fill_length3  = 6,
			.fill_value3   = 0x00,
			.fill_length4  = 11,
			.fill_value4   = 0x4e,
			.fill_length5  = 6,
			.fill_value5   = 0x00,
			.fill_length6  = 27,
			.fill_value6   = 0x4e,
			.fill_length7  = 6,
			.fill_value7   = 0x00,
			.precomp       = { }
			},
		.rw =
			{
			.sectors            = 16,
			.sync_value1        = 0xf57e,
			.sync_value2        = 0xf56f,
			.sync_value3        = 0xf56a,
			.crc16_init_value1  = 0,
			.crc16_init_value2  = 0,
			.crc16_init_value3  = 0,
			.flags              = 0,
			.bnd                =
				{
				BOUNDS_NEW(0x0800, 0x1a52, 0x2a00, 0),
				BOUNDS_NEW(0x2b00, 0x36a5, 0x4800, 1)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->fm_nec = fm_nec;
	fm_nec765_set_crc16_init_value(&fmt->fm_nec,
		FLAG_RW_CRC16_INIT_VALUE1_SET,
		fmt->fm_nec.rw.sync_value1,
		&fmt->fm_nec.rw.crc16_init_value1);
	fm_nec765_set_crc16_init_value(&fmt->fm_nec,
		FLAG_RW_CRC16_INIT_VALUE2_SET,
		fmt->fm_nec.rw.sync_value2,
		&fmt->fm_nec.rw.crc16_init_value2);
	fm_nec765_set_crc16_init_value(&fmt->fm_nec,
		FLAG_RW_CRC16_INIT_VALUE3_SET,
		fmt->fm_nec.rw.sync_value3,
		&fmt->fm_nec.rw.crc16_init_value3);
	fm_fill_sector_shift(fmt->fm_nec.rw.pshift, 0, GLOBAL_NR_SECTORS, 0);
	}



/****************************************************************************
 * fm_nec765_set_read_option
 ****************************************************************************/
static int
fm_nec765_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_IGNORE_SECTOR_SIZE)    return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_IGNORE_SECTOR_SIZE));
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_IGNORE_TRACK_MISMATCH));
	if (magic == MAGIC_MATCH_SIMPLE)          return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_MATCH_SIMPLE));
	if (magic == MAGIC_MATCH_SIMPLE_FIXUP)    return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_MATCH_SIMPLE_FIXUP));
	debug_error_condition(magic != MAGIC_POSTCOMP_SIMPLE);
	return (setvalue_uchar_bit(&fmt->fm_nec.rd.flags, val, FLAG_RD_POSTCOMP_SIMPLE));
	}



/****************************************************************************
 * fm_nec765_set_write_option
 ****************************************************************************/
static int
fm_nec765_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->fm_nec.wr.prolog_length, val, 1, 0xffff));
	if (magic == MAGIC_PROLOG_VALUE)  return (setvalue_uchar(&fmt->fm_nec.wr.prolog_value, val, 0, 0xff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->fm_nec.wr.epilog_length, val, 1, 0xffff));
	if (magic == MAGIC_EPILOG_VALUE)  return (setvalue_uchar(&fmt->fm_nec.wr.epilog_value, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH1)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length1, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE1)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value1, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH2)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length2, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE2)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value2, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH3)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length3, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE3)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value3, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH4)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length4, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE4)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value4, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH5)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length5, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE5)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value5, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH6)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length6, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE6)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value6, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH7)  return (setvalue_uchar(&fmt->fm_nec.wr.fill_length7, val, 0, 0xff));
	if (magic == MAGIC_FILL_VALUE7)   return (setvalue_uchar(&fmt->fm_nec.wr.fill_value7, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->fm_nec.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * fm_nec765_set_rw_option
 ****************************************************************************/
static int
fm_nec765_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)
		{
		fm_fill_sector_shift(fmt->fm_nec.rw.pshift, fmt->fm_nec.rw.sectors, GLOBAL_NR_SECTORS, 0);
		return (setvalue_uchar(&fmt->fm_nec.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
		}
	if (magic == MAGIC_SYNC_VALUE1)
		{
		if (! setvalue_ushort(&fmt->fm_nec.rw.sync_value1, val, 0, 0xffff)) return (0);
		return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE1_SET,
			fmt->fm_nec.rw.sync_value1,
			&fmt->fm_nec.rw.crc16_init_value1));
		}
	if (magic == MAGIC_SYNC_VALUE2)
		{
		if (! setvalue_ushort(&fmt->fm_nec.rw.sync_value2, val, 0, 0xffff)) return (0);
		return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE2_SET,
			fmt->fm_nec.rw.sync_value2,
			&fmt->fm_nec.rw.crc16_init_value2));
		}
	if (magic == MAGIC_SYNC_VALUE3)
		{
		if (! setvalue_ushort(&fmt->fm_nec.rw.sync_value3, val, 0, 0xffff)) return (0);
		return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE2_SET,
			fmt->fm_nec.rw.sync_value2,
			&fmt->fm_nec.rw.crc16_init_value2));
		}
	if (magic == MAGIC_CRC16_INIT_VALUE1)
		{
		setvalue_uchar_bit(&fmt->fm_nec.rw.flags, (val < 0) ? 0 : 1, FLAG_RW_CRC16_INIT_VALUE1_SET);
		if (val < 0) return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE1_SET,
			fmt->fm_nec.rw.sync_value1,
			&fmt->fm_nec.rw.crc16_init_value1));
		return (setvalue_ushort(&fmt->fm_nec.rw.crc16_init_value1, val, 0, 0xffff));
		}
	if (magic == MAGIC_CRC16_INIT_VALUE2)
		{
		setvalue_uchar_bit(&fmt->fm_nec.rw.flags, (val < 0) ? 0 : 1, FLAG_RW_CRC16_INIT_VALUE2_SET);
		if (val < 0) return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE2_SET,
			fmt->fm_nec.rw.sync_value2,
			&fmt->fm_nec.rw.crc16_init_value2));
		return (setvalue_ushort(&fmt->fm_nec.rw.crc16_init_value2, val, 0, 0xffff));
		}
	if (magic == MAGIC_CRC16_INIT_VALUE3)
		{
		setvalue_uchar_bit(&fmt->fm_nec.rw.flags, (val < 0) ? 0 : 1, FLAG_RW_CRC16_INIT_VALUE3_SET);
		if (val < 0) return (fm_nec765_set_crc16_init_value(&fmt->fm_nec,
			FLAG_RW_CRC16_INIT_VALUE3_SET,
			fmt->fm_nec.rw.sync_value3,
			&fmt->fm_nec.rw.crc16_init_value3));
		return (setvalue_ushort(&fmt->fm_nec.rw.crc16_init_value3, val, 0, 0xffff));
		}
	if (magic == MAGIC_SECTOR_SIZES) return (fm_set_sector_size(fmt->fm_nec.rw.pshift, ofs, GLOBAL_NR_SECTORS, val));
	if (magic == MAGIC_BOUNDS_OLD)   return (setvalue_bounds_old(fmt->fm_nec.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->fm_nec.rw.bnd, val, ofs));
	}



/****************************************************************************
 * fm_nec765_get_sector_size
 ****************************************************************************/
static int
fm_nec765_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	int				i, s;

	debug_error_condition(sector >= fmt->fm_nec.rw.sectors);
	if (sector >= 0) return (fm_nec765_sector_size(&fmt->fm_nec, sector));
	for (i = s = 0; i < fmt->fm_nec.rw.sectors; i++) s += fm_nec765_sector_size(&fmt->fm_nec, i);
	return (s);
	}



/****************************************************************************
 * fm_nec765_get_sectors
 ****************************************************************************/
static int
fm_nec765_get_sectors(
	union format			*fmt)

	{
	return (fmt->fm_nec.rw.sectors);
	}



/****************************************************************************
 * fm_nec765_get_flags
 ****************************************************************************/
static int
fm_nec765_get_flags(
	union format			*fmt)

	{
	if (options_get_output()) return (FORMAT_FLAG_OUTPUT);
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * fm_nec765_get_data_offset
 ****************************************************************************/
static int
fm_nec765_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * fm_nec765_get_data_size
 ****************************************************************************/
static int
fm_nec765_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * fm_nec765_read_options
 ****************************************************************************/
static struct format_option		fm_nec765_read_options[] =
	{
	FORMAT_OPTION_BOOLEAN("ignore_sector_size",    MAGIC_IGNORE_SECTOR_SIZE,    1),
	FORMAT_OPTION_BOOLEAN("ignore_checksums",      MAGIC_IGNORE_CHECKSUMS,      1),
	FORMAT_OPTION_BOOLEAN("ignore_track_mismatch", MAGIC_IGNORE_TRACK_MISMATCH, 1),
	FORMAT_OPTION_BOOLEAN("match_simple",          MAGIC_MATCH_SIMPLE,          1),
	FORMAT_OPTION_BOOLEAN("match_simple_fixup",    MAGIC_MATCH_SIMPLE_FIXUP,    1),
	FORMAT_OPTION_BOOLEAN_COMPAT("postcomp",              MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_BOOLEAN("postcomp_simple",       MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * fm_nec765_write_options
 ****************************************************************************/
static struct format_option		fm_nec765_write_options[] =
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
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       4),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * fm_nec765_rw_options
 ****************************************************************************/
static struct format_option		fm_nec765_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",           MAGIC_SECTORS,            1),
	FORMAT_OPTION_INTEGER("sync_value1",       MAGIC_SYNC_VALUE1,        1),
	FORMAT_OPTION_INTEGER("sync_value2",       MAGIC_SYNC_VALUE2,        1),
	FORMAT_OPTION_INTEGER("sync_value3",       MAGIC_SYNC_VALUE3,        1),
	FORMAT_OPTION_INTEGER("crc16_init_value1", MAGIC_CRC16_INIT_VALUE1,  1),
	FORMAT_OPTION_INTEGER("crc16_init_value2", MAGIC_CRC16_INIT_VALUE2,  1),
	FORMAT_OPTION_INTEGER("crc16_init_value3", MAGIC_CRC16_INIT_VALUE3,  1),
	FORMAT_OPTION_INTEGER("sector_sizes",      MAGIC_SECTOR_SIZES,      -1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",            MAGIC_BOUNDS_OLD,         6),
	FORMAT_OPTION_INTEGER("bounds_old",        MAGIC_BOUNDS_OLD,         6),
	FORMAT_OPTION_INTEGER("bounds_new",        MAGIC_BOUNDS_NEW,         6),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * fm_nec765_format_desc
 ****************************************************************************/
struct format_desc			fm_nec765_format_desc =
	{
	.name             = "fm_nec765",
	.level            = 3,
	.set_defaults     = fm_nec765_set_defaults,
	.set_read_option  = fm_nec765_set_read_option,
	.set_write_option = fm_nec765_set_write_option,
	.set_rw_option    = fm_nec765_set_rw_option,
	.get_sectors      = fm_nec765_get_sectors,
	.get_sector_size  = fm_nec765_get_sector_size,
	.get_flags        = fm_nec765_get_flags,
	.get_data_offset  = fm_nec765_get_data_offset,
	.get_data_size    = fm_nec765_get_data_size,
	.track_statistics = fm_nec765_statistics,
	.track_read       = fm_nec765_read_track,
	.track_write      = fm_nec765_write_track,
	.fmt_opt_rd       = fm_nec765_read_options,
	.fmt_opt_wr       = fm_nec765_write_options,
	.fmt_opt_rw       = fm_nec765_rw_options
	};
/******************************************************** Karsten Scheibler */

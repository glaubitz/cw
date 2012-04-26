/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_v9000.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "gcr_v9000.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "gcr.h"
#include "range.h"
#include "bitstream.h"
#include "container.h"
#include "match_simple.h"
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
	struct range			*rng,
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
	range_set_start(rng, fifo_get_rd_bitofs(ffo_l1) - i);
	return (i);
	}



/****************************************************************************
 * gcr_write_sync
 ****************************************************************************/
static int
gcr_write_sync(
	struct fifo			*ffo_l1,
	int				size)

	{
	int				i, val;

	verbose_message(GENERIC, 2, "writing sync at bit offset %d with %d bits", fifo_get_wr_bitofs(ffo_l1), size);
	for (; size > 0; size -= i)
		{
		i = (size > 16) ? 16 : size;
		val = 0xffff & ((1 << i) - 1);
		if (fifo_write_bits(ffo_l1, val, i) == -1) return (-1);
		}
	return (0);
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
	verbose_message(GENERIC, 2, "writing fill at bit offset %d with value 0x%02x", fifo_get_wr_bitofs(ffo_l1), val);
	while (size-- > 0) if (fifo_write_bits(ffo_l1, val, 8) == -1) return (-1);
	return (0);
	}



/****************************************************************************
 * gcr_read_bytes
 ****************************************************************************/
static int
gcr_read_bytes(
	struct fifo			*ffo_l1,
	struct disk_error		*dsk_err,
	unsigned char			*data,
	int				size)

	{
	const static unsigned char	decode[32] =
		{
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0x08, 0x00, 0x01, 0xff, 0x0c, 0x04, 0x05,
		0xff, 0xff, 0x02, 0x03, 0xff, 0x0f, 0x06, 0x07,
		0xff, 0x09, 0x0a, 0x0b, 0xff, 0x0d, 0x0e, 0xff
		};
	int				i, n1, n2;
	int				bitofs = fifo_get_rd_bitofs(ffo_l1);

	for (i = 0; i < size; i++)
		{
		n1 = fifo_read_bits(ffo_l1, 5);
		if (n1 == -1) return (-1);
		n2 = fifo_read_bits(ffo_l1, 5);
		if (n2 == -1) return (-1);
		if ((decode[n1] == 0xff) || (decode[n2] == 0xff))
			{
			verbose_message(GENERIC, 3, "gcr decode error around bit offset %d (byte %d), got nybbles 0x%02x 0x%02x", fifo_get_rd_bitofs(ffo_l1) - 10, i, n1, n2);
			disk_error_add(dsk_err, DISK_ERROR_FLAG_ENCODING, 1);
			}
		data[i] = (decode[n1] << 4) | decode[n2];
		}
	verbose_message(GENERIC, 2, "read %d bytes at bit offset %d", i, bitofs);
	return (0);
	}



/****************************************************************************
 * gcr_write_bytes
 ****************************************************************************/
static int
gcr_write_bytes(
	struct fifo			*ffo_l1,
	unsigned char			*data,
	int				size)

	{
	const static unsigned char	encode[16] =
		{
		0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17,
		0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15
		};
	int				i, n1, n2;

	verbose_message(GENERIC, 2, "writing %d bytes at bit offset %d", size, fifo_get_wr_bitofs(ffo_l1));
	for (i = 0; i < size; i++)
		{
		n1 = (data[i] >> 4) & 0x0f;
		if (fifo_write_bits(ffo_l1, encode[n1], 5) == -1) return (-1);
		n2 = data[i] & 0x0f;
		if (fifo_write_bits(ffo_l1, encode[n2], 5) == -1) return (-1);
		}
	return (0);
	}




/****************************************************************************
 *
 * functions for sector and track handling
 *
 ****************************************************************************/




#define HEADER_SIZE			4
#define DATA_SIZE			516
#define FLAG_RD_IGNORE_CHECKSUMS	(1 << 0)
#define FLAG_RD_IGNORE_TRACK_MISMATCH	(1 << 1)
#define FLAG_RD_IGNORE_DATA_ID		(1 << 2)
#define FLAG_RD_MATCH_SIMPLE		(1 << 3)
#define FLAG_RD_MATCH_SIMPLE_FIXUP	(1 << 4)
#define FLAG_RD_POSTCOMP_SIMPLE		(1 << 5)
#define FLAG_RW_FLIP_TRACK_ID		(1 << 0)



/****************************************************************************
 * gcr_v9000_checksum
 ****************************************************************************/
static int
gcr_v9000_checksum(
	unsigned char			*data,
	int				len)

	{
	int				c, i;

	for (c = i = 0; i < len; i++) c += data[i];
	return (c);
	}



/****************************************************************************
 * gcr_v9000_header_checksum
 ****************************************************************************/
static int
gcr_v9000_header_checksum(
	unsigned char			*data,
	int				len)

	{
	return (gcr_v9000_checksum(data, len) & 0xff);
	}



/****************************************************************************
 * gcr_v9000_data_checksum
 ****************************************************************************/
static int
gcr_v9000_data_checksum(
	unsigned char			*data,
	int				len)

	{
	return (gcr_v9000_checksum(data, len) & 0xffff);
	}



/****************************************************************************
 * gcr_v9000_track_number
 ****************************************************************************/
static cw_count_t
gcr_v9000_track_number(
	struct gcr_v9000		*gcr_v9,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if (format_track == -1)
		{
		cw_count_t		track = cwtool_track, side;

		if (gcr_v9->rw.side_offset != 0)
			{
			side = (cwtool_track >= gcr_v9->rw.side_offset) ? 1 : 0;
			if (side > 0) track -= gcr_v9->rw.side_offset;
			}
		else
			{
			side = track % 2;
			track /= 2;
			}
		if (gcr_v9->rw.flags & FLAG_RW_FLIP_TRACK_ID) side ^= 1;
		return (128 * side + track);
		}
	return (format_track);
	}



/****************************************************************************
 * gcr_v9000_read_sector2
 ****************************************************************************/
static int
gcr_v9000_read_sector2(
	struct fifo			*ffo_l1,
	struct gcr_v9000		*gcr_v9,
	struct disk_error		*dsk_err,
	struct range_sector		*rng_sec,
	unsigned char			*header,
	unsigned char			*data)

	{
	int				bitofs;

	while (1)
		{
		*dsk_err = (struct disk_error) { };
		if (gcr_read_sync(ffo_l1, range_sector_header(rng_sec), gcr_v9->rd.sync_length1) == -1) return (-1);
		bitofs = fifo_get_rd_bitofs(ffo_l1);
		if (gcr_read_bytes(ffo_l1, dsk_err, header, HEADER_SIZE) == -1) return (-1);
		range_set_end(range_sector_header(rng_sec), fifo_get_rd_bitofs(ffo_l1));
		fifo_set_rd_bitofs(ffo_l1, bitofs);
		if (format_compare2("header_id: got 0x%02x, expected 0x%02x", header[0], gcr_v9->rw.header_id) == 0) break;
		}
	if (gcr_read_sync(ffo_l1, range_sector_data(rng_sec), gcr_v9->rd.sync_length2) == -1) return (-1);
	if (gcr_read_bytes(ffo_l1, dsk_err, data, DATA_SIZE) == -1) return (-1);
	range_set_end(range_sector_data(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	verbose_message(GENERIC, 2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (1);
	}



/****************************************************************************
 * gcr_v9000_write_sector2
 ****************************************************************************/
static int
gcr_v9000_write_sector2(
	struct fifo			*ffo_l1,
	struct gcr_v9000		*gcr_v9,
	unsigned char			*header,
	unsigned char			*data)

	{
	if (gcr_write_fill(ffo_l1, gcr_v9->wr.fill_value, gcr_v9->wr.fill_length) == -1) return (-1);
	if (gcr_write_sync(ffo_l1, gcr_v9->wr.sync_length1) == -1) return (-1);
	if (gcr_write_bytes(ffo_l1, header, HEADER_SIZE) == -1) return (-1);
	if (gcr_write_fill(ffo_l1, gcr_v9->wr.fill_value, gcr_v9->wr.fill_length) == -1) return (-1);
	if (gcr_write_sync(ffo_l1, gcr_v9->wr.sync_length2) == -1) return (-1);
	if (gcr_write_bytes(ffo_l1, data, DATA_SIZE) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * gcr_v9000_read_sector
 ****************************************************************************/
static int
gcr_v9000_read_sector(
	struct fifo			*ffo_l1,
	struct gcr_v9000		*gcr_v9,
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
	int				result, track, sector;

	if (gcr_v9000_read_sector2(ffo_l1, gcr_v9, &dsk_err, &rng_sec, header, data) == -1) return (-1);

	/* accept only valid sector numbers */

	sector = header[2];
	track  = gcr_v9000_track_number(gcr_v9, cwtool_track, format_track, format_side);
	if (sector >= gcr_v9->rw.sectors)
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	result = format_compare2("header checksum: got 0x%02x, expected 0x%02x", header[3], gcr_v9000_header_checksum(&header[1], HEADER_SIZE - 2));
	result += format_compare2("data checksum: got 0x%04x, expected 0x%04x", gcr_read_u16_le(&data[513]), gcr_v9000_data_checksum(&data[1], DATA_SIZE - 4));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (gcr_v9->rd.flags & FLAG_RD_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", header[1], track);
	if (result > 0) verbose_message(GENERIC, 2, "track mismatch on sector %d", sector);
	if (gcr_v9->rd.flags & FLAG_RD_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare2("data_id: got 0x%02x, expected 0x%02x", data[0], gcr_v9->rw.data_id);
	if (result > 0) verbose_message(GENERIC, 2, "wrong data_id on sector %d", sector);
	if (gcr_v9->rd.flags & FLAG_RD_IGNORE_DATA_ID) disk_warning_add(&dsk_err, result);
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
 * gcr_v9000_write_sector
 ****************************************************************************/
static int
gcr_v9000_write_sector(
	struct fifo			*ffo_l1,
	struct gcr_v9000		*gcr_v9,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			header[HEADER_SIZE];
	unsigned char			data[DATA_SIZE];
	int				sector = disk_get_sector_number(dsk_sct);

	verbose_message(GENERIC, 1, "writing sector %d", sector);
	header[0] = gcr_v9->rw.header_id;
	header[1] = gcr_v9000_track_number(gcr_v9, cwtool_track, format_track, format_side);
	header[2] = sector;
	header[3] = gcr_v9000_header_checksum(&header[1], (HEADER_SIZE - 2));
	data[0]   = gcr_v9->rw.data_id;
	disk_sector_write(&data[1], dsk_sct);
	gcr_write_u16_le(&data[513], gcr_v9000_data_checksum(&data[1], DATA_SIZE - 4));
	data[515] = 0;
	return (gcr_v9000_write_sector2(ffo_l1, gcr_v9, header, data));
	}



/****************************************************************************
 * gcr_v9000_statistics
 ****************************************************************************/
static int
gcr_v9000_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(
		ffo_l0,
		cwtool_track,
		gcr_v9000_track_number(&fmt->gcr_v9, cwtool_track, format_track, format_side),
		-1);
	if (fmt->gcr_v9.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) histogram_postcomp_simple(
		ffo_l0,
		fmt->gcr_v9.rw.bnd,
		3,
		cwtool_track,
		gcr_v9000_track_number(&fmt->gcr_v9, cwtool_track, format_track, format_side),
		-1);
/*	histogram_analyze_gcr(
		ffo_l0,
		fmt->gcr_v9.rw.bnd,
		3,
		cwtool_track,
		gcr_v9000_track_number(&fmt->gcr_v9, cwtool_track, format_track, format_side),
		-1);*/
	return (1);
	}



/****************************************************************************
 * gcr_v9000_read_track2
 ****************************************************************************/
static void
gcr_v9000_read_track2(
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

	if (fmt->gcr_v9.rd.flags & FLAG_RD_POSTCOMP_SIMPLE) postcomp_simple_adjust(ffo_l0, fmt->gcr_v9.rw.bnd, 3, fmt->gcr_v9.rd.postcomp_simple_adjust[0], fmt->gcr_v9.rd.postcomp_simple_adjust[1]);
	bitstream_read(ffo_l0, &ffo_l1, fmt->gcr_v9.rw.bnd, 3);
	while (gcr_v9000_read_sector(&ffo_l1, &fmt->gcr_v9, con, dsk_sct, cwtool_track, format_track, format_side) != -1) ;
	}



/****************************************************************************
 * gcr_v9000_read_track
 ****************************************************************************/
static int
gcr_v9000_read_track(
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
		.bnd          = fmt->gcr_v9.rw.bnd,
		.bnd_size     = 3,
		.callback     = gcr_v9000_read_track2,
		.merge_two    = fmt->gcr_v9.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.merge_all    = fmt->gcr_v9.rd.flags & FLAG_RD_MATCH_SIMPLE,
		.fixup        = fmt->gcr_v9.rd.flags & FLAG_RD_MATCH_SIMPLE_FIXUP
		};

	if ((fmt->gcr_v9.rd.flags & FLAG_RD_MATCH_SIMPLE) || (options_get_output())) match_simple(&mat_sim_nfo);
	else gcr_v9000_read_track2(fmt, NULL, ffo_l0, ffo_l3, dsk_sct, cwtool_track, format_track, format_side);
	return (1);
	}



/****************************************************************************
 * gcr_v9000_write_track
 ****************************************************************************/
static int
gcr_v9000_write_track(
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

	if (gcr_write_fill(&ffo_l1, fmt->gcr_v9.wr.prolog_value, fmt->gcr_v9.wr.prolog_length) == -1) return (0);
	for (i = 0; i < fmt->gcr_v9.rw.sectors; i++) if (gcr_v9000_write_sector(&ffo_l1, &fmt->gcr_v9, &dsk_sct[i], cwtool_track, format_track, format_side) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (gcr_write_fill(&ffo_l1, fmt->gcr_v9.wr.epilog_value, fmt->gcr_v9.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (bitstream_write(&ffo_l1, ffo_l0, fmt->gcr_v9.rw.bnd, fmt->gcr_v9.wr.precomp, 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_SYNC_LENGTH1		1
#define MAGIC_SYNC_LENGTH2		2
#define MAGIC_IGNORE_CHECKSUMS		3
#define MAGIC_IGNORE_TRACK_MISMATCH	4
#define MAGIC_IGNORE_DATA_ID		5
#define MAGIC_MATCH_SIMPLE		6
#define MAGIC_MATCH_SIMPLE_FIXUP	7
#define MAGIC_POSTCOMP_SIMPLE		8
#define MAGIC_POSTCOMP_SIMPLE_ADJUST	9
#define MAGIC_PROLOG_LENGTH		10
#define MAGIC_EPILOG_LENGTH		11
#define MAGIC_FILL_LENGTH		12
#define MAGIC_FILL_VALUE		13
#define MAGIC_PRECOMP			14
#define MAGIC_SECTORS			15
#define MAGIC_HEADER_ID			16
#define MAGIC_DATA_ID			17
#define MAGIC_FLIP_TRACK_ID		18
#define MAGIC_SIDE_OFFSET		19
#define MAGIC_BOUNDS_OLD		20
#define MAGIC_BOUNDS_NEW		21



/****************************************************************************
 * gcr_v9000_set_defaults
 ****************************************************************************/
static void
gcr_v9000_set_defaults(
	union format			*fmt)

	{
	const static struct gcr_v9000	gcr_v9 =
		{
		.rd =
			{
			.sync_length1           = 60,
			.sync_length2           = 50,
			.postcomp_simple_adjust = { },
			.flags                  = 0
			},
		.wr =
			{
			.prolog_length = 0,
			.epilog_length = 64,
			.prolog_value  = 0x55,
			.epilog_value  = 0x55,
			.sync_length1  = 60,
			.sync_length2  = 50,
			.fill_length   = 8,
			.fill_value    = 0x55,
			.precomp       = { }
			},
		.rw =
			{
			.sectors     = 21,
			.header_id   = 0x07,
			.data_id     = 0x08,
			.flags       = 0,
			.side_offset = 0,
			.bnd         =
				{
				BOUNDS_NEW(0x0800, 0x1200, 0x1c00, 0),
				BOUNDS_NEW(0x1d00, 0x2500, 0x2f00, 1),
				BOUNDS_NEW(0x3000, 0x3800, 0x5000, 2)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->gcr_v9 = gcr_v9;
	}



/****************************************************************************
 * gcr_v9000_set_read_option
 ****************************************************************************/
static int
gcr_v9000_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SYNC_LENGTH1)          return (setvalue_ushort(&fmt->gcr_v9.rd.sync_length1, val, 10, 0x400));
	if (magic == MAGIC_SYNC_LENGTH2)          return (setvalue_ushort(&fmt->gcr_v9.rd.sync_length2, val, 10, 0x400));
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_IGNORE_TRACK_MISMATCH));
	if (magic == MAGIC_IGNORE_DATA_ID)        return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_IGNORE_DATA_ID));
	if (magic == MAGIC_MATCH_SIMPLE)          return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_MATCH_SIMPLE));
	if (magic == MAGIC_MATCH_SIMPLE_FIXUP)    return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_MATCH_SIMPLE_FIXUP));
	if (magic == MAGIC_POSTCOMP_SIMPLE)       return (setvalue_uchar_bit(&fmt->gcr_v9.rd.flags, val, FLAG_RD_POSTCOMP_SIMPLE));
	debug_error_condition(magic != MAGIC_POSTCOMP_SIMPLE_ADJUST);
	return (setvalue_short(&fmt->gcr_v9.rd.postcomp_simple_adjust[ofs], val, -0x0400, 0x0400));
	}



/****************************************************************************
 * gcr_v9000_set_write_option
 ****************************************************************************/
static int
gcr_v9000_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->gcr_v9.wr.prolog_length, val, 0, 0xffff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->gcr_v9.wr.epilog_length, val, 8, 0xffff));
	if (magic == MAGIC_SYNC_LENGTH1)  return (setvalue_ushort(&fmt->gcr_v9.wr.sync_length1, val, 10, 0x400));
	if (magic == MAGIC_SYNC_LENGTH2)  return (setvalue_ushort(&fmt->gcr_v9.wr.sync_length2, val, 10, 0x400));
	if (magic == MAGIC_FILL_LENGTH)   return (setvalue_uchar(&fmt->gcr_v9.wr.fill_length, val, 8, 0xff));
	if (magic == MAGIC_FILL_VALUE)    return (setvalue_uchar(&fmt->gcr_v9.wr.fill_value, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->gcr_v9.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * gcr_v9000_set_rw_option
 ****************************************************************************/
static int
gcr_v9000_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)       return (setvalue_uchar(&fmt->gcr_v9.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
	if (magic == MAGIC_HEADER_ID)     return (setvalue_uchar(&fmt->gcr_v9.rw.header_id, val, 0, 0xff));
	if (magic == MAGIC_DATA_ID)       return (setvalue_uchar(&fmt->gcr_v9.rw.data_id, val, 0, 0xff));
	if (magic == MAGIC_FLIP_TRACK_ID) return (setvalue_uchar_bit(&fmt->gcr_v9.rw.flags, val, FLAG_RW_FLIP_TRACK_ID));
	if (magic == MAGIC_SIDE_OFFSET)   return (setvalue_uchar(&fmt->gcr_v9.rw.side_offset, val, 0, GLOBAL_NR_TRACKS / 2));
	if (magic == MAGIC_BOUNDS_OLD)    return (setvalue_bounds_old(fmt->gcr_v9.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->gcr_v9.rw.bnd, val, ofs));
	}



/****************************************************************************
 * gcr_v9000_get_sector_size
 ****************************************************************************/
static int
gcr_v9000_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= fmt->gcr_v9.rw.sectors);
	if (sector < 0) return (512 * fmt->gcr_v9.rw.sectors);
	return (512);
	}



/****************************************************************************
 * gcr_v9000_get_sectors
 ****************************************************************************/
static int
gcr_v9000_get_sectors(
	union format			*fmt)

	{
	return (fmt->gcr_v9.rw.sectors);
	}



/****************************************************************************
 * gcr_v9000_get_flags
 ****************************************************************************/
static int
gcr_v9000_get_flags(
	union format			*fmt)

	{
	if (options_get_output()) return (FORMAT_FLAG_OUTPUT);
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * gcr_v9000_get_data_offset
 ****************************************************************************/
static int
gcr_v9000_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * gcr_v9000_get_data_size
 ****************************************************************************/
static int
gcr_v9000_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * gcr_v9000_read_options
 ****************************************************************************/
static struct format_option		gcr_v9000_read_options[] =
	{
	FORMAT_OPTION_INTEGER("sync_length1",           MAGIC_SYNC_LENGTH1,           1),
	FORMAT_OPTION_INTEGER("sync_length2",           MAGIC_SYNC_LENGTH2,           1),
	FORMAT_OPTION_BOOLEAN("ignore_checksums",       MAGIC_IGNORE_CHECKSUMS,       1),
	FORMAT_OPTION_BOOLEAN("ignore_track_mismatch",  MAGIC_IGNORE_TRACK_MISMATCH,  1),
	FORMAT_OPTION_BOOLEAN("ignore_data_id",         MAGIC_IGNORE_DATA_ID,         1),
	FORMAT_OPTION_BOOLEAN("match_simple",           MAGIC_MATCH_SIMPLE,           1),
	FORMAT_OPTION_BOOLEAN("match_simple_fixup",     MAGIC_MATCH_SIMPLE_FIXUP,     1),
	FORMAT_OPTION_BOOLEAN_COMPAT("postcomp",               MAGIC_POSTCOMP_SIMPLE,        1),
	FORMAT_OPTION_BOOLEAN("postcomp_simple",        MAGIC_POSTCOMP_SIMPLE,        1),
	FORMAT_OPTION_INTEGER("postcomp_simple_adjust", MAGIC_POSTCOMP_SIMPLE_ADJUST, 2),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_v9000_write_options
 ****************************************************************************/
static struct format_option		gcr_v9000_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("sync_length1",  MAGIC_SYNC_LENGTH1,  1),
	FORMAT_OPTION_INTEGER("sync_length2",  MAGIC_SYNC_LENGTH2,  1),
	FORMAT_OPTION_INTEGER("fill_length",   MAGIC_FILL_LENGTH,   1),
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       9),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_v9000_rw_options
 ****************************************************************************/
static struct format_option		gcr_v9000_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",           MAGIC_SECTORS,       1),
	FORMAT_OPTION_INTEGER("header_id",         MAGIC_HEADER_ID,     1),
	FORMAT_OPTION_INTEGER("data_id",           MAGIC_DATA_ID,       1),
	FORMAT_OPTION_BOOLEAN_OBSOLETE("flip_track_id",     MAGIC_FLIP_TRACK_ID, 1),
	FORMAT_OPTION_INTEGER_OBSOLETE("side_offset_v9000", MAGIC_SIDE_OFFSET,   1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",            MAGIC_BOUNDS_OLD,    9),
	FORMAT_OPTION_INTEGER("bounds_old",        MAGIC_BOUNDS_OLD,    9),
	FORMAT_OPTION_INTEGER("bounds_new",        MAGIC_BOUNDS_NEW,    9),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * gcr_v9000_format_desc
 ****************************************************************************/
struct format_desc			gcr_v9000_format_desc =
	{
	.name             = "gcr_v9000",
	.level            = 3,
	.set_defaults     = gcr_v9000_set_defaults,
	.set_read_option  = gcr_v9000_set_read_option,
	.set_write_option = gcr_v9000_set_write_option,
	.set_rw_option    = gcr_v9000_set_rw_option,
	.get_sectors      = gcr_v9000_get_sectors,
	.get_sector_size  = gcr_v9000_get_sector_size,
	.get_flags        = gcr_v9000_get_flags,
	.get_data_offset  = gcr_v9000_get_data_offset,
	.get_data_size    = gcr_v9000_get_data_size,
	.track_statistics = gcr_v9000_statistics,
	.track_read       = gcr_v9000_read_track,
	.track_write      = gcr_v9000_write_track,
	.fmt_opt_rd       = gcr_v9000_read_options,
	.fmt_opt_wr       = gcr_v9000_write_options,
	.fmt_opt_rw       = gcr_v9000_rw_options
	};
/******************************************************** Karsten Scheibler */

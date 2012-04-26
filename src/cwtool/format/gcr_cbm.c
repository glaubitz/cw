/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_cbm.c
 *
 ****************************************************************************
 *
 * sector header:
 * L1: 8 x 0x55, 5 x 0xff
 * L2: 0x08, checksum, sector, track, 0x30, 0x30, 0x0f, 0x0f
 *
 * sector data:
 * L1: 8 x 0x55, 5 x 0xff
 * L2: 0x07, 256 x data bytes, checksum, 0x00, 0x00
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "gcr_cbm.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
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




/*
 * while reading just ignore the last two bytes of sector and header data,
 * because we don't really need them. this will prevent sectors to be
 * classified as erroneous, which just have problems with the last two
 * bytes but are otherwise ok
 */

#define HEADER_SIZE			8
#define HEADER_READ_SIZE		(HEADER_SIZE - 2)
#define HEADER_WRITE_SIZE		HEADER_SIZE
#define DATA_SIZE			260
#define DATA_READ_SIZE			(DATA_SIZE - 2)
#define DATA_WRITE_SIZE			DATA_SIZE
#define FLAG_IGNORE_CHECKSUMS		(1 << 0)
#define FLAG_IGNORE_TRACK_MISMATCH	(1 << 1)
#define FLAG_IGNORE_DATA_ID		(1 << 2)
#define FLAG_MATCH_SIMPLE		(1 << 3)
#define FLAG_MATCH_SIMPLE_FIXUP		(1 << 4)
#define FLAG_POSTCOMP_SIMPLE		(1 << 5)



/****************************************************************************
 * gcr_cbm_checksum
 ****************************************************************************/
static int
gcr_cbm_checksum(
	unsigned char			*data,
	int				len)

	{
	int				c, i;

	for (c = i = 0; i < len; i++) c ^= data[i];
	return (c);
	}



/****************************************************************************
 * gcr_cbm_track_number
 ****************************************************************************/
static cw_count_t
gcr_cbm_track_number(
	struct gcr_cbm			*gcr_cbm,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	if (format_track == -1) return ((cwtool_track / gcr_cbm->rw.track_step) + 1);
	return (format_track);
	}



/****************************************************************************
 * gcr_cbm_read_sector2
 ****************************************************************************/
static int
gcr_cbm_read_sector2(
	struct fifo			*ffo_l1,
	struct gcr_cbm			*gcr_cbm,
	struct disk_error		*dsk_err,
	struct range_sector		*rng_sec,
	unsigned char			*header,
	unsigned char			*data)

	{
	int				bitofs;

	while (1)
		{
		*dsk_err = (struct disk_error) { };
		if (gcr_read_sync(ffo_l1, range_sector_header(rng_sec), gcr_cbm->rd.sync_length) == -1) return (-1);
		bitofs = fifo_get_rd_bitofs(ffo_l1);
		if (gcr_read_bytes(ffo_l1, dsk_err, header, HEADER_READ_SIZE) == -1) return (-1);
		range_set_end(range_sector_header(rng_sec), fifo_get_rd_bitofs(ffo_l1));
		fifo_set_rd_bitofs(ffo_l1, bitofs);
		if (format_compare2("header_id: got 0x%02x, expected 0x%02x", header[0], gcr_cbm->rw.header_id) == 0) break;
		}
	if (gcr_read_sync(ffo_l1, range_sector_data(rng_sec), gcr_cbm->rd.sync_length) == -1) return (-1);
	if (gcr_read_bytes(ffo_l1, dsk_err, data, DATA_READ_SIZE) == -1) return (-1);
	range_set_end(range_sector_data(rng_sec), fifo_get_rd_bitofs(ffo_l1));
	verbose_message(GENERIC, 2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (1);
	}



/****************************************************************************
 * gcr_cbm_write_sector2
 ****************************************************************************/
static int
gcr_cbm_write_sector2(
	struct fifo			*ffo_l1,
	struct gcr_cbm			*gcr_cbm,
	unsigned char			*header,
	unsigned char			*data)

	{
	if (gcr_write_fill(ffo_l1, gcr_cbm->wr.fill_value, gcr_cbm->wr.fill_length) == -1) return (-1);
	if (gcr_write_sync(ffo_l1, gcr_cbm->wr.sync_length) == -1) return (-1);
	if (gcr_write_bytes(ffo_l1, header, HEADER_WRITE_SIZE) == -1) return (-1);
	if (gcr_write_fill(ffo_l1, gcr_cbm->wr.fill_value, gcr_cbm->wr.fill_length) == -1) return (-1);
	if (gcr_write_sync(ffo_l1, gcr_cbm->wr.sync_length) == -1) return (-1);
	if (gcr_write_bytes(ffo_l1, data, DATA_WRITE_SIZE) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * gcr_cbm_read_sector
 ****************************************************************************/
static int
gcr_cbm_read_sector(
	struct fifo			*ffo_l1,
	struct gcr_cbm			*gcr_cbm,
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

	if (gcr_cbm_read_sector2(ffo_l1, gcr_cbm, &dsk_err, &rng_sec, header, data) == -1) return (-1);

	/* accept only valid sector numbers */

	sector = header[2];
	track  = gcr_cbm_track_number(gcr_cbm, cwtool_track, format_track, format_side);
	if (sector >= gcr_cbm->rw.sectors)
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	result = format_compare2("header xor checksum: got 0x%02x, expected 0x%02x", header[1], gcr_cbm_checksum(&header[2], HEADER_READ_SIZE - 2));
	result += format_compare2("data xor checksum: got 0x%02x, expected 0x%02x", data[257], gcr_cbm_checksum(&data[1], DATA_READ_SIZE - 2));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (gcr_cbm->rd.flags & FLAG_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", header[3], track);
	if (result > 0) verbose_message(GENERIC, 2, "track mismatch on sector %d", sector);
	if (gcr_cbm->rd.flags & FLAG_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare2("data_id: got 0x%02x, expected 0x%02x", data[0], gcr_cbm->rw.data_id);
	if (result > 0) verbose_message(GENERIC, 2, "wrong data_id on sector %d", sector);
	if (gcr_cbm->rd.flags & FLAG_IGNORE_DATA_ID) disk_warning_add(&dsk_err, result);
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
 * gcr_cbm_write_sector
 ****************************************************************************/
static int
gcr_cbm_write_sector(
	struct fifo			*ffo_l1,
	struct gcr_cbm			*gcr_cbm,
	struct disk_sector		*dsk_sct,
	unsigned char			*id,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			header[HEADER_SIZE];
	unsigned char			data[DATA_SIZE];
	int				sector = disk_get_sector_number(dsk_sct);

	verbose_message(GENERIC, 1, "writing sector %d with id 0x%02x 0x%02x", sector, id[0], id[1]);
	header[0] = gcr_cbm->rw.header_id;
	header[2] = sector;
	header[3] = gcr_cbm_track_number(gcr_cbm, cwtool_track, format_track, format_side);
	header[4] = id[0];
	header[5] = id[1];
	header[6] = 0x0f;
	header[7] = 0x0f;
	header[1] = gcr_cbm_checksum(&header[2], 6);
	data[0]   = gcr_cbm->rw.data_id;
	disk_sector_write(&data[1], dsk_sct);
	data[257] = gcr_cbm_checksum(&data[1], 256);
	data[258] = 0;
	data[259] = 0;
	return (gcr_cbm_write_sector2(ffo_l1, gcr_cbm, header, data));
	}



/****************************************************************************
 * gcr_cbm_statistics
 ****************************************************************************/
static int
gcr_cbm_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(
		ffo_l0,
		cwtool_track,
		gcr_cbm_track_number(&fmt->gcr_cbm, cwtool_track, format_track, format_side),
		-1);
	if (fmt->gcr_cbm.rd.flags & FLAG_POSTCOMP_SIMPLE) histogram_postcomp_simple(
		ffo_l0,
		fmt->gcr_cbm.rw.bnd,
		3,
		cwtool_track,
		gcr_cbm_track_number(&fmt->gcr_cbm, cwtool_track, format_track, format_side),
		-1);
	return (1);
	}



/****************************************************************************
 * gcr_cbm_read_track2
 ****************************************************************************/
static void
gcr_cbm_read_track2(
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

	if (fmt->gcr_cbm.rd.flags & FLAG_POSTCOMP_SIMPLE) postcomp_simple(ffo_l0, fmt->gcr_cbm.rw.bnd, 3);
	bitstream_read(ffo_l0, &ffo_l1, fmt->gcr_cbm.rw.bnd, 3);
	while (gcr_cbm_read_sector(&ffo_l1, &fmt->gcr_cbm, con, dsk_sct, cwtool_track, format_track, format_side) != -1) ;
	}



/****************************************************************************
 * gcr_cbm_read_track
 ****************************************************************************/
static int
gcr_cbm_read_track(
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
		.bnd          = fmt->gcr_cbm.rw.bnd,
		.bnd_size     = 3,
		.callback     = gcr_cbm_read_track2,
		.merge_two    = fmt->gcr_cbm.rd.flags & FLAG_MATCH_SIMPLE,
		.merge_all    = fmt->gcr_cbm.rd.flags & FLAG_MATCH_SIMPLE,
		.fixup        = fmt->gcr_cbm.rd.flags & FLAG_MATCH_SIMPLE_FIXUP
		};

	if ((fmt->gcr_cbm.rd.flags & FLAG_MATCH_SIMPLE) || (options_get_output())) match_simple(&mat_sim_nfo);
	else gcr_cbm_read_track2(fmt, NULL, ffo_l0, ffo_l3, dsk_sct, cwtool_track, format_track, format_side);
	return (1);
	}



/****************************************************************************
 * gcr_cbm_write_track
 ****************************************************************************/
static int
gcr_cbm_write_track(
	union format			*fmt,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	unsigned char			*data,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	unsigned char			id[2] = { 0x30, 0x30 };
	unsigned char			data_l1[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1 = FIFO_INIT(data_l1, sizeof (data_l1));
	int				i;

	if (data != NULL) id[0] = data[0], id[1] = data[1];
	if (gcr_write_fill(&ffo_l1, fmt->gcr_cbm.wr.prolog_value, fmt->gcr_cbm.wr.prolog_length) == -1) return (0);
	for (i = 0; i < fmt->gcr_cbm.rw.sectors; i++) if (gcr_cbm_write_sector(&ffo_l1, &fmt->gcr_cbm, &dsk_sct[i], id, cwtool_track, format_track, format_side) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (gcr_write_fill(&ffo_l1, fmt->gcr_cbm.wr.epilog_value, fmt->gcr_cbm.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (bitstream_write(&ffo_l1, ffo_l0, fmt->gcr_cbm.rw.bnd, fmt->gcr_cbm.wr.precomp, 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_SYNC_LENGTH		1
#define MAGIC_IGNORE_CHECKSUMS		2
#define MAGIC_IGNORE_TRACK_MISMATCH	3
#define MAGIC_IGNORE_DATA_ID		4
#define MAGIC_MATCH_SIMPLE		5
#define MAGIC_MATCH_SIMPLE_FIXUP	6
#define MAGIC_POSTCOMP_SIMPLE		7
#define MAGIC_PROLOG_LENGTH		8
#define MAGIC_EPILOG_LENGTH		9
#define MAGIC_FILL_LENGTH		10
#define MAGIC_FILL_VALUE		11
#define MAGIC_PRECOMP			12
#define MAGIC_SECTORS			13
#define MAGIC_HEADER_ID			14
#define MAGIC_DATA_ID			15
#define MAGIC_TRACK_STEP		16
#define MAGIC_BOUNDS_OLD		17
#define MAGIC_BOUNDS_NEW		18



/****************************************************************************
 * gcr_cbm_set_defaults
 ****************************************************************************/
static void
gcr_cbm_set_defaults(
	union format			*fmt)

	{
	const static struct gcr_cbm	gcr_cbm =
		{
		.rd =
			{
			.sync_length = 40,
			.flags       = 0
			},
		.wr =
			{
			.prolog_length = 0,
			.epilog_length = 64,
			.prolog_value  = 0x55,
			.epilog_value  = 0x55,
			.sync_length   = 40,
			.fill_length   = 8,
			.fill_value    = 0x55,
			.precomp       = { },
			.data_offset   = 0x165a2,
			.data_size     = 2
			},
		.rw =
			{
			.sectors    = 21,
			.header_id  = 0x08,
			.data_id    = 0x07,
			.track_step = 4,
			.bnd        =
				{
				BOUNDS_NEW(0x0f00, 0x1200, 0x1c00, 0),
				BOUNDS_NEW(0x1d00, 0x2500, 0x2f00, 1),
				BOUNDS_NEW(0x3000, 0x3800, 0x4800, 2)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->gcr_cbm = gcr_cbm;
	}



/****************************************************************************
 * gcr_cbm_set_read_option
 ****************************************************************************/
static int
gcr_cbm_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SYNC_LENGTH)           return (setvalue_ushort(&fmt->gcr_cbm.rd.sync_length, val, 9, 0x400));
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_IGNORE_TRACK_MISMATCH));
	if (magic == MAGIC_IGNORE_DATA_ID)        return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_IGNORE_DATA_ID));
	if (magic == MAGIC_MATCH_SIMPLE)          return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_MATCH_SIMPLE));
	if (magic == MAGIC_MATCH_SIMPLE_FIXUP)    return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_MATCH_SIMPLE_FIXUP));
	debug_error_condition(magic != MAGIC_POSTCOMP_SIMPLE);
	return (setvalue_uchar_bit(&fmt->gcr_cbm.rd.flags, val, FLAG_POSTCOMP_SIMPLE));
	}



/****************************************************************************
 * gcr_cbm_set_write_option
 ****************************************************************************/
static int
gcr_cbm_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->gcr_cbm.wr.prolog_length, val, 0, 0xffff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->gcr_cbm.wr.epilog_length, val, 8, 0xffff));
	if (magic == MAGIC_SYNC_LENGTH)   return (setvalue_ushort(&fmt->gcr_cbm.wr.sync_length, val, 9, 0x400));
	if (magic == MAGIC_FILL_LENGTH)   return (setvalue_uchar(&fmt->gcr_cbm.wr.fill_length, val, 8, 0xff));
	if (magic == MAGIC_FILL_VALUE)    return (setvalue_uchar(&fmt->gcr_cbm.wr.fill_value, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->gcr_cbm.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * gcr_cbm_set_rw_option
 ****************************************************************************/
static int
gcr_cbm_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)    return (setvalue_uchar(&fmt->gcr_cbm.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
	if (magic == MAGIC_HEADER_ID)  return (setvalue_uchar(&fmt->gcr_cbm.rw.header_id, val, 0, 0xff));
	if (magic == MAGIC_DATA_ID)    return (setvalue_uchar(&fmt->gcr_cbm.rw.data_id, val, 0, 0xff));
	if (magic == MAGIC_TRACK_STEP) return (setvalue_uchar(&fmt->gcr_cbm.rw.track_step, val, 1, 4));
	if (magic == MAGIC_BOUNDS_OLD) return (setvalue_bounds_old(fmt->gcr_cbm.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->gcr_cbm.rw.bnd, val, ofs));
	}



/****************************************************************************
 * gcr_cbm_get_sector_size
 ****************************************************************************/
static int
gcr_cbm_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= fmt->gcr_cbm.rw.sectors);
	if (sector < 0) return (256 * fmt->gcr_cbm.rw.sectors);
	return (256);
	}



/****************************************************************************
 * gcr_cbm_get_sectors
 ****************************************************************************/
static int
gcr_cbm_get_sectors(
	union format			*fmt)

	{
	return (fmt->gcr_cbm.rw.sectors);
	}



/****************************************************************************
 * gcr_cbm_get_flags
 ****************************************************************************/
static int
gcr_cbm_get_flags(
	union format			*fmt)

	{
	if (options_get_output()) return (FORMAT_FLAG_OUTPUT);
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * gcr_cbm_get_data_offset
 ****************************************************************************/
static int
gcr_cbm_get_data_offset(
	union format			*fmt)

	{
	return (fmt->gcr_cbm.wr.data_offset);
	}



/****************************************************************************
 * gcr_cbm_get_data_size
 ****************************************************************************/
static int
gcr_cbm_get_data_size(
	union format			*fmt)

	{
	return (fmt->gcr_cbm.wr.data_size);
	}



/****************************************************************************
 * gcr_cbm_read_options
 ****************************************************************************/
static struct format_option		gcr_cbm_read_options[] =
	{
	FORMAT_OPTION_INTEGER("sync_length",           MAGIC_SYNC_LENGTH,           1),
	FORMAT_OPTION_BOOLEAN("ignore_checksums",      MAGIC_IGNORE_CHECKSUMS,      1),
	FORMAT_OPTION_BOOLEAN("ignore_track_mismatch", MAGIC_IGNORE_TRACK_MISMATCH, 1),
	FORMAT_OPTION_BOOLEAN("ignore_data_id",        MAGIC_IGNORE_DATA_ID,        1),
	FORMAT_OPTION_BOOLEAN("match_simple",          MAGIC_MATCH_SIMPLE,          1),
	FORMAT_OPTION_BOOLEAN("match_simple_fixup",    MAGIC_MATCH_SIMPLE_FIXUP,    1),
	FORMAT_OPTION_BOOLEAN_COMPAT("postcomp",              MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_BOOLEAN("postcomp_simple",       MAGIC_POSTCOMP_SIMPLE,       1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_cbm_write_options
 ****************************************************************************/
static struct format_option		gcr_cbm_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION_INTEGER("sync_length",   MAGIC_SYNC_LENGTH,   1),
	FORMAT_OPTION_INTEGER("fill_length",   MAGIC_FILL_LENGTH,   1),
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       9),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * gcr_cbm_rw_options
 ****************************************************************************/
static struct format_option		gcr_cbm_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",      MAGIC_SECTORS,      1),
	FORMAT_OPTION_INTEGER("header_id",    MAGIC_HEADER_ID,    1),
	FORMAT_OPTION_INTEGER("data_id",      MAGIC_DATA_ID,      1),
	FORMAT_OPTION_INTEGER_OBSOLETE("track_step",   MAGIC_TRACK_STEP,   1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",       MAGIC_BOUNDS_OLD,   9),
	FORMAT_OPTION_INTEGER("bounds_old",   MAGIC_BOUNDS_OLD,   9),
	FORMAT_OPTION_INTEGER("bounds_new",   MAGIC_BOUNDS_NEW,   9),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * gcr_cbm_format_desc
 ****************************************************************************/
struct format_desc			gcr_cbm_format_desc =
	{
	.name             = "gcr_cbm",
	.level            = 3,
	.set_defaults     = gcr_cbm_set_defaults,
	.set_read_option  = gcr_cbm_set_read_option,
	.set_write_option = gcr_cbm_set_write_option,
	.set_rw_option    = gcr_cbm_set_rw_option,
	.get_sectors      = gcr_cbm_get_sectors,
	.get_sector_size  = gcr_cbm_get_sector_size,
	.get_flags        = gcr_cbm_get_flags,
	.get_data_offset  = gcr_cbm_get_data_offset,
	.get_data_size    = gcr_cbm_get_data_size,
	.track_statistics = gcr_cbm_statistics,
	.track_read       = gcr_cbm_read_track,
	.track_write      = gcr_cbm_write_track,
	.fmt_opt_rd       = gcr_cbm_read_options,
	.fmt_opt_wr       = gcr_cbm_write_options,
	.fmt_opt_rw       = gcr_cbm_rw_options
	};
/******************************************************** Karsten Scheibler */

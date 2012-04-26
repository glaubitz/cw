/****************************************************************************
 ****************************************************************************
 *
 * format/tbe_cw.c
 *
 ****************************************************************************
 *
 * - EXPERIMENTAL, currently not used by the default config
 * - TBE = two bit encoding
 * - every bit pair is encoded with one catweasel counter value, so for one
 *   byte exactly 4 such values are needed, MFM needs in worst case 8 values,
 *   so with TBE we get a more compact representation in catweasel memory
 * - we need at least 4 distinguishable counter values (in fact 6 are used,
 *   4 for the bit pairs, one to signal sync and one to fill the track, they
 *   are called tokens here and numbered from 0 - 5)
 * - with this encoding we do not have a fixed sector length, to guarantee
 *   a maximum length we swap bit pairs, so the most occuring gets small
 *   counter values and the least occuring large counter values, so the
 *   worst case is an identically distribution of the bit pairs, the sector
 *   length is then calculated as average of the 4 counter values
 * - we store the bit swap vector in one byte, so we can unshuffle the data
 *   when reading the data back
 * - sector layout: 1 (counters) FILL (token 5)
 *                  4 (counters) SYNC (token 4)
 *                  2 (bytes)    crc16
 *                  1 (bytes)    format id
 *                  1 (bytes)    bit pair swap vector
 *                  1 (bytes)    track
 *                  1 (bytes)    sector
 *                  2 (bytes)    sector size
 *                  x (bytes)    data bytes (with swapped bit pairs)
 * - this encoding may use the floppy drive and floppy disk out of normal
 *   specifications, so errors may occur on otherwise error free floppy
 *   disks
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "tbe_cw.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "tbe.h"
#include "bitstream.h"
#include "container.h"
#include "histogram.h"
#include "setvalue.h"




/****************************************************************************
 *
 * low level functions
 *
 ****************************************************************************/




/****************************************************************************
 * tbe_read_sync
 ****************************************************************************/
static int
tbe_read_sync(
	struct fifo			*ffo_l0,
	int				*lookup,
	int				size)

	{
	int				i, token;

	for (i = 0; i < size; i++)
		{
		token = bitstream_read_counter(ffo_l0, lookup);
		if (token == -1) return (-1);
		if (token == 4)
			{
			if (i == 0) verbose_message(GENERIC, 2, "got first sync at offset %d", fifo_get_rd_ofs(ffo_l0) - 1);
			continue;
			}
		i = -1;
		}
	verbose_message(GENERIC, 2, "got sync at offset %d with %d counter values", fifo_get_rd_ofs(ffo_l0) - i, i);
	return (1);
	}



/****************************************************************************
 * tbe_write_sync
 ****************************************************************************/
static int
tbe_write_sync(
	struct fifo			*ffo_l0,
	struct bitstream_counter	*bst_cnt,
	int				size)

	{
	verbose_message(GENERIC, 2, "writing sync at offset %d with %d counter values", fifo_get_wr_ofs(ffo_l0), size);
	while (size-- > 0) if (bitstream_write_counter(ffo_l0, bst_cnt, 4) == -1) return (-1);
	return (0);
	}



/****************************************************************************
 * tbe_write_fill
 ****************************************************************************/
static int
tbe_write_fill(
	struct fifo			*ffo_l0,
	struct bitstream_counter	*bst_cnt,
	int				size)

	{
	verbose_message(GENERIC, 2, "writing fill at offset %d", fifo_get_wr_ofs(ffo_l0));
	while (size-- > 0) if (bitstream_write_counter(ffo_l0, bst_cnt, 5) == -1) return (-1);
	return (0);
	}



/****************************************************************************
 * tbe_read_2bits
 ****************************************************************************/
static int
tbe_read_2bits(
	struct fifo			*ffo_l0,
	struct disk_error		*dsk_err,
	int				*lookup,
	int				ofs,
	int				val)

	{
	int				token;

	if (val == -1) return (-1);
	token = bitstream_read_counter(ffo_l0, lookup);
	if (token == -1) return (-1);
	if ((token < 0) || (token > 3))
		{
		verbose_message(GENERIC, 3, "wrong counter value at offset %d (byte %d)", fifo_get_rd_ofs(ffo_l0) - 1, ofs);
		disk_error_add(dsk_err, DISK_ERROR_FLAG_ENCODING, 1);
		token = 0;
		}
	return ((val << 2) | token);
	}



/****************************************************************************
 * tbe_read_bytes
 ****************************************************************************/
static int
tbe_read_bytes(
	struct fifo			*ffo_l0,
	struct disk_error		*dsk_err,
	int				*lookup,
	unsigned char			*data,
	int				size)

	{
	int				i, val;
	int				ofs = fifo_get_rd_ofs(ffo_l0);

	for (i = 0; i < size; i++)
		{
		val = tbe_read_2bits(ffo_l0, dsk_err, lookup, i, 0);
		val = tbe_read_2bits(ffo_l0, dsk_err, lookup, i, val);
		val = tbe_read_2bits(ffo_l0, dsk_err, lookup, i, val);
		val = tbe_read_2bits(ffo_l0, dsk_err, lookup, i, val);
		if (val == -1) return (-1);
		data[i] = val;
		}
	verbose_message(GENERIC, 2, "read %d bytes at offset %d", i, ofs);
	return (0);
	}


/****************************************************************************
 * tbe_write_bytes
 ****************************************************************************/
static int
tbe_write_bytes(
	struct fifo			*ffo_l0,
	struct bitstream_counter	*bst_cnt,
	unsigned char			*data,
	int				size)

	{
	int				i, val;

	verbose_message(GENERIC, 2, "writing %d bytes at offset %d", size, fifo_get_wr_ofs(ffo_l0));
	for (i = 0; i < size; i++)
		{
		val = data[i];
		if (bitstream_write_counter(ffo_l0, bst_cnt, val >> 6)       == -1) return (-1);
		if (bitstream_write_counter(ffo_l0, bst_cnt, (val >> 4) & 3) == -1) return (-1);
		if (bitstream_write_counter(ffo_l0, bst_cnt, (val >> 2) & 3) == -1) return (-1);
		if (bitstream_write_counter(ffo_l0, bst_cnt, val & 3)        == -1) return (-1);
		}
	return (0);
	}




/****************************************************************************
 *
 * functions for sector and track handling
 *
 ****************************************************************************/




#define HEADER_SIZE			8
#define DATA_SIZE			32768
#define FLAG_IGNORE_SECTOR_SIZE		(1 << 0)
#define FLAG_IGNORE_CHECKSUMS		(1 << 1)
#define FLAG_IGNORE_TRACK_MISMATCH	(1 << 2)
#define FLAG_IGNORE_FORMAT_ID		(1 << 3)



/****************************************************************************
 * tbe_cw_sector_size
 ****************************************************************************/
static int
tbe_cw_sector_size(
	struct tbe_cw			*tbe_cw,
	int				sector)

	{
	if (sector == 0) return (tbe_cw->rw.sector0_size);
	return (tbe_cw->rw.sector_size);
	}



/****************************************************************************
 * tbe_cw_bitswap
 ****************************************************************************/
static int
tbe_cw_bitswap(
	unsigned char			*data,
	int				size,
	int				s0,
	int				s1,
	int				s2,
	int				s3)

	{
	int				i, val, val2;
	int				swap[] = { s0, s1, s2, s3 };

	/* check validity of bit pair swap vector */

	if ((s0 == s1) || (s0 == s2) || (s0 == s3) ||
		(s1 == s2) || (s1 == s3) || (s2 == s3)) return (-1);

	/* check if we really have to do something */

	if ((s0 == 0) && (s1 == 1) && (s2 == 2) && (s3 == 3)) return (0);

	/* swap bit pairs */

	for (i = 0; i < size; i++)
		{
		val = data[i];
		val2 = swap[val >> 6];
		val2 = (val2 << 2) | swap[(val >> 4) & 3];
		val2 = (val2 << 2) | swap[(val >> 2) & 3];
		val2 = (val2 << 2) | swap[val & 3];
		data[i] = val2;
		}
	return (0);
	}



/****************************************************************************
 * tbe_cw_swap
 ****************************************************************************/
static void
tbe_cw_swap(
	int				*count,
	int				*swap,
	int				i,
	int				j)

	{
	int				t1 = count[i], t2 = swap[i];

	count[i] = count[j];
	count[j] = t1;
	swap[i]  = swap[j];
	swap[j]  = t2;
	}



/****************************************************************************
 * tbe_cw_calculate_bitswap
 ****************************************************************************/
static int
tbe_cw_calculate_bitswap(
	unsigned char			*data,
	int				size)

	{
	int				i, val, count[4] = { };
	int				swap1[4] = { 0, 1, 2, 3 }, swap2[4];

	/* count occurancies of bit pairs */

	for (i = 0; i < size; i++)
		{
		val = data[i];
		count[val >> 6]++;
		count[(val >> 4) & 3]++;
		count[(val >> 2) & 3]++;
		count[val & 3]++;
		}
	debug_message(GENERIC, 3, "count[4] = {%6d,%6d,%6d,%6d }", count[0], count[1], count[2], count[3]);

	/* swap bit pairs, so that the most occuring pair is 00 */

	if (count[0] < count[1]) tbe_cw_swap(count, swap1, 0, 1);
	if (count[2] < count[3]) tbe_cw_swap(count, swap1, 2, 3);
	if (count[0] < count[2]) tbe_cw_swap(count, swap1, 0, 2);
	if (count[1] < count[3]) tbe_cw_swap(count, swap1, 1, 3);
	if (count[1] < count[2]) tbe_cw_swap(count, swap1, 1, 2);
	i = tbe_cw_bitswap(data, size, swap1[0], swap1[1], swap1[2], swap1[3]);
	debug_message(GENERIC, 3, "swap1[4] = { %d, %d, %d, %d }", swap1[0], swap1[1], swap1[2], swap1[3]);
	debug_error_condition(i == -1);

	/* return information how to swap back */

	for (i = 0; i < 4; i++) swap2[swap1[i]] = i;
	return ((swap2[0] << 6) | (swap2[1] << 4) | (swap2[2] << 2) | swap2[3]);
	}



/****************************************************************************
 * tbe_cw_read_sector2
 ****************************************************************************/
static int
tbe_cw_read_sector2(
	struct fifo			*ffo_l0,
	struct tbe_cw			*tbe_cw,
	struct disk_error		*dsk_err,
	int				*lookup,
	unsigned char			*data)

	{
	int				ofs, size;

	*dsk_err = (struct disk_error) { };
	if (tbe_read_sync(ffo_l0, lookup, tbe_cw->rd.sync_length) == -1) return (-1);
	ofs = fifo_get_rd_ofs(ffo_l0);
	if (tbe_read_bytes(ffo_l0, dsk_err, lookup, data, HEADER_SIZE) == -1) return (-1);
	size = tbe_cw_sector_size(tbe_cw, data[5]);
	if (tbe_read_bytes(ffo_l0, dsk_err, lookup, &data[HEADER_SIZE], size) == -1) return (-1);
	verbose_message(GENERIC, 2, "rewinding to offset %d", ofs);
	fifo_set_rd_ofs(ffo_l0, ofs);
	return (1);
	}



/****************************************************************************
 * tbe_cw_write_sector2
 ****************************************************************************/
static int
tbe_cw_write_sector2(
	struct fifo			*ffo_l0,
	struct tbe_cw			*tbe_cw,
	struct bitstream_counter	*bst_cnt,
	unsigned char			*data,
	int				size)

	{
	if (tbe_write_fill(ffo_l0, bst_cnt, 1) == -1) return (-1);
	if (tbe_write_sync(ffo_l0, bst_cnt, tbe_cw->rd.sync_length) == -1) return (-1);
	if (tbe_write_bytes(ffo_l0, bst_cnt, data, size) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * tbe_cw_read_sector
 ****************************************************************************/
static int
tbe_cw_read_sector(
	struct fifo			*ffo_l0,
	struct tbe_cw			*tbe_cw,
	struct disk_sector		*dsk_sct,
	int				*lookup,
	int				track)

	{
	struct disk_error		dsk_err;
	unsigned char			data[HEADER_SIZE + DATA_SIZE];
	int				result, sector, size;

	if (tbe_cw_read_sector2(ffo_l0, tbe_cw, &dsk_err, lookup, data) == -1) return (-1);

	/* accept only valid sector numbers */

	sector = data[5];
	if (sector >= tbe_cw->rw.sectors)
		{
		verbose_message(GENERIC, 1, "sector %d out of range", sector);
		return (0);
		}
	verbose_message(GENERIC, 1, "got sector %d", sector);

	/* check sector quality */

	size = tbe_cw_sector_size(tbe_cw, sector);
	result = format_compare2("sector size: got %d, expected %d", tbe_read_u16_be(&data[6]), size);
	if (result > 0) verbose_message(GENERIC, 2, "wrong sector size on sector %d", sector);
	if (tbe_cw->rd.flags & FLAG_IGNORE_SECTOR_SIZE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_SIZE, result);

	result = format_compare2("crc16 checksum: got 0x%04x, expected 0x%04x", tbe_read_u16_be(data), tbe_crc16(tbe_cw->rw.crc16_init_value, &data[2], HEADER_SIZE - 2 + size));
	if (result > 0) verbose_message(GENERIC, 2, "checksum error on sector %d", sector);
	if (tbe_cw->rd.flags & FLAG_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", data[4], track);
	if (result > 0) verbose_message(GENERIC, 2, "track mismatch on sector %d", sector);
	if (tbe_cw->rd.flags & FLAG_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare2("format_id: got %d, expected %d", data[2], tbe_cw->rw.format_id);
	if (result > 0) verbose_message(GENERIC, 2, "wrong format_id on sector %d", sector);
	if (tbe_cw->rd.flags & FLAG_IGNORE_FORMAT_ID) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_ID, result);

	/* reverse swap of bit pairs */

	if (tbe_cw_bitswap(&data[HEADER_SIZE], size, data[3] >> 6,
		(data[3] >> 4) & 3, (data[3] >> 2) & 3, data[3] & 3) == -1)
		{
		verbose_message(GENERIC, 2, "invalid bit pair swap vector on sector %d", sector);
		disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, 1);
		}

	/*
	 * take the data if the found sector is of better quality than the
	 * current one
	 */

	disk_set_sector_number(&dsk_sct[sector], sector);
	disk_sector_read(&dsk_sct[sector], &dsk_err, &data[HEADER_SIZE]);
	return (1);
	}



/****************************************************************************
 * tbe_cw_write_sector
 ****************************************************************************/
static int
tbe_cw_write_sector(
	struct fifo			*ffo_l0,
	struct tbe_cw			*tbe_cw,
	struct disk_sector		*dsk_sct,
	struct bitstream_counter	*bst_cnt,
	int				track)

	{
	unsigned char			data[HEADER_SIZE + DATA_SIZE];
	int				sector = disk_get_sector_number(dsk_sct);
	int				size = tbe_cw_sector_size(tbe_cw, sector);

	verbose_message(GENERIC, 1, "writing sector %d", sector);
	disk_sector_write(&data[HEADER_SIZE], dsk_sct);
	data[2] = tbe_cw->rw.format_id;
	data[3] = tbe_cw_calculate_bitswap(&data[HEADER_SIZE], size);
	data[4] = track;
	data[5] = sector;
	tbe_write_u16_be(&data[6], size);
	tbe_write_u16_be(data, tbe_crc16(tbe_cw->rw.crc16_init_value, &data[2], HEADER_SIZE - 2 + size));
	return (tbe_cw_write_sector2(ffo_l0, tbe_cw, bst_cnt, data, HEADER_SIZE + size));
	}



/****************************************************************************
 * tbe_cw_statistics
 ****************************************************************************/
static int
tbe_cw_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal(ffo_l0, cwtool_track, cwtool_track, -1);
	return (1);
	}



/****************************************************************************
 * tbe_cw_read_track
 ****************************************************************************/
static int
tbe_cw_read_track(
	union format			*fmt,
	struct container		*con,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	int				lookup[128];

	bitstream_read_lookup(fmt->tbe_cw.rw.bnd, 6, lookup);
	while (tbe_cw_read_sector(ffo_l0, &fmt->tbe_cw, dsk_sct, lookup, cwtool_track) != -1) ;
	return (1);
	}



/****************************************************************************
 * tbe_cw_write_track
 ****************************************************************************/
static int
tbe_cw_write_track(
	union format			*fmt,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	unsigned char			*data,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	struct bitstream_counter	bst_cnt = BITSTREAM_COUNTER_INIT(fmt->tbe_cw.rw.bnd, fmt->tbe_cw.wr.precomp, 6);
	int				i;

	if (tbe_write_fill(ffo_l0, &bst_cnt, fmt->tbe_cw.wr.prolog_length) == -1) return (0);
	for (i = 0; i < fmt->tbe_cw.rw.sectors; i++) if (tbe_cw_write_sector(ffo_l0, &fmt->tbe_cw, &dsk_sct[i], &bst_cnt, cwtool_track) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (tbe_write_fill(ffo_l0, &bst_cnt, fmt->tbe_cw.wr.epilog_length) == -1) return (0);
	fifo_set_flags(ffo_l0, FIFO_FLAG_WRITABLE);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_SYNC_LENGTH		1
#define MAGIC_IGNORE_SECTOR_SIZE	2
#define MAGIC_IGNORE_CHECKSUMS		3
#define MAGIC_IGNORE_TRACK_MISMATCH	4
#define MAGIC_IGNORE_FORMAT_ID		5
#define MAGIC_PROLOG_LENGTH		6
#define MAGIC_EPILOG_LENGTH		7
#define MAGIC_PRECOMP			8
#define MAGIC_SECTORS			9
#define MAGIC_FORMAT_ID			10
#define MAGIC_CRC16_INIT_VALUE		11
#define MAGIC_SECTOR0_SIZE		12
#define MAGIC_SECTOR_SIZE		13
#define MAGIC_BOUNDS_OLD		14
#define MAGIC_BOUNDS_NEW		15



/****************************************************************************
 * tbe_cw_set_defaults
 ****************************************************************************/
static void
tbe_cw_set_defaults(
	union format			*fmt)

	{
	const static struct tbe_cw	tbe_cw =
		{
		.rd =
			{
			.sync_length = 4,
			.flags       = 0
			},
		.wr =
			{
			.prolog_length = 8,
			.epilog_length = 8,
			.precomp       = { }
			},
		.rw =
			{
			.sectors          = 14,
			.format_id        = 0x00,
			.crc16_init_value = 0xffff,
			.sector0_size     = 1024,
			.sector_size      = 1024,
			.bnd              =
				{
				BOUNDS_NEW(0x0800, 0x1800, 0x1e00, 0),
				BOUNDS_NEW(0x1f00, 0x2400, 0x2a00, 1),
				BOUNDS_NEW(0x2b00, 0x3000, 0x3600, 2),
				BOUNDS_NEW(0x3700, 0x3c00, 0x4600, 3),
				BOUNDS_NEW(0x4700, 0x4f00, 0x6300, 4),
				BOUNDS_NEW(0x6400, 0x7700, 0x7f00, 5)
				}
			}
		};

	debug_message(GENERIC, 2, "setting defaults");
	fmt->tbe_cw = tbe_cw;
	}



/****************************************************************************
 * tbe_cw_set_read_option
 ****************************************************************************/
static int
tbe_cw_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SYNC_LENGTH)           return (setvalue_uchar(&fmt->tbe_cw.rd.sync_length, val, 1, 0xff));
	if (magic == MAGIC_IGNORE_SECTOR_SIZE)    return (setvalue_uchar_bit(&fmt->tbe_cw.rd.flags, val, FLAG_IGNORE_SECTOR_SIZE));
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->tbe_cw.rd.flags, val, FLAG_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->tbe_cw.rd.flags, val, FLAG_IGNORE_TRACK_MISMATCH));
	debug_error_condition(magic != MAGIC_IGNORE_FORMAT_ID);
	return (setvalue_uchar_bit(&fmt->tbe_cw.rd.flags, val, FLAG_IGNORE_FORMAT_ID));
	}



/****************************************************************************
 * tbe_cw_set_write_option
 ****************************************************************************/
static int
tbe_cw_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->tbe_cw.wr.prolog_length, val, 4, 0xffff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->tbe_cw.wr.epilog_length, val, 4, 0xffff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->tbe_cw.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * tbe_cw_set_rw_option
 ****************************************************************************/
static int
tbe_cw_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_message(GENERIC, 2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)          return (setvalue_uchar(&fmt->tbe_cw.rw.sectors, val, 1, GLOBAL_NR_SECTORS));
	if (magic == MAGIC_FORMAT_ID)        return (setvalue_uchar(&fmt->tbe_cw.rw.format_id, val, 0, 0xff));
	if (magic == MAGIC_CRC16_INIT_VALUE) return (setvalue_ushort(&fmt->tbe_cw.rw.crc16_init_value, val, 0, 0xffff));
	if (magic == MAGIC_SECTOR0_SIZE)     return (setvalue_ushort(&fmt->tbe_cw.rw.sector0_size, val, 0x10, 0x4000));
	if (magic == MAGIC_SECTOR_SIZE)      return (setvalue_ushort(&fmt->tbe_cw.rw.sector_size, val, 0x10, 0x4000));
	if (magic == MAGIC_BOUNDS_OLD)       return (setvalue_bounds_old(fmt->tbe_cw.rw.bnd, val, ofs));
	debug_error_condition(magic != MAGIC_BOUNDS_NEW);
	return (setvalue_bounds_new(fmt->tbe_cw.rw.bnd, val, ofs));
	}



/****************************************************************************
 * tbe_cw_get_sector_size
 ****************************************************************************/
static int
tbe_cw_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	int				i, s;

	debug_error_condition(sector >= fmt->tbe_cw.rw.sectors);
	if (sector >= 0) return (tbe_cw_sector_size(&fmt->tbe_cw, sector));
	for (i = s = 0; i < fmt->tbe_cw.rw.sectors; i++) s += tbe_cw_sector_size(&fmt->tbe_cw, i);
	return (s);
	}



/****************************************************************************
 * tbe_cw_get_sectors
 ****************************************************************************/
static int
tbe_cw_get_sectors(
	union format			*fmt)

	{
	return (fmt->tbe_cw.rw.sectors);
	}



/****************************************************************************
 * tbe_cw_get_flags
 ****************************************************************************/
static int
tbe_cw_get_flags(
	union format			*fmt)

	{
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * tbe_cw_get_data_offset
 ****************************************************************************/
static int
tbe_cw_get_data_offset(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * tbe_cw_get_data_size
 ****************************************************************************/
static int
tbe_cw_get_data_size(
	union format			*fmt)

	{
	return (-1);
	}



/****************************************************************************
 * tbe_cw_read_options
 ****************************************************************************/
static struct format_option		tbe_cw_read_options[] =
	{
	FORMAT_OPTION_INTEGER("sync_length",           MAGIC_SYNC_LENGTH,           1),
	FORMAT_OPTION_BOOLEAN("ignore_sector_size",    MAGIC_IGNORE_SECTOR_SIZE,    1),
	FORMAT_OPTION_BOOLEAN("ignore_checksums",      MAGIC_IGNORE_CHECKSUMS,      1),
	FORMAT_OPTION_BOOLEAN("ignore_track_mismatch", MAGIC_IGNORE_TRACK_MISMATCH, 1),
	FORMAT_OPTION_BOOLEAN("ignore_format_id",      MAGIC_IGNORE_FORMAT_ID,      1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * tbe_cw_write_options
 ****************************************************************************/
static struct format_option		tbe_cw_write_options[] =
	{
	FORMAT_OPTION_INTEGER("prolog_length", MAGIC_PROLOG_LENGTH,  1),
	FORMAT_OPTION_INTEGER("epilog_length", MAGIC_EPILOG_LENGTH,  1),
	FORMAT_OPTION_INTEGER("precomp",       MAGIC_PRECOMP,       36),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * tbe_cw_rw_options
 ****************************************************************************/
static struct format_option		tbe_cw_rw_options[] =
	{
	FORMAT_OPTION_INTEGER("sectors",          MAGIC_SECTORS,           1),
	FORMAT_OPTION_INTEGER("format_id",        MAGIC_FORMAT_ID,         1),
	FORMAT_OPTION_INTEGER("crc16_init_value", MAGIC_CRC16_INIT_VALUE,  1),
	FORMAT_OPTION_INTEGER("sector0_size",     MAGIC_SECTOR0_SIZE,      1),
	FORMAT_OPTION_INTEGER("sector_size",      MAGIC_SECTOR_SIZE,       1),
	FORMAT_OPTION_INTEGER_COMPAT("bounds",           MAGIC_BOUNDS_OLD,       18),
	FORMAT_OPTION_INTEGER("bounds_old",       MAGIC_BOUNDS_OLD,       18),
	FORMAT_OPTION_INTEGER("bounds_new",       MAGIC_BOUNDS_NEW,       18),
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * tbe_cw_format_desc
 ****************************************************************************/
struct format_desc			tbe_cw_format_desc =
	{
	.name             = "tbe_cw",
	.level            = 3,
	.set_defaults     = tbe_cw_set_defaults,
	.set_read_option  = tbe_cw_set_read_option,
	.set_write_option = tbe_cw_set_write_option,
	.set_rw_option    = tbe_cw_set_rw_option,
	.get_sectors      = tbe_cw_get_sectors,
	.get_sector_size  = tbe_cw_get_sector_size,
	.get_flags        = tbe_cw_get_flags,
	.get_data_offset  = tbe_cw_get_data_offset,
	.get_data_size    = tbe_cw_get_data_size,
	.track_statistics = tbe_cw_statistics,
	.track_read       = tbe_cw_read_track,
	.track_write      = tbe_cw_write_track,
	.fmt_opt_rd       = tbe_cw_read_options,
	.fmt_opt_wr       = tbe_cw_write_options,
	.fmt_opt_rw       = tbe_cw_rw_options
	};
/******************************************************** Karsten Scheibler */

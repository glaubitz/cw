/****************************************************************************
 ****************************************************************************
 *
 * format/mfmfm.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "mfmfm.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../disk.h"
#include "../fifo.h"
#include "range.h"



/****************************************************************************
 * mfmfm_decode_table
 ****************************************************************************/
const int				mfmfm_decode_table[0x56] =
	{
	0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 
	0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 
	0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07, 
	0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07, 
	0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 
	0x00, 0x01, 0x00, 0x01, 0x02, 0x03, 0x02, 0x03, 
	0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07, 
	0x04, 0x05, 0x04, 0x05, 0x06, 0x07, 0x06, 0x07, 
	0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 
	0x08, 0x09, 0x08, 0x09, 0x0a, 0x0b, 0x0a, 0x0b, 
	0x0c, 0x0d, 0x0c, 0x0d, 0x0e, 0x0f
	};



/****************************************************************************
 * mfmfm_encode_table
 ****************************************************************************/
const int				mfmfm_encode_table[0x10] =
	{
	0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
	0x40, 0x41, 0x44, 0x45, 0x50, 0x51, 0x54, 0x55
	};



/****************************************************************************
 * mfmfm_read_sync
 ****************************************************************************/
int
mfmfm_read_sync(
	struct fifo			*ffo_l1,
	struct range			*rng,
	int				val,
	int				size)

	{
	int				i, j, bits;
	int				reg = fifo_read_bits(ffo_l1, 15);

	if (reg == -1) return (-1);
	for (j = 0; j < size; )
		{
		bits = fifo_read_bits(ffo_l1, 16);
		if (bits == -1) return (-1);
		if ((j > 0) && (bits == val))
			{
			j++;
			continue;
			}
		bits = reg = (reg << 16) | bits;
		for (i = j = 0; i < 16; i++, bits >>= 1)
			{
			if ((bits & 0xffff) != val) continue;
			fifo_set_rd_bitofs(ffo_l1, fifo_get_rd_bitofs(ffo_l1) - i);
			verbose_message(GENERIC, 3, "first sync found at bit offset %d with value 0x%04x", fifo_get_rd_bitofs(ffo_l1) - 16, val);
			j = 1;
			break;
			}
		}
	verbose_message(GENERIC, 2, "got %d sync(s) at bit offset %d with value 0x%04x", size, fifo_get_rd_bitofs(ffo_l1) - 16 * size, val);
	range_set_start(rng, fifo_get_rd_bitofs(ffo_l1) - 16 * size);
	return (j);
	}



/****************************************************************************
 * mfmfm_read_sync2
 ****************************************************************************/
int
mfmfm_read_sync2(
	struct fifo			*ffo_l1,
	struct range			*rng,
	int				val1,
	int				val2)

	{
	int				i, bits, val;
	int				reg = fifo_read_bits(ffo_l1, 15);

	if (reg == -1) return (-1);
	while (1)
		{
		bits = fifo_read_bits(ffo_l1, 16);
		if (bits == -1) return (-1);
		bits = reg = (reg << 16) | bits;
		for (i = 0; i < 16; i++, bits >>= 1) 
			{
			val = bits & 0xffff;
			if ((val == val1) || (val == val2)) goto found;
			}
		}
found:
	fifo_set_rd_bitofs(ffo_l1, fifo_get_rd_bitofs(ffo_l1) - i);
	verbose_message(GENERIC, 2, "got sync at bit offset %d with value 0x%04x", fifo_get_rd_bitofs(ffo_l1) - 16, val);
	range_set_start(rng, fifo_get_rd_bitofs(ffo_l1) - 16);
	return ((val == val1) ? 0 : 1);
	}



/****************************************************************************
 * mfmfm_write_sync
 ****************************************************************************/
int
mfmfm_write_sync(
	struct fifo			*ffo_l1,
	int				val,
	int				size)

	{
	verbose_message(GENERIC, 2, "writing sync at bit offset %d with value 0x%04x", fifo_get_wr_bitofs(ffo_l1), val);
	while (size-- > 0) if (fifo_write_bits(ffo_l1, val, 16) == -1) return (-1);
	return (0);
	}



/****************************************************************************
 * mfmfm_write_fill
 ****************************************************************************/
int
mfmfm_write_fill(
	struct fifo			*ffo_l1,
	int				val,
	int				size,
	int				(*write_func)(struct fifo *, int))

	{
	verbose_message(GENERIC, 2, "writing fill at bit offset %d with value 0x%02x", fifo_get_wr_bitofs(ffo_l1), val);
	while (size-- > 0) if (write_func(ffo_l1, val) == -1) return (-1);
	return (0);
	}



/****************************************************************************
 * mfmfm_read_bytes
 ****************************************************************************/
int
mfmfm_read_bytes(
	struct fifo			*ffo_l1,
	struct disk_error		*dsk_err,
	unsigned char			*data,
	int				size,
	int				(*read_func)(struct fifo *, struct disk_error *, int))

	{
	int				bitofs = fifo_get_rd_bitofs(ffo_l1);
	int				d, i;

	for (i = 0; i < size; i++)
		{
		d = read_func(ffo_l1, dsk_err, i);
		if (d == -1) return (-1);
		data[i] = d;
		}
	verbose_message(GENERIC, 2, "read %d bytes at bit offset %d with", i, bitofs);
	return (0);
	}



/****************************************************************************
 * mfmfm_write_bytes
 ****************************************************************************/
int
mfmfm_write_bytes(
	struct fifo			*ffo_l1,
	unsigned char			*data,
	int				size,
	int				(*write_func)(struct fifo *, int))

	{
	int				i;

	verbose_message(GENERIC, 2, "writing %d bytes at bit offset %d", size, fifo_get_wr_bitofs(ffo_l1));
	for (i = 0; i < size; i++) write_func(ffo_l1, data[i]);
	return (0);
	}



/****************************************************************************
 * mfmfm_get_sector_shift
 ****************************************************************************/
int
mfmfm_get_sector_shift(
	unsigned char			*pshift,
	int				sector,
	int				sectors)

	{
	int				i = sector / 2, j = 4 - 4 * (sector % 2);

	if ((sector < 0) || (sector >= sectors)) return (-1);
	return ((pshift[i] >> j) & 0x0f);
	}



/****************************************************************************
 * mfmfm_set_sector_shift
 ****************************************************************************/
static int
mfmfm_set_sector_shift(
	unsigned char			*pshift,
	int				sector,
	int				sectors,
	int				shift)

	{
	int				i = sector / 2, j = 4 * (sector % 2);
	int				k = 4 - j;

	debug_error_condition((sector < 0) || (sector >= sectors));
	if ((shift < 0) || (shift > 7)) return (0);
	pshift[i] = (((pshift[i] >> j) & 0x0f) << j) | (shift << k);
	return (1);
	}



/****************************************************************************
 * mfmfm_set_sector_size
 ****************************************************************************/
int
mfmfm_set_sector_size(
	unsigned char			*pshift,
	int				sector,
	int				sectors,
	int				size)

	{
	int				shift;

	if ((size & (size - 1)) != 0) return (0);
	for (shift = -8; size > 0; shift++) size >>= 1;
	return (mfmfm_set_sector_shift(pshift, sector, sectors, shift));
	}



/****************************************************************************
 * mfmfm_fill_sector_shift
 ****************************************************************************/
int
mfmfm_fill_sector_shift(
	unsigned char			*pshift,
	int				sector,
	int				sectors,
	int				shift)

	{
	int				s;

	for (s = sector; s < sectors; s++) if (! mfmfm_set_sector_shift(pshift, s, sectors, shift)) return (0);
	return (1);
	}
/******************************************************** Karsten Scheibler */

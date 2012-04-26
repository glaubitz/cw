/****************************************************************************
 ****************************************************************************
 *
 * format/mfm.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "mfm.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"



/****************************************************************************
 * mfm_read_8data_bits
 ****************************************************************************/
int
mfm_read_8data_bits(
	struct fifo			*ffo_l1,
	struct disk_error		*dsk_err,
	int				ofs)

	{
	int				last = fifo_last_bit_read(ffo_l1);
	int				clock, data = fifo_read_bits(ffo_l1, 16);

	if (data == -1) return (-1);
	clock = (last << 16) | data;
	clock |= (clock >> 2) & 0x5555;
	clock ^= clock << 1;
	if ((clock & 0xaaaa) != 0xaaaa)
		{
		verbose_message(GENERIC, 3, "wrong mfm clock bit around bit offset %d (byte %d)", fifo_get_rd_bitofs(ffo_l1), ofs);
		disk_error_add(dsk_err, DISK_ERROR_FLAG_ENCODING, 1);
		}
	return ((mfmfm_decode_table[(data >> 8) & 0x55] << 4) | mfmfm_decode_table[data & 0x55]);
	}



/****************************************************************************
 * mfm_write_8data_bits
 ****************************************************************************/
int
mfm_write_8data_bits(
	struct fifo			*ffo_l1,
	int				val)

	{
	int				data, clock;

	data  = (mfmfm_encode_table[val >> 4] << 8) | mfmfm_encode_table[val & 0x0f];
	clock = (fifo_last_bit_written(ffo_l1) << 15) | (data >> 1) | (data << 1);
	if (fifo_write_bits(ffo_l1, data ^ clock ^ 0xaaaa, 16) == -1) return (-1);
	return (0);
	}
/******************************************************** Karsten Scheibler */

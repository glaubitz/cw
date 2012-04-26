/****************************************************************************
 ****************************************************************************
 *
 * format/fm.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "fm.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"



/****************************************************************************
 * fm_read_8data_bits
 ****************************************************************************/
int
fm_read_8data_bits(
	struct fifo			*ffo_l1,
	struct disk_error		*dsk_err,
	int				ofs)

	{
	int				data = fifo_read_bits(ffo_l1, 16);

	if (data == -1) return (-1);
	if ((data & 0xaaaa) != 0xaaaa)
		{
		verbose_message(GENERIC, 3, "wrong fm clock bit around bit offset %d (byte %d)", fifo_get_rd_bitofs(ffo_l1), ofs);
		disk_error_add(dsk_err, DISK_ERROR_FLAG_ENCODING, 1);
		}
	return ((mfmfm_decode_table[(data >> 8) & 0x55] << 4) | mfmfm_decode_table[data & 0x55]);
	}



/****************************************************************************
 * fm_write_8data_bits
 ****************************************************************************/
int
fm_write_8data_bits(
	struct fifo			*ffo_l1,
	int				val)

	{
	int				data;

	data  = (mfmfm_encode_table[val >> 4] << 8) | mfmfm_encode_table[val & 0x0f];
	if (fifo_write_bits(ffo_l1, data | 0xaaaa, 16) == -1) return (-1);
	return (0);
	}
/******************************************************** Karsten Scheibler */

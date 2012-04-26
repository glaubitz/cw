/****************************************************************************
 ****************************************************************************
 *
 * format/bitstream.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "bitstream.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../fifo.h"
#include "bounds.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * bitstream_read_lookup2
 ****************************************************************************/
static int
bitstream_read_lookup2(
	struct bounds			*bnd,
	int				bnd_size,
	int				*lookup,
	int				*error)

	{
	int				i, j, l, h, w;

	/*
	 * find highest valid count increment, it by one and fill lookup
	 * with it (areas out of bounds will generate invalid bit patterns)
	 */

	for (i = j = 0; i < bnd_size; i++) if (bnd[i].count > j) j = bnd[i].count;
	for (i = 0, j++; i < GLOBAL_NR_PULSE_LENGTHS; i++)
		{
		lookup[i] = j;
		if (error != NULL) error[i] = 0xff;
		}

	/* fill lookup */

	for (i = 0; i < bnd_size; i++)
		{
		debug_error_condition((bnd[i].read_low > 0x7fff) || (bnd[i].read_high > 0x7fff));
		debug_error_condition(bnd[i].read_low > bnd[i].read_high);
		l = (bnd[i].read_low  + 0xff) >> 8;
		w = (bnd[i].write     + 0x80) >> 8;
		h = (bnd[i].read_high + 0xff) >> 8;
		for (j = l; (j <= h) && (j < GLOBAL_NR_PULSE_LENGTHS); j++)
			{
			lookup[j] = bnd[i].count;
			if (error == NULL) continue;
			if (j < w) error[j] = w - j;
			else error[j] = j - w;
			}
		}
	return (0);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * bitstream_read_lookup
 ****************************************************************************/
int
bitstream_read_lookup(
	struct bounds			*bnd,
	int				bnd_size,
	int				*lookup)

	{
	return (bitstream_read_lookup2(bnd, bnd_size, lookup, NULL));
	}



/****************************************************************************
 * bitstream_read_counter
 ****************************************************************************/
int
bitstream_read_counter(
	struct fifo			*ffo_l0,
	int				*lookup)

	{
	int				i = fifo_read_byte(ffo_l0);

	if (i == -1) return (-1);
	return (lookup[i & GLOBAL_PULSE_LENGTH_MASK]);
	}



/****************************************************************************
 * bitstream_write_counter
 ****************************************************************************/
int
bitstream_write_counter(
	struct fifo			*ffo_l0,
	struct bitstream_counter	*bst_cnt,
	int				i)

	{
	int				clip, val, precomp;

	if ((i < 0) || (i >= bst_cnt->bnd_size))
		{
		verbose_message(GENERIC, 3, "could not convert invalid bit pattern at offset %d", fifo_get_wr_ofs(ffo_l0));
		bst_cnt->invalid++;
		i = bst_cnt->bnd_size - 1;
		}
	bst_cnt->this += bst_cnt->bnd[i].write;
	if (bst_cnt->last > 0)
		{
		precomp       = bst_cnt->precomp[bst_cnt->bnd_size * bst_cnt->last_i + i];
		bst_cnt->last -= precomp;
		bst_cnt->this += precomp;
		clip = 0, val = bst_cnt->last;
		if (val < 0x0300) clip = 1, val = 0x0300;
		if (val > 0x7fff) clip = 1, val = 0x7fff;
		if (clip) error_warning("precompensation lead to invalid catweasel counter 0x%04x at offset %d", bst_cnt->last, fifo_get_wr_ofs(ffo_l0));
		if (fifo_write_byte(ffo_l0, val >> 8) == -1) return (-1);
		}
	bst_cnt->last   = bst_cnt->this;
	bst_cnt->this   &= 0xff;
	bst_cnt->last_i = i; 
	return (0);
	}



/****************************************************************************
 * bitstream_read
 ****************************************************************************/
int
bitstream_read(
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l1,
	struct bounds			*bnd,
	int				bnd_size)

	{
	int				i, lookup[GLOBAL_NR_PULSE_LENGTHS];

	/* create lookup table */

	bitstream_read_lookup(bnd, bnd_size, lookup);

	/* convert raw counter values to raw bits */

	debug_message(GENERIC, 3, "bitstream_read ffo_l0->wr_ofs = %d, ffo_l1->limit = %d", fifo_get_wr_ofs(ffo_l0), fifo_get_limit(ffo_l1));
	while (1)
		{
		i = bitstream_read_counter(ffo_l0, lookup);
		if (i == -1) break;
		if (fifo_write_count(ffo_l1, i) == -1) debug_error();
		}
	fifo_write_flush(ffo_l1);
	debug_message(GENERIC, 3, "bitstream_read ffo_l0->wr_ofs = %d, ffo_l1->wr_bitofs = %d", fifo_get_wr_ofs(ffo_l0), fifo_get_wr_bitofs(ffo_l1));
	return (0);
	}



/****************************************************************************
 * bitstream_read_map
 ****************************************************************************/
int
bitstream_read_map(
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l1,
	struct bounds			*bnd,
	int				bnd_size,
	struct bitstream_map		*bst_map,
	int				bst_map_size)

	{
	int				b, e, i, j, s;
	int				lookup[GLOBAL_NR_PULSE_LENGTHS];
	int				error[GLOBAL_NR_PULSE_LENGTHS];

	/* create lookup table */

	bitstream_read_lookup2(bnd, bnd_size, lookup, error);

	/* convert raw counter values to raw bits */

	debug_message(GENERIC, 3, "bitstream_read_map ffo_l0->wr_ofs = %d", fifo_get_wr_ofs(ffo_l0));
	if (ffo_l1 != NULL) debug_message(GENERIC, 3, "bitstream_read_map ffo_l1->limit = %d", fifo_get_limit(ffo_l1));
	for (j = 0, s = 0; j < bst_map_size; j++)
		{
		b = fifo_read_byte(ffo_l0);
		if (b == -1) break;
		e = error[b & GLOBAL_PULSE_LENGTH_MASK];
		i = lookup[b & GLOBAL_PULSE_LENGTH_MASK];
		s += i + 1;
		bst_map[j] = (struct bitstream_map)
			{
			.length     = i + 1,
			.length_sum = s,
			.error      = e
			};
		if (ffo_l1 != NULL)
			{
			if (fifo_write_count(ffo_l1, i) == -1) debug_error();
			}
		}
	debug_message(GENERIC, 3, "bitstream_read_map ffo_l0->wr_ofs = %d", fifo_get_wr_ofs(ffo_l0));
	if (ffo_l1 != NULL)
		{
		fifo_write_flush(ffo_l1);
		debug_message(GENERIC, 3, "bitstream_read_map ffo_l1->wr_bitofs = %d", fifo_get_wr_bitofs(ffo_l1));
		}
	return (j);
	}



/****************************************************************************
 * bitstream_write
 ****************************************************************************/
int
bitstream_write(
	struct fifo			*ffo_l1,
	struct fifo			*ffo_l0,
	struct bounds			*bnd,
	short				*precomp,
	int				bnd_size)

	{
	struct bitstream_counter	bst_cnt = BITSTREAM_COUNTER_INIT(bnd, precomp, bnd_size);
	int				i, j, lookup[GLOBAL_NR_PULSE_LENGTHS];

	/* create lookup table */

	for (i = 0; i < GLOBAL_NR_PULSE_LENGTHS; i++) lookup[i] = -1;
	for (i = 0; i < bnd_size; i++)
		{
		j = bnd[i].count;
		debug_error_condition(j >= 128);
		lookup[j] = i;
		}

	/* convert raw bits to raw counter values and do precompensation */

	debug_message(GENERIC, 3, "bitstream_write ffo_l1->wr_bitofs = %d, ffo_l0->limit = %d", fifo_get_wr_bitofs(ffo_l1), fifo_get_limit(ffo_l0));
	fifo_set_flags(ffo_l0, FIFO_FLAG_WRITABLE);
	while (1)
		{
		i = fifo_read_count(ffo_l1);
		if (i == -1) break;
		if (i >= 128) i = 127;
		if (bitstream_write_counter(ffo_l0, &bst_cnt, lookup[i]) == -1) return (-1);
		}
	if (bst_cnt.invalid > 0) error_warning("could not convert %d invalid bit patterns", bst_cnt.invalid);
	debug_message(GENERIC, 3, "bitstream_write ffo_l1->rd_bitofs = %d, ffo_l0->wr_ofs = %d", fifo_get_rd_bitofs(ffo_l1), fifo_get_wr_ofs(ffo_l0));

	return (0);
	}
/******************************************************** Karsten Scheibler */

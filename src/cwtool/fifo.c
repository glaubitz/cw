/****************************************************************************
 ****************************************************************************
 *
 * fifo.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <string.h>

#include "fifo.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"



/****************************************************************************
 * fifo_reset
 ****************************************************************************/
int
fifo_reset(
	struct fifo			*ffo)

	{
	*ffo = (struct fifo) { .data = ffo->data, .size = ffo->size, .limit = ffo->limit };
	return (1);
	}



/****************************************************************************
 * fifo_get_size
 ****************************************************************************/
int
fifo_get_size(
	struct fifo			*ffo)

	{
	return (ffo->size);
	}



/****************************************************************************
 * fifo_set_limit
 ****************************************************************************/
int
fifo_set_limit(
	struct fifo			*ffo,
	int				limit)

	{
	debug_error_condition((limit < 0) || (limit > ffo->size));
	ffo->limit = limit;
	return (0);
	}



/****************************************************************************
 * fifo_get_limit
 ****************************************************************************/
int
fifo_get_limit(
	struct fifo			*ffo)

	{
	return (ffo->limit);
	}



/****************************************************************************
 * fifo_get_data
 ****************************************************************************/
unsigned char *
fifo_get_data(
	struct fifo			*ffo)

	{
	return (ffo->data);
	}



/****************************************************************************
 * fifo_set_wr_ofs
 ****************************************************************************/
int
fifo_set_wr_ofs(
	struct fifo			*ffo,
	int				wr_ofs)

	{
	debug_error_condition((wr_ofs < 0) || (wr_ofs > ffo->limit));
	debug_error_condition(ffo->wr_bitofs & 7);
	ffo->wr_ofs    = wr_ofs;
	ffo->wr_bitofs = 8 * wr_ofs;
	return (0);
	}



/****************************************************************************
 * fifo_get_wr_ofs
 ****************************************************************************/
int
fifo_get_wr_ofs(
	struct fifo			*ffo)

	{
	return (ffo->wr_ofs);
	}



/****************************************************************************
 * fifo_get_wr_bitofs
 ****************************************************************************/
int
fifo_get_wr_bitofs(
	struct fifo			*ffo)

	{
	return (ffo->wr_bitofs);
	}



/****************************************************************************
 * fifo_set_rd_ofs
 ****************************************************************************/
int
fifo_set_rd_ofs(
	struct fifo			*ffo,
	int				rd_ofs)

	{
	debug_error_condition((rd_ofs < 0) || (rd_ofs > ffo->wr_ofs));
	debug_error_condition(ffo->rd_bitofs & 7);
	ffo->rd_ofs    = rd_ofs;
	ffo->rd_bitofs = 8 * rd_ofs;
	return (0);
	}



/****************************************************************************
 * fifo_set_rd_bitofs
 ****************************************************************************/
int
fifo_set_rd_bitofs(
	struct fifo			*ffo,
	int				rd_bitofs)

	{
	debug_error_condition((rd_bitofs < 0) || (rd_bitofs > ffo->wr_bitofs));
	ffo->rd_ofs    = (rd_bitofs + 7) / 8;
	ffo->rd_bitofs = rd_bitofs;
	if (ffo->rd_bitofs & 7) ffo->reg = ((int) ffo->data[ffo->rd_ofs - 1]) << (ffo->rd_bitofs & 7);
	return (0);
	}



/****************************************************************************
 * fifo_get_rd_ofs
 ****************************************************************************/
int
fifo_get_rd_ofs(
	struct fifo			*ffo)

	{
	return (ffo->rd_ofs);
	}



/****************************************************************************
 * fifo_get_rd_bitofs
 ****************************************************************************/
int
fifo_get_rd_bitofs(
	struct fifo			*ffo)

	{
	return (ffo->rd_bitofs);
	}



/****************************************************************************
 * fifo_last_bit_read
 ****************************************************************************/
int
fifo_last_bit_read(
	struct fifo			*ffo)

	{
	return ((ffo->reg & 0x100) ? 1 : 0);
	}



/****************************************************************************
 * fifo_last_bit_written
 ****************************************************************************/
int
fifo_last_bit_written(
	struct fifo			*ffo)

	{
	if (ffo->wr_bitofs == 0) return (1);
	return (ffo->reg & 1);
	}



/****************************************************************************
 * fifo_read_bits
 ****************************************************************************/
int
fifo_read_bits(
	struct fifo			*ffo,
	int				bits)

	{
	int				avail = 0, shift = ffo->rd_bitofs & 7;
	int				mask = (1 << bits) - 1;

	debug_error_condition(bits > 16);
	if (shift > 0) avail = 8 - shift;
	while (avail < bits)
		{
		if (ffo->rd_ofs >= ffo->wr_ofs) return (-1);
		ffo->reg <<= 8;
		ffo->reg |= ((int) ffo->data[ffo->rd_ofs++]) << shift;
		avail += 8;
		}
	shift = avail - bits - 8 + shift;
	if (shift < 0) ffo->reg <<= -shift;
	if (shift > 0) ffo->reg >>= shift;
	ffo->rd_bitofs += bits;
	if (ffo->rd_bitofs >= ffo->wr_bitofs) return (-1); 
	return ((ffo->reg >> 8) & mask);
	}



/****************************************************************************
 * fifo_write_bits
 ****************************************************************************/
int
fifo_write_bits(
	struct fifo			*ffo,
	int				val,
	int				bits)

	{
	int				avail  = bits + (ffo->wr_bitofs & 7);
	int				wr_ofs = ffo->wr_bitofs / 8;

	debug_error_condition(bits > 16);
	debug_error_condition((val < 0) || (val >= (1 << bits)));
	ffo->reg = (ffo->reg << bits) | val;
	while (avail > 7)
		{
		if (wr_ofs >= ffo->limit) return (-1);
		avail -= 8;
		ffo->data[wr_ofs++] = ffo->reg >> avail;
		}
	ffo->wr_bitofs += bits;
	ffo->wr_ofs = (ffo->wr_bitofs + 7) / 8;
	return (0);
	}



/****************************************************************************
 * fifo_read_count
 ****************************************************************************/
int
fifo_read_count(
	struct fifo			*ffo)

	{
	int				i;

	for (i = 0; ; i++)
		{
		if ((ffo->rd_bitofs++ & 7) == 0)
			{
			if (ffo->rd_ofs >= ffo->wr_ofs) break;
			ffo->reg |= (int) ffo->data[ffo->rd_ofs++];
			}
		if (ffo->rd_bitofs >= ffo->wr_bitofs) break; 
		ffo->reg <<= 1;
		if (ffo->reg & 0x100) return (i);
		}
	return (-1);
	}



/****************************************************************************
 * fifo_read_byte
 ****************************************************************************/
int
fifo_read_byte(
	struct fifo			*ffo)

	{
	if (ffo->rd_ofs >= ffo->wr_ofs) return (-1);
	debug_error_condition((ffo->rd_bitofs & 7) != 0);
	ffo->rd_bitofs += 8;
	return (ffo->data[ffo->rd_ofs++]);
	}



/****************************************************************************
 * fifo_write_byte
 ****************************************************************************/
int
fifo_write_byte(
	struct fifo			*ffo,
	int				byte)

	{
	if (ffo->wr_ofs >= ffo->limit) return (-1);
	debug_error_condition((ffo->wr_bitofs & 7) != 0);
	ffo->wr_bitofs += 8;
	ffo->data[ffo->wr_ofs++] = byte;
	return (0);
	}



/****************************************************************************
 * fifo_read_block
 ****************************************************************************/
int
fifo_read_block(
	struct fifo			*ffo,
	unsigned char			*data,
	int				size)

	{
	int				result = 0;

	if (ffo->rd_ofs >= ffo->wr_ofs) return (-1);
	debug_error_condition(size < 0);
	debug_error_condition(data == NULL);
	debug_error_condition((ffo->rd_bitofs & 7) != 0);
	if (ffo->rd_ofs + size > ffo->wr_ofs) result = -1, size = ffo->wr_ofs - ffo->rd_ofs;
	memcpy(data, &ffo->data[ffo->rd_ofs], size);
	ffo->rd_ofs += size;
	ffo->rd_bitofs += 8 * size;
	return (result);
	}



/****************************************************************************
 * fifo_write_block
 ****************************************************************************/
int
fifo_write_block(
	struct fifo			*ffo,
	unsigned char			*data,
	int				size)

	{
	int				result = 0;

	if (ffo->wr_ofs >= ffo->limit) return (-1);
	debug_error_condition(size < 0);
	debug_error_condition(data == NULL);
	debug_error_condition((ffo->wr_bitofs & 7) != 0);
	if (ffo->wr_ofs + size > ffo->limit) result = -1, size = ffo->limit - ffo->wr_ofs;
	memcpy(&ffo->data[ffo->wr_ofs], data, size);
	ffo->wr_ofs += size;
	ffo->wr_bitofs += 8 * size;
	return (result);
	}



/****************************************************************************
 * fifo_copy_block
 ****************************************************************************/
int
fifo_copy_block(
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	int				size)

	{
	int				result = 0;

	if (ffo_src->rd_ofs >= ffo_src->wr_ofs) return (-1);
	if (ffo_dst->wr_ofs >= ffo_dst->limit) return (-1);
	debug_error_condition(size < 0);
	debug_error_condition((ffo_src->rd_bitofs & 7) != 0);
	debug_error_condition((ffo_dst->wr_bitofs & 7) != 0);
	if (ffo_src->rd_ofs + size > ffo_src->wr_ofs) result = -1, size = ffo_src->wr_ofs - ffo_src->rd_ofs;
	if (ffo_dst->wr_ofs + size > ffo_dst->limit) result = -1, size = ffo_dst->limit - ffo_dst->wr_ofs;
	memcpy(&ffo_dst->data[ffo_dst->wr_ofs], &ffo_src->data[ffo_src->rd_ofs], size);
	ffo_src->rd_ofs += size;
	ffo_src->rd_bitofs += 8 * size;
	ffo_dst->wr_ofs += size;
	ffo_dst->wr_bitofs += 8 * size;
	return (result);
	}



/****************************************************************************
 * fifo_copy_bitblock
 ****************************************************************************/
int
fifo_copy_bitblock(
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	int				bits)

	{
	int				b, val;

	if ((bits & 7) + (ffo_src->rd_bitofs & 7) + (ffo_dst->wr_bitofs & 7) == 0) return (fifo_copy_block(ffo_src, ffo_dst, bits / 8));
	while (bits > 0)
		{
		b = (bits < 16) ? bits : 16;
		val = fifo_read_bits(ffo_src, b);
		if (val == -1) return (-1);
		if (fifo_write_bits(ffo_dst, val, b) == -1) return (-1);
		bits -= b;
		}
	return (0);
	}



/****************************************************************************
 * fifo_write_flush
 ****************************************************************************/
int
fifo_write_flush(
	struct fifo			*ffo)

	{
	if (ffo->wr_bitofs & 7) ffo->data[ffo->wr_ofs - 1] = ffo->reg << (8 - (ffo->wr_bitofs & 7));
	ffo->reg = 0;
	return (0);
	}



/****************************************************************************
 * fifo_set_flags
 ****************************************************************************/
int
fifo_set_flags(
	struct fifo			*ffo,
	int				flags)

	{
	ffo->flags |= flags;
	return (0);
	}



/****************************************************************************
 * fifo_clear_flags
 ****************************************************************************/
int
fifo_clear_flags(
	struct fifo			*ffo,
	int				flags)

	{
	ffo->flags &= ~flags;
	return (0);
	}



/****************************************************************************
 * fifo_get_flags
 ****************************************************************************/
int
fifo_get_flags(
	struct fifo			*ffo)

	{
	return (ffo->flags);
	}



/****************************************************************************
 * fifo_set_speed
 ****************************************************************************/
int
fifo_set_speed(
	struct fifo			*ffo,
	int				speed)

	{
	debug_error_condition((speed < 0) || (speed > 3));
	ffo->speed = speed;
	return (0);
	}



/****************************************************************************
 * fifo_get_speed
 ****************************************************************************/
int
fifo_get_speed(
	struct fifo			*ffo)

	{
	return (ffo->speed);
	}
/******************************************************** Karsten Scheibler */

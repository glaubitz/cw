/****************************************************************************
 ****************************************************************************
 *
 * fifo.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FIFO_H
#define CWTOOL_FIFO_H

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define FIFO_INIT(d, s)			(struct fifo) { .data = d, .size = s, .limit = s }

/* writable in the sense of "writable to a catweasel device" */

#define FIFO_FLAG_WRITABLE		(1 << 0)
#define FIFO_FLAG_INDEX_STORED		(1 << 1)
#define FIFO_FLAG_INDEX_ALIGNED		(1 << 2)

struct fifo
	{
	unsigned char			*data;
	int				size;
	int				limit;
	int				wr_ofs;
	int				wr_bitofs;
	int				rd_ofs;
	int				rd_bitofs;
	int				reg;
	int				flags;
	int				speed;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern int				fifo_reset(struct fifo *);
extern int				fifo_get_size(struct fifo *);
extern int				fifo_set_limit(struct fifo *, int);
extern int				fifo_get_limit(struct fifo *);
extern unsigned char			*fifo_get_data(struct fifo *);
extern int				fifo_set_wr_ofs(struct fifo *, int);
extern int				fifo_get_wr_ofs(struct fifo *);
extern int				fifo_get_wr_bitofs(struct fifo *);
extern int				fifo_set_rd_ofs(struct fifo *, int);
extern int				fifo_set_rd_bitofs(struct fifo *, int);
extern int				fifo_get_rd_ofs(struct fifo *);
extern int				fifo_get_rd_bitofs(struct fifo *);
extern int				fifo_last_bit_read(struct fifo *);
extern int				fifo_last_bit_written(struct fifo *);
extern int				fifo_read_bits(struct fifo *, int);
extern int				fifo_write_bits(struct fifo *, int, int);
extern int				fifo_read_count(struct fifo *);
extern int				fifo_read_byte(struct fifo *);
extern int				fifo_write_byte(struct fifo *, int);
extern int				fifo_read_block(struct fifo *, unsigned char *, int);
extern int				fifo_write_block(struct fifo *, unsigned char *, int);
extern int				fifo_copy_block(struct fifo *, struct fifo *, int);
extern int				fifo_copy_bitblock(struct fifo *, struct fifo *, int);
extern int				fifo_write_flush(struct fifo *);
extern int				fifo_set_flags(struct fifo *, int);
extern int				fifo_clear_flags(struct fifo *, int);
extern int				fifo_get_flags(struct fifo *);
extern int				fifo_set_speed(struct fifo *, int);
extern int				fifo_get_speed(struct fifo *);

#define fifo_write_count(ffo, count)	fifo_write_bits(ffo, 1, count + 1)



#endif /* !CWTOOL_FIFO_H */
/******************************************************** Karsten Scheibler */

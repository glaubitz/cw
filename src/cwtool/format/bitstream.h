/****************************************************************************
 ****************************************************************************
 *
 * format/bitstream.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_BITSTREAM_H
#define CWTOOL_FORMAT_BITSTREAM_H

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct fifo;
struct bounds;

#define BITSTREAM_COUNTER_INIT(b, p, s)	(struct bitstream_counter) { .bnd = b, .precomp = p, .bnd_size = s }

struct bitstream_counter
	{
	int				last;
	int				this;
	int				last_i;
	int				invalid;
	struct bounds			*bnd;
	short				*precomp;
	int				bnd_size;
	};

struct bitstream_map
	{
	cw_count_t			length:8;
	cw_count_t			length_sum:24;
	cw_raw8_t			error;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern int				bitstream_read_lookup(struct bounds *, int, int *);
extern int				bitstream_read_counter(struct fifo *, int *);
extern int				bitstream_write_counter(struct fifo *, struct bitstream_counter *, int);
extern int				bitstream_read(struct fifo *, struct fifo *, struct bounds *, int);
extern int				bitstream_read_map(struct fifo *, struct fifo *, struct bounds *, int, struct bitstream_map *, int);
extern int				bitstream_write(struct fifo *, struct fifo *, struct bounds *, short *, int);



#endif /* !CWTOOL_FORMAT_BITSTREAM_H */
/******************************************************** Karsten Scheibler */

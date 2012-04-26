/****************************************************************************
 ****************************************************************************
 *
 * format/bounds.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_BOUNDS_H
#define CWTOOL_FORMAT_BOUNDS_H

#define BOUNDS_OLD(l, w, h, c)		(struct bounds) { .read_low = l, .write = w - 1, .read_high = h, .count = c }
#define BOUNDS_NEW(l, w, h, c)		(struct bounds) { .read_low = l, .write = w, .read_high = h, .count = c }

struct bounds
	{
	unsigned short			read_low;
	unsigned short			write;
	unsigned short			read_high;
	unsigned short			count;
	};

extern int				bounds_old_set_read_low(struct bounds *, struct bounds *, int);
extern int				bounds_old_set_write(struct bounds *, int);
extern int				bounds_old_set_read_high(struct bounds *, int);
extern int				bounds_new_set_read_low(struct bounds *, struct bounds *, int);
extern int				bounds_new_set_write(struct bounds *, int);
extern int				bounds_new_set_read_high(struct bounds *, int);



#endif /* !CWTOOL_FORMAT_BOUNDS_H */
/******************************************************** Karsten Scheibler */

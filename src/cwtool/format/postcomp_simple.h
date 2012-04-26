/****************************************************************************
 ****************************************************************************
 *
 * format/postcomp_simple.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_POSTCOMP_SIMPLE_H
#define CWTOOL_FORMAT_POSTCOMP_SIMPLE_H

#include "types.h"

struct fifo;
struct bounds;

extern int				postcomp_simple(struct fifo *, struct bounds *, int);
extern int				postcomp_simple_adjust(struct fifo *, struct bounds *, int, int, int);



#endif /* !CWTOOL_FORMAT_POSTCOMP_SIMPLE_H */
/******************************************************** Karsten Scheibler */

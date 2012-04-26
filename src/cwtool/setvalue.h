/****************************************************************************
 ****************************************************************************
 *
 * setvalue.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_SETVALUE_H
#define CWTOOL_SETVALUE_H

#include "types.h"




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_bool_t			setvalue_uchar_bit(unsigned char *, int, int);
extern cw_bool_t			setvalue_uchar(unsigned char *, int, int, int);
extern cw_bool_t			setvalue_ushort(unsigned short *, int, int, int);
extern cw_bool_t			setvalue_short(short *, int, int, int);
extern cw_bool_t			setvalue_uint(unsigned int *, unsigned int, unsigned int, unsigned int);
extern cw_bool_t			setvalue_int(int *, int, int, int);
extern cw_bool_t			setvalue_uint_bit(unsigned int *, unsigned int, unsigned int);



#endif /* !CWTOOL_SETVALUE_H */
/******************************************************** Karsten Scheibler */

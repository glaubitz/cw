/****************************************************************************
 ****************************************************************************
 *
 * setvalue.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "setvalue.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/



/****************************************************************************
 * setvalue_uchar_bit
 ****************************************************************************/
cw_bool_t
setvalue_uchar_bit(
	unsigned char			*dst,
	int				val,
	int				bit)

	{
	debug_error_condition((bit < 0) || (bit > 0x80) || ((bit & (bit - 1)) != 0));
	if (val == 0) *dst &= ~bit;
	else if (val == 1) *dst |= bit;
	else return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_uchar
 ****************************************************************************/
cw_bool_t
setvalue_uchar(
	unsigned char			*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (CW_BOOL_FAIL);
	*dst = val;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_ushort
 ****************************************************************************/
cw_bool_t
setvalue_ushort(
	unsigned short			*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (CW_BOOL_FAIL);
	*dst = val;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_short
 ****************************************************************************/
cw_bool_t
setvalue_short(
	short				*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (CW_BOOL_FAIL);
	*dst = val;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_uint
 ****************************************************************************/
cw_bool_t
setvalue_uint(
	unsigned int			*dst,
	unsigned int			val,
	unsigned int			low,
	unsigned int			high)

	{
	if ((val < low) || (val > high)) return (CW_BOOL_FAIL);
	*dst = val;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_int
 ****************************************************************************/
cw_bool_t
setvalue_int(
	int				*dst,
	int				val,
	int				low,
	int				high)

	{
	if ((val < low) || (val > high)) return (CW_BOOL_FAIL);
	*dst = val;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * setvalue_uint_bit
 ****************************************************************************/
cw_bool_t
setvalue_uint_bit(
	unsigned int			*dst,
	unsigned int			val,
	unsigned int			bit)

	{
	debug_error_condition((bit < 0) || (bit > 0x80000000) || ((bit & (bit - 1)) != 0));
	if (val == 0) *dst &= ~bit;
	else if (val == 1) *dst |= bit;
	else return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

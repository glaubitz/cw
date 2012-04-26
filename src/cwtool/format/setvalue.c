/****************************************************************************
 ****************************************************************************
 *
 * format/setvalue.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "setvalue.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "bounds.h"



/****************************************************************************
 * setvalue_bounds_old
 ****************************************************************************/
int
setvalue_bounds_old(
	struct bounds			*bnd,
	int				val,
	int				ofs)

	{
	int				i = ofs % 3, j = ofs / 3;
	struct bounds			*bnd_last = (j > 0) ? &bnd[j - 1] : NULL;

	if (i == 0) return (bounds_old_set_read_low(bnd_last, &bnd[j], val));
	if (i == 1) return (bounds_old_set_write(&bnd[j], val));
	return (bounds_old_set_read_high(&bnd[j], val));
	}



/****************************************************************************
 * setvalue_bounds_new
 ****************************************************************************/
int
setvalue_bounds_new(
	struct bounds			*bnd,
	int				val,
	int				ofs)

	{
	int				i = ofs % 3, j = ofs / 3;
	struct bounds			*bnd_last = (j > 0) ? &bnd[j - 1] : NULL;

	if (i == 0) return (bounds_new_set_read_low(bnd_last, &bnd[j], val));
	if (i == 1) return (bounds_new_set_write(&bnd[j], val));
	return (bounds_new_set_read_high(&bnd[j], val));
	}
/******************************************************** Karsten Scheibler */

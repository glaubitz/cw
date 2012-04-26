/****************************************************************************
 ****************************************************************************
 *
 * format/postcomp_simple.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "postcomp_simple.h"
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
 * postcomp_simple_sign
 ****************************************************************************/
static const char *
postcomp_simple_sign(
	int				val)

	{
	if (val < 0) return ("-");
	return ("");
	}



/****************************************************************************
 * postcomp_simple_value
 ****************************************************************************/
static int
postcomp_simple_value(
	int				val)

	{
	if (val < 0) return (-val);
	return (val);
	}



/****************************************************************************
 * raw_val
 ****************************************************************************/
static int
raw_val(
	struct bounds			*bnd,
	int				i,
	int				adjust0,
	int				adjust1)

	{
	int				a = adjust0;
	int				val;

	if (i != 0) a = adjust1;
	val = (bnd[i].write + a) >> 8;
	if (val < GLOBAL_MIN_PULSE_LENGTH) val = GLOBAL_MIN_PULSE_LENGTH;
	if (val > GLOBAL_MAX_PULSE_LENGTH) val = GLOBAL_MAX_PULSE_LENGTH;
	return (val);
	}



/****************************************************************************
 * postcomp_simple_lookup
 ****************************************************************************/
static void
postcomp_simple_lookup(
	struct bounds			*bnd,
	int				bnd_size,
	int				*lookup,
	int				area,
	int				adjust0,
	int				adjust1)

	{
	int				i, j, l, h, m, v;

	for (i = 0; i < GLOBAL_NR_PULSE_LENGTHS; i++) lookup[i] = -1;

	for (i = 0; i < bnd_size; i++)
		{
		v = raw_val(bnd, i, adjust0, adjust1);
		l = v - area;
		h = v + area;
		if (i != 0)
			{
			m = (raw_val(bnd, i - 1, adjust0, adjust1) + v) / 2;
			l = (l < m + 1) ? m + 1 : l;
			}
		else l = (l < 0) ? 0 : l;
		if (i != bnd_size - 1)
			{
			m = (v + raw_val(bnd, i + 1, adjust0, adjust1)) / 2;
			h = (h > m - 1) ? m - 1 : h;
			}
		else h = (h > GLOBAL_MAX_PULSE_LENGTH) ? GLOBAL_MAX_PULSE_LENGTH : h;
		for (j = l; j <= h; j++) lookup[j] = i;
		}
	}



/****************************************************************************
 * postcomp_simple_calculate
 ****************************************************************************/
static int
postcomp_simple_calculate(
	struct bounds			*bnd,
	int				bnd_size,
	unsigned char			*data,
	char				*error,
	char				*done,
	int				len,
	int				stage,
	int				area,
	int				adjust0,
	int				adjust1)

	{
	int				lookup[GLOBAL_NR_PULSE_LENGTHS];
	int				d, e, i, j, p;

	/* create lookup table */

	postcomp_simple_lookup(bnd, bnd_size, lookup, area, adjust0, adjust1);

	/* iterate over data */

	for (i = 1, e = error[i] / 2, p = 0; i < len - 1; i++)
		{
		d = (data[i] + e) & GLOBAL_PULSE_LENGTH_MASK;
		e = error[i + 1] / 2;
		j = lookup[d];
		if ((done[i] >= stage) || (j == -1)) continue;
		d -= raw_val(bnd, j, adjust0, adjust1);
		error[i - 1] += d / 2;
		error[i]     -= d;
		error[i + 1] += d - (d / 2);
		done[i] = stage;
		p++;
		}
	return (p);
	}



/****************************************************************************
 * postcomp_simple_apply
 ****************************************************************************/
static int
postcomp_simple_apply(
	unsigned char			*data,
	char				*error,
	int				len)

	{
	int				e, i, p;

	for (i = p = 0; i < len; i++)
		{
		e = error[i] / 2;
		if (e == 0) continue;
		data[i] += e;
		p++;
		}
	return (p);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * postcomp_simple
 ****************************************************************************/
int
postcomp_simple(
	struct fifo			*ffo,
	struct bounds			*bnd,
	int				bnd_size)

	{
	return (postcomp_simple_adjust(ffo, bnd, bnd_size, 0, 0));
	}



/****************************************************************************
 * postcomp_simple_adjust
 ****************************************************************************/
int
postcomp_simple_adjust(
	struct fifo			*ffo,
	struct bounds			*bnd,
	int				bnd_size,
	int				adjust0,
	int				adjust1)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				len   = fifo_get_wr_ofs(ffo);
	char				error[GLOBAL_MAX_TRACK_SIZE] = { };
	char				done[GLOBAL_MAX_TRACK_SIZE] = { };
	int				stage, area;

	/*
	 * increasing number of stages produces sometimes very nice
	 * histograms, but will not improve readability of a disk
	 */

	if (bnd_size < 2) return (0);
	verbose_message(GENERIC, 1, "doing simple postcompensation with adjust { %s0x%04x %s0x%04x }",
		postcomp_simple_sign(adjust0),
		postcomp_simple_value(adjust0),
		postcomp_simple_sign(adjust1),
		postcomp_simple_value(adjust1));
	for (stage = 1; stage < 2; stage++) for (area = 2; area < 16; area++)
		{
		postcomp_simple_calculate(bnd, bnd_size, data, error, done, len, stage, area, adjust0, adjust1);
		}
	return (postcomp_simple_apply(data, error, len));
	}
/******************************************************** Karsten Scheibler */

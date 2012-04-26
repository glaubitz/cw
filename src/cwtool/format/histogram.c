/****************************************************************************
 ****************************************************************************
 *
 * format/histogram.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "histogram.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../string.h"
#include "../fifo.h"
#include "bounds.h"
#include "postcomp_simple.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * histogram_get_max
 ****************************************************************************/
static cw_count_t
histogram_get_max(
	cw_hist_t			histogram,
	cw_count_t			increment)

	{
	cw_count_t			limits[] = { 10, 12, 14, 16, 18, 20, 25, 30, 40, 50, 60, 80, 100 };
	cw_count_t			h, factor, max;
	cw_index_t			i, j;

	/* find maximum value in histogram */

	error_condition((GLOBAL_NR_PULSE_LENGTHS % increment) != 0);
	for (i = max = 0; i < GLOBAL_NR_PULSE_LENGTHS; i += increment)
		{
		for (j = h = 0; j < increment; j++) h += histogram[i + j];
		if (h > max) max = h;
		}

	/* round up maximum value */

	for (factor = 1; factor < 1000000000; factor *= 10) if (max / factor < 100) break;
	if (max / factor > 100) return (max);
	for (i = 0; max / factor > limits[i]; i++) ;
	return (factor * limits[i]);
	}


/****************************************************************************
 * histogram_get_scale
 ****************************************************************************/
static cw_count_t
histogram_get_scale(
	cw_count_t			i,
	cw_count_t			m,
	cw_count_t			max)

	{
	if (! options_get_histogram_exponential()) return ((i * max) / 20);
	return ((i > 0) ? 4 * (m / 5) : 0);
	}


/****************************************************************************
 * histogram_head_line
 ****************************************************************************/
static cw_char_t *
histogram_head_line(
	cw_char_t			*line,
	cw_size_t			size,
	cw_count_t			length,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side,
	const cw_char_t			*info,
	const cw_char_t			*info2)

	{
	cw_count_t			l;

	if (format_track != -1) 
		{
		if (format_side == -1) l = string_snprintf(line, size, "===[track %d (%d)", cwtool_track, format_track);
		else l = string_snprintf(line, size, "===[track %d (%d,%d)", cwtool_track, format_track, format_side);
		}
	else l = string_snprintf(line, size, "===[track %d", cwtool_track);
	if (info != NULL) l += string_snprintf(&line[l], size - l, " | %s", info);
	if (info2 != NULL) l += string_snprintf(&line[l], size - l, " | %s", info2);
	l += string_snprintf(&line[l], size - l, "]===");
	while (l < length) l += string_snprintf(&line[l], size - l, "=");
	return (line);
	}



/****************************************************************************
 * histogram_long
 ****************************************************************************/
static cw_void_t
histogram_long(
	cw_hist_t			histogram,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side,
	const cw_char_t			*info,
	const cw_char_t			*info2)

	{
	cw_char_t			line[4096];
	cw_count_t			m, max;
	cw_index_t			i, j;

	max = m = histogram_get_max(histogram, 1);
	printf("%s\n", histogram_head_line(line, sizeof (line), 128 + 6, cwtool_track, format_track, format_side, info, info2));
	for (i = 20; i-- > 0; )
		{
		m = histogram_get_scale(i, m, max);
		printf("%5d ", m);
		for (j = 0; j < GLOBAL_NR_PULSE_LENGTHS; j++)
			{
			if (histogram[j] > m) printf("*");
			else printf(" ");
			}
		printf("\n");
		}
	printf("      00000000000000001111111111111111222222222222222233333333333333334444444444444444555555555555555566666666666666667777777777777777\n");
	printf("      0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n");
	}



/****************************************************************************
 * histogram_short
 ****************************************************************************/
static cw_void_t
histogram_short(
	cw_hist_t			histogram,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side,
	const cw_char_t			*info,
	const cw_char_t			*info2)

	{
	cw_char_t			line[4096];
	cw_count_t			m, max;
	cw_index_t			i, j;

	max = m = histogram_get_max(histogram, 2);
	printf("%s\n", histogram_head_line(line, sizeof (line), 64 + 6, cwtool_track, format_track, format_side, info, info2));
	for (i = 20; i-- > 0; )
		{
		m = histogram_get_scale(i, m, max);
		printf("%5d ", m);
		for (j = 0; j < GLOBAL_NR_PULSE_LENGTHS; j += 2)
			{
			if (histogram[j] + histogram[j + 1] > m) printf("*");
			else printf(" ");
			}
		printf("\n");
		}
	printf("      0000000011111111222222223333333344444444555555556666666677777777\n");
	printf("      02468ace02468ace02468ace02468ace02468ace02468ace02468ace02468ace\n");
	}



/****************************************************************************
 * histogram_normal2
 ****************************************************************************/
static cw_void_t
histogram_normal2(
	struct fifo			*ffo,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side,
	const cw_char_t			*info)

	{
	cw_char_t			info2[4096];
	cw_raw8_t			*data = fifo_get_data(ffo);
	cw_size_t			size = fifo_get_wr_ofs(ffo);
	cw_hist_t			histogram = { };
	cw_hist2_t			histogram2 = { };
	cw_void_t			(*func)(cw_hist_t, cw_count_t, cw_count_t, cw_count_t, const cw_char_t *, const cw_char_t *);
	cw_index_t			i;

	func = histogram_short;
	if (verbose_get_level(VERBOSE_CLASS_CWTOOL_S) >= VERBOSE_LEVEL_1) func = histogram_long;
	histogram_calculate(data, size, histogram, histogram2);
	func(histogram, cwtool_track, format_track, format_side, info, NULL);
	if (! options_get_histogram_context()) return;
	for (i = 0; i < GLOBAL_NR_PULSE_LENGTHS; i++)
		{
		if (histogram[i] < size / 30) continue;
		string_snprintf(info2, sizeof (info2), "prefix 0x%02x", i);
		func(histogram2[i], cwtool_track, format_track, format_side, info, info2);
		}
	}



/****************************************************************************
 * histogram_analyze
 ****************************************************************************/
cw_void_t
histogram_analyze(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side,
	cw_count_t			start)

	{
	cw_raw8_t			*data = fifo_get_data(ffo);
	cw_size_t			size = fifo_get_wr_ofs(ffo);
	cw_hist_t			histogram = { };
	cw_index_t			i, j, l, h;
	cw_count64_t			sum;
	cw_count_t			count[GLOBAL_NR_BOUNDS] = { };
	cw_count_t			base;

	error_condition(bnd_size != 3);
	histogram_calculate(data, size, histogram, NULL);
	for (i = 0, sum = 0; i < bnd_size; i++)
		{
		l = (bnd[i].read_low   + 0xff) >> 8;
		h = (bnd[i].read_high  + 0xff) >> 8;
		for (j = l; j <= h; j++)
			{
			sum += (j << 8) * histogram[j];
			count[i] += histogram[j];
			}
		}
	base = sum / (start * count[0] + (start + 1) * count[1] + (start + 2) * count[2]);
	printf("bounds_new\n\t{\n");
	for (i = 0; i < bnd_size; i++)
		{
		printf("\t0x%04x 0x%04x 0x%04x\n", bnd[i].read_low, (start + i) * base, bnd[i].read_high);
		}
	printf("\t}\n");
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * histogram_calculate
 ****************************************************************************/
cw_void_t
histogram_calculate(
	cw_raw8_t			*data,
	cw_size_t			size,
	cw_hist_t			histogram,
	cw_hist2_t			histogram2)

	{
	cw_index_t			d1, d2, i;

	if ((histogram == NULL) && (histogram2 == NULL)) return;
	for (i = 1; i < size; i++)
		{
		d1 = data[i - 1] & GLOBAL_PULSE_LENGTH_MASK;
		d2 = data[i] & GLOBAL_PULSE_LENGTH_MASK;
		if (histogram != NULL) histogram[d1]++;
		if (histogram2 != NULL) histogram2[d1][d2]++;
		}
	}



/****************************************************************************
 * histogram_blur
 ****************************************************************************/
cw_void_t
histogram_blur(
	cw_hist_t			src,
	cw_hist_t			dst,
	cw_count_t			range)

	{
	cw_count_t			i, j, start, end;

	for (i = 0; i < GLOBAL_NR_PULSE_LENGTHS; i++)
		{
		start = i - range;
		end = i + range;
		if (start < 0) start = 0;
		if (end > GLOBAL_NR_PULSE_LENGTHS) end = GLOBAL_NR_PULSE_LENGTHS;
		for (j = start, dst[i] = 0; j <= end; j++) dst[i] += src[j];
		}
	}



/****************************************************************************
 * histogram_normal
 ****************************************************************************/
cw_void_t
histogram_normal(
	struct fifo			*ffo,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_normal2(ffo, cwtool_track, format_track, format_side, NULL);
	}



/****************************************************************************
 * histogram_postcomp_simple
 ****************************************************************************/
cw_void_t
histogram_postcomp_simple(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	cw_char_t			info[4096];
	cw_count_t			p;

	if (verbose_get_level(VERBOSE_CLASS_CWTOOL_S) >= VERBOSE_LEVEL_2)
		{
		p = postcomp_simple(ffo, bnd, bnd_size);
		string_snprintf(info, sizeof (info), "postcompensated %d values", p);
		histogram_normal2(ffo, cwtool_track, format_track, format_side, info);
		}
	}



/****************************************************************************
 * histogram_analyze_gcr
 ****************************************************************************/
cw_void_t
histogram_analyze_gcr(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_analyze(ffo, bnd, bnd_size, cwtool_track, format_track, format_side, 1);
	}



/****************************************************************************
 * histogram_analyze_mfm
 ****************************************************************************/
cw_void_t
histogram_analyze_mfm(
	struct fifo			*ffo,
	struct bounds			*bnd,
	cw_size_t			bnd_size,
	cw_count_t			cwtool_track,
	cw_count_t			format_track,
	cw_count_t			format_side)

	{
	histogram_analyze(ffo, bnd, bnd_size, cwtool_track, format_track, format_side, 2);
	}
/******************************************************** Karsten Scheibler */

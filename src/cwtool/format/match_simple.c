/****************************************************************************
 ****************************************************************************
 *
 * format/match_simple.c
 *
 ****************************************************************************
 *
 * - take data of two reads of the same track: (1) and (2)
 * - take the first WINDOW_SIZE pulse lengths from (1) and search for them
 *   in (2)
 * - allow some PULSE_JITTER while searching (deviation between (1) and (2))
 * - furthermore allow pulse degeneration to some point, that means longer
 *   pulse may degenerate to shorter pulses, the sum of them should match
 *   the longer pulse
 * - but at least MIN_MATCHES pulse lengths should be the same
 * - try to merge the two track copies into one (hopefully with less errors)
 * - use error information from format description (absolute difference of
 *   pulse length to expected pulse length, favor those pulse lengths with
 *   less error)
 * - some examples of possible errors (numbers in hex):
 *   a) long pulse 54 in (2) degenerated to three shorter pulses in (1)
 *      22, 13, 1c
 *      (1) (2)
 *      1c  1c
 *      1c  1c
 *      37  36
 *      1d  1d
 *      22  54
 *      13
 *      1c
 *      1d  1c
 *      36  37
 *      1c  1c
 *      1b  1b
 *      1c  1c
 *   b) long pulse 6f in (1) degenerated to three shorter pulses in (1) 0e,
 *      27, 37. Looks like a) but the longer pulse itself is also wrong
 *      because it is too long and should be two pulses (somehing like 38,37)
 *      this cannot be fixed without a third copy of the track data
 *      (1) (2)
 *      37  37
 *      1c  1d
 *      1d  1d
 *      6f  0e
 *          27
 *          37
 *      1c  1c
 *      1b  1b
 *      1d  1d
 *      53  53
 *      1d  1d
 *   c) pulse sequence in (2) with 53, 1d is correct, looks like 53
 *      degenerated to three pulses in (1), the first 25 survived the last
 *      two joined with the following 1d to 4a.
 *      (1) (2)
 *      35  36
 *      1d  1d
 *      1b  1b
 *      1c  1c
 *      25  53
 *      4a
 *          1d
 *      36  36
 *      1d  1d
 *      37  37
 *      38  37
 *      36  36
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "match_simple.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../fifo.h"
#include "bitstream.h"
#include "container.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define SEARCH_START			0
#define WINDOW_SIZE			512
#define PULSE_JITTER			4
#define MIN_MATCHES			(WINDOW_SIZE - 3 * (WINDOW_SIZE / 64))

struct match_state
	{
	cw_raw8_t			*data1;
	cw_raw8_t			*data2;
	cw_index_t			data1_offset;
	cw_index_t			data2_offset;
	cw_size_t			data1_limit;
	cw_size_t			data2_limit;
	};




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * match_simple_fixup_long_pulses
 ****************************************************************************/
static cw_size_t
match_simple_fixup_long_pulses(
	cw_raw8_t			*data,
	cw_raw8_t			*error,
	cw_size_t			size)

	{
	cw_count_t			e[3], s;
	cw_index_t			i, j;

	for (i = j = 2; i < size; i++, j++)
		{
		e[0] = error[i - 2];
		e[1] = error[i - 1];
		e[2] = error[i];
		data[j - 2] = data[i - 2];
		if ((e[1] != 0xff) || (e[0] + e[2] < 7)) continue;
		s = data[i - 2] + data[i - 1] + data[i];
		verbose_message(GENERIC, 4, "fixup: %02x %02x %02x => %02x", data[i - 2], data[i - 1], data[i], s);
		data[j - 2] = s;
		i += 2;
		}

	return (j);
	}



/****************************************************************************
 * match_simple_print_window
 ****************************************************************************/
static cw_void_t
match_simple_print_window(
	cw_raw8_t			*data1,
	cw_raw8_t			*data2,
	cw_size_t			window_size,
	cw_count_t			pulse_jitter)

	{
	cw_index_t			i, j;
	cw_count_t			s1, s2;
	cw_raw8_t			d1, d2;

	s1 = s2 = 0;
	for (i = j = 0; (i < window_size) && (j < window_size); )
		{
		d1 = data1[i] & GLOBAL_PULSE_LENGTH_MASK;
		d2 = data2[j] & GLOBAL_PULSE_LENGTH_MASK;
		if (s1 < s2 - pulse_jitter)
			{
			s1 += d1;
			verbose_message(GENERIC, 4, "match_simple: %02x(%04x)          [%2d]", d1, s1, s1 - s2);
			i++;
			}
		else if (s2 < s1 - pulse_jitter)
			{
			s2 += d2;
			verbose_message(GENERIC, 4, "match_simple:          %02x(%04x) [%2d]", d2, s2, s1 - s2);
			j++;
			}
		else
			{
			s1 = d1;
			s2 = d2;
			verbose_message(GENERIC, 4, "match_simple: %02x(%04x) %02x(%04x) [%2d]", d1, s1, d2, s2, s1 - s2);
			i++, j++;
			}
		}
	}



/****************************************************************************
 * match_simple_compare_window
 ****************************************************************************/
static cw_count_t
match_simple_compare_window(
	struct match_state		*sta,
	cw_size_t			window_size,
	cw_count_t			pulse_jitter,
	cw_count_t			min_matches)

	{
	cw_index_t			i, j;
	cw_count_t			o1, o2, s1, s2, m, mm;
	cw_raw8_t			d1, d2;

	i = j = 0;
	s1 = s2 = m = 0;
	mm = window_size - min_matches;	/* max allowed mismatches */
	o1 = sta->data1_offset;
	o2 = sta->data2_offset;
	if (sta->data1_offset + window_size >= sta->data1_limit) return (-1);
	if (sta->data2_offset + window_size >= sta->data2_limit) return (-1);
	while ((i < window_size) && (j < window_size))
		{
		d1 = sta->data1[o1 + i] & GLOBAL_PULSE_LENGTH_MASK;
		d2 = sta->data2[o2 + j] & GLOBAL_PULSE_LENGTH_MASK;

		/* do not continue if it is impossible to reach min_matches */

		if (i - m > mm) return (-1);
		if (j - m > mm) return (-1);

		/*
		 * do not try to match pulses of undefined length
		 * (GLOBAL_MAX_PULSE_LENGTH)
		 */

		if (d1 == GLOBAL_MAX_PULSE_LENGTH) return (-1);
		if (d2 == GLOBAL_MAX_PULSE_LENGTH) return (-1);

		/* check if pulse lengths match */

		if      (s1 < s2 - pulse_jitter) s1 += d1, i++;
		else if (s2 < s1 - pulse_jitter) s2 += d2, j++;
		else
			{
			s1 = d1, i++;
			s2 = d2, j++;
			if ((d1 >= d2 - pulse_jitter) && (d2 >= d1 - pulse_jitter))
				{
				sta->data1_offset = o1 + i;
				sta->data2_offset = o2 + j;
				m++;
				}
			}
		}
	return (m);
	}



/****************************************************************************
 * match_simple_search_start
 ****************************************************************************/
static cw_index_t
match_simple_search_start(
	cw_raw8_t			*data1,
	cw_size_t			data1_limit,
	cw_raw8_t			*data2,
	cw_size_t			data2_limit,
	cw_index_t			i,
	cw_size_t			window_size,
	cw_count_t			pulse_jitter,
	cw_count_t			min_matches)

	{
	struct match_state		sta;
	cw_index_t			j;
	cw_count_t			m;

	sta = (struct match_state)
		{
		.data1       = data1,
		.data2       = data2,
		.data1_limit = data1_limit,
		.data2_limit = data2_limit
		};
	for (j = 0; j < data2_limit; j++)
		{
		sta.data1_offset = i;
		sta.data2_offset = j;
		m = match_simple_compare_window(
			&sta,
			window_size,
			pulse_jitter,
			min_matches);
		if (m < min_matches) continue;
		verbose_message(GENERIC, 3, "match_simple: window_size = %d, matches = %d, position %d", window_size, m, j);
		match_simple_print_window(
			&data1[i],
			&data2[j],
			window_size,
			pulse_jitter);
		return (j);
		}
	verbose_message(GENERIC, 3, "match_simple: window_size = %d, no match", window_size);
	return (-1);
	}



/****************************************************************************
 * match_simple_search_end
 ****************************************************************************/
static cw_index_t
match_simple_search_end(
	cw_raw8_t			*data1,
	cw_size_t			data1_limit,
	cw_raw8_t			*data2,
	cw_size_t			data2_limit,
	cw_index_t			i,
	cw_index_t			j,
	cw_size_t			window_size,
	cw_count_t			pulse_jitter,
	cw_count_t			min_matches)

	{
	struct match_state		sta;
	cw_count_t			m;

	sta = (struct match_state)
		{
		.data1        = data1,
		.data2        = data2,
		.data1_limit  = data1_limit,
		.data2_limit  = data2_limit,
		.data1_offset = i,
		.data2_offset = j
		};
	while (1)
		{
		i = sta.data1_offset;
		j = sta.data2_offset;
		m = match_simple_compare_window(
			&sta,
			window_size,
			pulse_jitter,
			min_matches);
		verbose_message(GENERIC, 3, "match_simple: continuing windows_size = %d, matches = %d, position = %d - %d", window_size, m, j, sta.data2_offset);
		if ((m < min_matches) && (j < 70000))
			{
			verbose_message(GENERIC, 4, "match_simple: early end");
			match_simple_print_window(
				&data1[i],
				&data2[j],
				2 * window_size,
				pulse_jitter);
			}
		if (m < min_matches) break;
/*		if (m != window_size) match_simple_print_window(
			&data1[i],
			&data2[j],
			window_size,
			pulse_jitter);*/
		}
	return (j);
	}



/****************************************************************************
 * match_simple_merge
 ****************************************************************************/
static cw_size_t
match_simple_merge(
	cw_raw8_t			*data_dst,
	cw_raw8_t			*error_dst,
	cw_size_t			data_dst_limit,
	cw_raw8_t			*data1,
	cw_raw8_t			*error1,
	cw_size_t			data1_limit,
	cw_raw8_t			*data2,
	cw_raw8_t			*error2,
	cw_size_t			data2_limit,
	cw_index_t			start,
	cw_index_t			end,
	cw_count_t			pulse_jitter)

	{
	cw_index_t			d, i, j, k;
	cw_count_t			s1, s2;
	cw_raw8_t			d1, d2, e1, e2;

	verbose_message(GENERIC, 3, "match_simple_merge: start = %d, end = %d", start, end);
	d = i = k = 0;
	j = start;
	s1 = s2 = 0;
	while ((i < data1_limit) && (j < data2_limit) && (j < end) && (k < data_dst_limit))
		{
		d1 = data1[i] & GLOBAL_PULSE_LENGTH_MASK;
		d2 = data2[j] & GLOBAL_PULSE_LENGTH_MASK;
		e1 = error1[i];
		e2 = error2[j];

		/* check if pulse lengths match */

		if (s1 < s2 - pulse_jitter)
			{
			s1 += d1, i++;
			if (d == 1) error_dst[k] = e1, data_dst[k] = d1, k++;
/*			verbose_message(GENERIC, 4, "match_simple_merge: %02x(%04x,%02x)                [%2d]", d1, s1, e1, s1 - s2);*/
			}
		else if (s2 < s1 - pulse_jitter)
			{
			s2 += d2, j++;
			if (d == 2) error_dst[k] = e2, data_dst[k] = d2, k++;
/*			verbose_message(GENERIC, 4, "match_simple_merge:             %02x(%04x,%02x)    [%2d]", d2, s2, e2, s1 - s2);*/
			}
		else
			{
			s1 = d1, i++;
			s2 = d2, j++;
			if (((d1 >= d2 - pulse_jitter) && (d2 >= d1 - pulse_jitter)) || (e1 <= e2))
				{
				d = 1, error_dst[k] = e1, data_dst[k] = d1;
				}
			else
				{
				d = 2, error_dst[k] = e2, data_dst[k] = d2;
				}
/*			verbose_message(GENERIC, 4, "match_simple_merge: %02x(%04x,%02x) %02x(%04x,%02x) %02x(%02x) [%2d]", d1, s1, e1, d2, s2, e2, data_dst[k], error_dst[k], s1 - s2);*/
			k++;
			}
		}
	while ((i < data1_limit) && (k < data_dst_limit))
		{
		data_dst[k] = data1[i];
		error_dst[k] = error1[i];
		k++, i++;
		}
	verbose_message(GENERIC, 3, "match_simple_merge: new size = %d", k);
	return (k);
	}



/****************************************************************************
 * match_simple_store2
 ****************************************************************************/
static cw_void_t
match_simple_store2(
	struct match_simple_info	*mat_sim_nfo,
	struct bitstream_map		*bst_map,
	cw_size_t			size,
	cw_index_t			index)

	{
	struct container_lookup		*con_lkp;
	cw_raw8_t			*error;
	cw_index_t			i;

	/* store error information in container */

	error = container_get_error(mat_sim_nfo->con, index);
	for (i = 0; i < size; i++) error[i] = bst_map[i].error;

	/* store lookup information in container */

	con_lkp = container_get_lookup(mat_sim_nfo->con, index);
	for (i = 0; i < size; i++) con_lkp[i] = (struct container_lookup)
		{
		.length     = bst_map[i].length,
		.length_sum = bst_map[i].length_sum
		};
	}



/****************************************************************************
 * match_simple_store
 ****************************************************************************/
static cw_index_t
match_simple_store(
	struct match_simple_info	*mat_sim_nfo)

	{
	struct bitstream_map		bst_map[GLOBAL_MAX_TRACK_SIZE];
	cw_size_t			size;
	cw_index_t			i;

	/* get error information (deviation from write bounds) */

	size = fifo_get_wr_ofs(mat_sim_nfo->ffo_l0);
	bitstream_read_map(
		mat_sim_nfo->ffo_l0,
		NULL,
		mat_sim_nfo->bnd,
		mat_sim_nfo->bnd_size,
		bst_map,
		GLOBAL_MAX_TRACK_SIZE);
	fifo_set_rd_ofs(mat_sim_nfo->ffo_l0, 0);

	/*
	 * store raw data in container. store a copy of first track data.
	 * this is later used to mix all tracks together
	 */

	if (container_get_entries(mat_sim_nfo->con) == 0)
		{
		container_store_data_and_error(
			mat_sim_nfo->con,
			fifo_get_data(mat_sim_nfo->ffo_l0),
			NULL,
			GLOBAL_MAX_TRACK_SIZE);
		match_simple_store2(mat_sim_nfo, bst_map, size, 0);
		}
	i = container_store_data_and_error(
		mat_sim_nfo->con,
		fifo_get_data(mat_sim_nfo->ffo_l0),
		NULL,
		size);
	match_simple_store2(mat_sim_nfo, bst_map, size, i);

	/* return container number */

	return (i);
	}



/****************************************************************************
 * match_simple_do_callback
 ****************************************************************************/
static cw_void_t
match_simple_do_callback(
	struct match_simple_info	*mat_sim_nfo,
	struct container		*con)

	{
	mat_sim_nfo->callback(
		mat_sim_nfo->fmt,
		con,
		mat_sim_nfo->ffo_l0,
		mat_sim_nfo->ffo_l3,
		mat_sim_nfo->dsk_sct,
		mat_sim_nfo->cwtool_track,
		mat_sim_nfo->format_track,
		mat_sim_nfo->format_side);
	}



/****************************************************************************
 * match_simple_merge2
 ****************************************************************************/
static cw_size_t
match_simple_merge2(
	struct match_simple_info	*mat_sim_nfo,
	cw_raw8_t			*data_dst,
	cw_raw8_t			*error_dst,
	cw_size_t			data_dst_limit,
	cw_index_t			index1,
	cw_index_t			index2)

	{
	cw_raw8_t			data3[GLOBAL_MAX_TRACK_SIZE];
	cw_raw8_t			error3[GLOBAL_MAX_TRACK_SIZE];
	cw_raw8_t			*data1, *error1, *data2, *error2;
	cw_size_t			data1_limit, data2_limit, data3_limit;
	cw_index_t			i, j;

	data1       = container_get_data(mat_sim_nfo->con, index1);
	error1      = container_get_error(mat_sim_nfo->con, index1);
	data1_limit = container_get_limit(mat_sim_nfo->con, index1);
	data2       = container_get_data(mat_sim_nfo->con, index2);
	error2      = container_get_error(mat_sim_nfo->con, index2);
	data2_limit = container_get_limit(mat_sim_nfo->con, index2);

	i = match_simple_search_start(
		data1,
		data1_limit,
		data2,
		data2_limit,
		SEARCH_START,
		WINDOW_SIZE,
		PULSE_JITTER,
		MIN_MATCHES);

	if (i == -1) return (-1);

	j = match_simple_search_end(
		data1,
		data1_limit,
		data2,
		data2_limit,
		SEARCH_START,
		i,
		WINDOW_SIZE,
		PULSE_JITTER,
		MIN_MATCHES);

	data3_limit = match_simple_merge(
		data3,
		error3,
		GLOBAL_MAX_TRACK_SIZE,
		&data1[SEARCH_START],
		&error1[SEARCH_START],
		data1_limit - SEARCH_START,
		data2,
		error2,
		data2_limit,
		i,
		j,
		PULSE_JITTER);

	i = match_simple_search_start(
		data2,
		data2_limit,
		data3,
		data3_limit,
		SEARCH_START,
		WINDOW_SIZE,
		PULSE_JITTER,
		MIN_MATCHES);

	/* if data contains a complete rotation this should not happen */

	if (i == -1) return (-1);

	j = match_simple_search_end(
		data2,
		data2_limit,
		data3,
		data3_limit,
		SEARCH_START,
		i,
		WINDOW_SIZE,
		PULSE_JITTER,
		MIN_MATCHES);

	data_dst_limit = match_simple_merge(
		data_dst,
		error_dst,
		data_dst_limit,
		&data2[SEARCH_START],
		&error2[SEARCH_START],
		data2_limit - SEARCH_START,
		data3,
		error3,
		data3_limit,
		i,
		j,
		PULSE_JITTER);

	return (data_dst_limit);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * match_simple
 ****************************************************************************/
cw_void_t
match_simple(
	struct match_simple_info	*mat_sim_nfo)

	{
	cw_raw8_t			error_dummy[GLOBAL_MAX_TRACK_SIZE];
	cw_index_t			i, j;
	cw_size_t			limit;

	/* store raw data for later usage */

	i = match_simple_store(mat_sim_nfo);

	/* give unprocessed data to callback */

	match_simple_do_callback(mat_sim_nfo, mat_sim_nfo->con);

	/* merge the current track with another track read earlier */

	if (mat_sim_nfo->merge_two) for (j = 1; j < i; j++)
		{
		limit = match_simple_merge2(
			mat_sim_nfo,
			fifo_get_data(mat_sim_nfo->ffo_l0),
			error_dummy,
			fifo_get_size(mat_sim_nfo->ffo_l0),
			j,
			i);
		if (limit == -1) continue;
		fifo_reset(mat_sim_nfo->ffo_l0);
		if (mat_sim_nfo->fixup) limit = match_simple_fixup_long_pulses(
			fifo_get_data(mat_sim_nfo->ffo_l0),
			error_dummy,
			limit);
		fifo_set_wr_ofs(mat_sim_nfo->ffo_l0, limit);
		match_simple_do_callback(mat_sim_nfo, NULL);
		}

	/* mix the current track to the mixed versions of all other tracks */

	if (mat_sim_nfo->merge_all) while (i > 1)
		{
		limit = match_simple_merge2(
			mat_sim_nfo,
			container_get_data(mat_sim_nfo->con, 0),
			container_get_error(mat_sim_nfo->con, 0),
			container_get_size(mat_sim_nfo->con, 0),
			0,
			i);
		if (limit == -1) break;
		container_set_limit(mat_sim_nfo->con, 0, limit);
		fifo_reset(mat_sim_nfo->ffo_l0);
		fifo_write_block(
			mat_sim_nfo->ffo_l0,
			container_get_data(mat_sim_nfo->con, 0),
			limit);
		if (mat_sim_nfo->fixup) limit = match_simple_fixup_long_pulses(
			fifo_get_data(mat_sim_nfo->ffo_l0),
			error_dummy,
			limit);
		fifo_set_wr_ofs(mat_sim_nfo->ffo_l0, limit);
		match_simple_do_callback(mat_sim_nfo, NULL);
		break;
		}
	}
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * format/container.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "container.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * container_init
 ****************************************************************************/
struct container *
container_init(
	struct container		*con)

	{
	cw_flag_t			flags = CONTAINER_FLAG_INITIALIZED;

	if (con == NULL)
		{
		con = malloc(sizeof (struct container));
		if (con == NULL) error_oom();
		flags |= CONTAINER_FLAG_MALLOC;
		}
	*con = (struct container) { .flags = flags };
	return (con);
	}



/****************************************************************************
 * container_deinit
 ****************************************************************************/
cw_void_t
container_deinit(
	struct container		*con)

	{
	cw_flag_t			flags = con->flags;
	cw_index_t			i;

	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	for (i = 0; i < con->entries; i++)
		{
		if (con->data[i]  != NULL) free(con->data[i]);
		if (con->error[i] != NULL) free(con->error[i]);
		if (con->lkp[i]   != NULL) free(con->lkp[i]);
		}
	con->flags = CONTAINER_FLAG_NONE;
	if (flags & CONTAINER_FLAG_MALLOC) free(con);
	}



/****************************************************************************
 * container_store_data_and_error
 ****************************************************************************/
cw_index_t
container_store_data_and_error(
	struct container		*con,
	cw_raw8_t			*data,
	cw_raw8_t			*error,
	cw_size_t			size)

	{
	cw_index_t			i = con->entries;

	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition(i >= CONTAINER_NR_ENTRIES);
	con->data[i]  = malloc(size * sizeof (cw_raw8_t));
	con->error[i] = malloc(size * sizeof (cw_raw8_t));
	con->lkp[i]   = malloc(size * sizeof (struct container_lookup));
	con->size[i]  = size;
	con->limit[i] = size;
	con->entries++;
	if ((con->data[i] == NULL) || (con->error[i] == NULL) || (con->lkp[i] == NULL)) error_oom();
	if (data  != NULL) memcpy(con->data[i],  data,  size);
	if (error != NULL) memcpy(con->error[i], error, size);
	memset(con->lkp[i], 0, size * sizeof (struct container_lookup));
	return(i);
	}



/****************************************************************************
 * container_get_entries
 ****************************************************************************/
cw_count_t
container_get_entries(
	struct container		*con)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	return (con->entries);
	}



/****************************************************************************
 * container_get_data
 ****************************************************************************/
cw_raw8_t *
container_get_data(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	return (con->data[index]);
	}



/****************************************************************************
 * container_get_error
 ****************************************************************************/
cw_raw8_t *
container_get_error(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	return (con->error[index]);
	}



/****************************************************************************
 * container_get_lookup
 ****************************************************************************/
struct container_lookup *
container_get_lookup(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	return (con->lkp[index]);
	}



/****************************************************************************
 * container_get_size
 ****************************************************************************/
cw_size_t
container_get_size(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	return (con->size[index]);
	}



/****************************************************************************
 * container_set_limit
 ****************************************************************************/
cw_void_t
container_set_limit(
	struct container		*con,
	cw_index_t			index,
	cw_size_t			limit)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	error_condition((limit < 0) || (limit > con->size[index]));
	con->limit[index] = limit;
	}



/****************************************************************************
 * container_get_limit
 ****************************************************************************/
cw_size_t
container_get_limit(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	return (con->limit[index]);
	}



/****************************************************************************
 * container_lookup_position
 ****************************************************************************/
cw_count_t
container_lookup_position(
	struct container		*con,
	cw_index_t			index,
	cw_count_t			bitofs)

	{
	struct container_lookup		*con_lkp;
	cw_index_t			l, m, h;
	cw_count_t			s;

	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	l = 0;
	h = con->limit[index];
	con_lkp = con->lkp[index];
	do
		{
		m = (l + h) / 2;
		s = con_lkp[m].length_sum;
		if (bitofs < s) h = m;
		if (bitofs > s) l = m;
		}
	while ((bitofs != s) && (h - l > 1));
	return (m);
	}



/****************************************************************************
 * container_store_range_sector
 ****************************************************************************/
cw_index_t
container_store_range_sector(
	struct container		*con,
	cw_index_t			index,
	struct range_sector		*rng_sec)

	{
	cw_index_t			i;

	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	i = con->range_entries[index];
	error_condition(i >= CONTAINER_NR_RANGES);
	con->rng_sec[index][i] = *rng_sec;
	con->range_entries[index]++;
	return (i);
	}



/****************************************************************************
 * container_append_range_sector
 ****************************************************************************/
cw_index_t
container_append_range_sector(
	struct container		*con,
	struct range_sector		*rng_sec)

	{
	return (container_store_range_sector(con, con->entries - 1, rng_sec));
	}



/****************************************************************************
 * container_get_range_entries
 ****************************************************************************/
cw_count_t
container_get_range_entries(
	struct container		*con,
	cw_index_t			index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	return (con->range_entries[index]);
	}



/****************************************************************************
 * container_get_range_sector
 ****************************************************************************/
struct range_sector *
container_get_range_sector(
	struct container		*con,
	cw_index_t			index,
	cw_index_t			range_index)

	{
	error_condition(! (con->flags & CONTAINER_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= con->entries));
	error_condition((range_index < 0) || (range_index >= con->range_entries[index]));
	return (&con->rng_sec[index][range_index]);
	}
/******************************************************** Karsten Scheibler */

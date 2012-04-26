/****************************************************************************
 ****************************************************************************
 *
 * format/container.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_CONTAINER_H
#define CWTOOL_FORMAT_CONTAINER_H

#include "types.h"
#include "../global.h"
#include "range.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




/*
 * (number of retries + first read) * number of images (last one is
 * destination)
 */

#define CONTAINER_NR_ENTRIES		(GLOBAL_NR_RETRIES + 1) * (GLOBAL_NR_IMAGES - 1)
#define CONTAINER_NR_RANGES		(4 * GLOBAL_NR_SECTORS)

struct container_lookup
	{
	cw_count_t			length:8;
	cw_count_t			length_sum:24;
	};

#define CONTAINER_FLAG_NONE		0
#define CONTAINER_FLAG_INITIALIZED	(1 << 0)
#define CONTAINER_FLAG_MALLOC		(1 << 1)

struct container
	{
	cw_raw8_t			*data[CONTAINER_NR_ENTRIES];
	cw_raw8_t			*error[CONTAINER_NR_ENTRIES];
	struct container_lookup		*lkp[CONTAINER_NR_ENTRIES];
	cw_size_t			size[CONTAINER_NR_ENTRIES];
	cw_size_t			limit[CONTAINER_NR_ENTRIES];
	cw_count_t			entries;
	struct range_sector		rng_sec[CONTAINER_NR_ENTRIES][CONTAINER_NR_RANGES];
	cw_count_t			range_entries[CONTAINER_NR_ENTRIES];
	cw_flag_t			flags;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern struct container *
container_init(
	struct container		*con);

extern cw_void_t
container_deinit(
	struct container		*con);

extern cw_index_t
container_store_data_and_error(
	struct container		*con,
	cw_raw8_t			*data,
	cw_raw8_t			*error,
	cw_size_t			size);

extern cw_count_t
container_get_entries(
	struct container		*con);

extern cw_raw8_t *
container_get_data(
	struct container		*con,
	cw_index_t			index);

extern cw_raw8_t *
container_get_error(
	struct container		*con,
	cw_index_t			index);

extern struct container_lookup *
container_get_lookup(
	struct container		*con,
	cw_index_t			index);

extern cw_size_t
container_get_size(
	struct container		*con,
	cw_index_t			index);

extern cw_void_t
container_set_limit(
	struct container		*con,
	cw_index_t			index,
	cw_size_t			limit);

extern cw_size_t
container_get_limit(
	struct container		*con,
	cw_index_t			index);

extern cw_count_t
container_lookup_position(
	struct container		*con,
	cw_index_t			index,
	cw_count_t			bitofs);

extern cw_index_t
container_store_range_sector(
	struct container		*con,
	cw_index_t			index,
	struct range_sector		*rng_sec);

extern cw_index_t
container_append_range_sector(
	struct container		*con,
	struct range_sector		*rng_sec);

extern cw_count_t
container_get_range_entries(
	struct container		*con,
	cw_index_t			index);

extern struct range_sector *
container_get_range_sector(
	struct container		*con,
	cw_index_t			index,
	cw_index_t			range_index);



#endif /* !CWTOOL_FORMAT_CONTAINER_H */
/******************************************************** Karsten Scheibler */

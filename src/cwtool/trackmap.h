/****************************************************************************
 ****************************************************************************
 *
 * trackmap.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_TRACKMAP_H
#define CWTOOL_TRACKMAP_H

#include "types.h"
#include "global.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define TRACKMAP_ENTRY_INIT(t)		(struct trackmap_entry) { .cwtool_track = t, .image_track = -1, .format_track = -1, .format_side = -1 }

struct trackmap_entry
	{
	cw_index_t			index;
	cw_count_t			cwtool_track;
	cw_count_t			image_track;
	cw_count_t			format_track;
	cw_count_t			format_side;
	};

#define TRACKMAP_FLAG_INITIALIZED	(1 << 0)
#define TRACKMAP_MAX_NAME_SIZE		GLOBAL_MAX_NAME_SIZE
#define TRACKMAP_NR_ENTRIES		GLOBAL_NR_TRACKS

struct trackmap
	{
	cw_char_t			name[TRACKMAP_MAX_NAME_SIZE];
	struct trackmap_entry		ent_by_index[TRACKMAP_NR_ENTRIES];
	struct trackmap_entry		*ent_by_cwtool_track[TRACKMAP_NR_ENTRIES];
	cw_count_t			entries;
	cw_flag_t			flags;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern struct trackmap *
trackmap_get(
	cw_index_t			index);

extern struct trackmap *
trackmap_search(
	const cw_char_t			*name);

extern cw_void_t
trackmap_init(
	struct trackmap			*trm,
	const cw_char_t			*name);

extern cw_void_t
trackmap_deinit(
	struct trackmap			*trm);

extern cw_count_t
trackmap_entries(
	struct trackmap			*trm);

extern cw_bool_t
trackmap_full(
	struct trackmap			*trm);

extern cw_bool_t
trackmap_cwtool_track_present(
	struct trackmap			*trm,
	cw_count_t			cwtool_track);

extern cw_bool_t
trackmap_cwtool_track_ok(
	struct trackmap			*trm,
	cw_count_t			cwtool_track);

extern cw_bool_t
trackmap_image_track_present(
	struct trackmap			*trm,
	cw_count_t			image_track);

extern cw_bool_t
trackmap_image_track_ok(
	struct trackmap			*trm,
	cw_count_t			image_track);

extern cw_bool_t
trackmap_entry_append(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern struct trackmap_entry *
trackmap_entry_get_by_index(
	struct trackmap			*trm,
	cw_index_t			index);

extern struct trackmap_entry *
trackmap_entry_get_by_cwtool_track(
	struct trackmap			*trm,
	cw_count_t			cwtool_track);

extern cw_bool_t
trackmap_entry_check(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern cw_bool_t
trackmap_entry_set_image_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			image_track);

extern cw_bool_t
trackmap_entry_set_format_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			format_track);

extern cw_bool_t
trackmap_entry_set_format_side(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			format_side);

extern cw_index_t
trackmap_entry_get_index(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern cw_count_t
trackmap_entry_get_cwtool_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern cw_count_t
trackmap_entry_get_image_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern cw_count_t
trackmap_entry_get_format_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);

extern cw_count_t
trackmap_entry_get_format_side(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent);



#endif /* !CWTOOL_TRACKMAP_H */
/******************************************************** Karsten Scheibler */

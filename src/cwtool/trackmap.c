/****************************************************************************
 ****************************************************************************
 *
 * trackmap.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "trackmap.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "string.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * trackmap_add_default
 ****************************************************************************/
static cw_void_t
trackmap_add_default(
	cw_void_t)

	{
	struct trackmap			*trm = trackmap_get(-1);
	struct trackmap_entry		trm_ent;
	cw_index_t			t;

	trackmap_init(trm, "#default");
	for (t = 0; t < GLOBAL_NR_TRACKS; t++)
		{
		trm_ent = TRACKMAP_ENTRY_INIT(t);
		trackmap_entry_set_image_track(trm, &trm_ent, t);
		trackmap_entry_append(trm, &trm_ent);
		}
	}



/****************************************************************************
 * trackmap_check_index
 ****************************************************************************/
static cw_bool_t
trackmap_check_index(
	cw_index_t			index)

	{
	if ((index < 0) || (index >= GLOBAL_NR_TRACKS)) return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_check_cwtool_track
 ****************************************************************************/
static cw_bool_t
trackmap_check_cwtool_track(
	cw_count_t			cwtool_track)

	{
	if ((cwtool_track < 0) || (cwtool_track >= GLOBAL_NR_TRACKS)) return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_check_image_track
 ****************************************************************************/
static cw_bool_t
trackmap_check_image_track(
	cw_count_t			image_track)

	{
	if ((image_track < -1) || (image_track >= GLOBAL_NR_TRACKS)) return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_check_format_track
 ****************************************************************************/
static cw_bool_t
trackmap_check_format_track(
	cw_count_t			format_track)

	{
	if ((format_track < -1) || (format_track > 255)) return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_check_format_side
 ****************************************************************************/
static cw_bool_t
trackmap_check_format_side(
	cw_count_t			format_side)

	{
	if ((format_side < -1) || (format_side > 1)) return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * trackmap_get
 ****************************************************************************/
struct trackmap *
trackmap_get(
	cw_index_t			index)

	{
	static struct trackmap		trm[GLOBAL_NR_TRACKMAPS];
	static cw_count_t		trackmaps;
	static cw_bool_t		default_added = CW_BOOL_FALSE;

	if (! default_added)
		{
		default_added = CW_BOOL_TRUE;
		trackmap_add_default();
		}
	if (index == -1)
		{
		if (trackmaps >= GLOBAL_NR_TRACKMAPS) error_message("too many trackmaps defined");
		debug_message(GENERIC, 1, "request for unused trackmap struct, trackmaps = %d", trackmaps);
		return (&trm[trackmaps++]);
		}
	if ((index >= 0) && (index < trackmaps)) return (&trm[index]);
	return (NULL);
	}



/****************************************************************************
 * trackmap_search
 ****************************************************************************/
struct trackmap *
trackmap_search(
	const cw_char_t			*name)

	{
	struct trackmap			*trm;
	cw_index_t			i;

	for (i = 0; (trm = trackmap_get(i)) != NULL; i++) if (string_equal(name, trm->name)) break;
	return (trm);
	}



/****************************************************************************
 * trackmap_init
 ****************************************************************************/
cw_void_t
trackmap_init(
	struct trackmap			*trm,
	const cw_char_t			*name)

	{
	*trm = (struct trackmap)
		{
		.flags = TRACKMAP_FLAG_INITIALIZED
		};
	string_copy(trm->name, TRACKMAP_MAX_NAME_SIZE, name);
	}



/****************************************************************************
 * trackmap_deinit
 ****************************************************************************/
cw_void_t
trackmap_deinit(
	struct trackmap			*trm)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	*trm = (struct trackmap) { };
	}



/****************************************************************************
 * trackmap_entries
 ****************************************************************************/
cw_count_t
trackmap_entries(
	struct trackmap			*trm)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm->entries);
	}



/****************************************************************************
 * trackmap_full
 ****************************************************************************/
cw_bool_t
trackmap_full(
	struct trackmap			*trm)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (trm->entries >= TRACKMAP_NR_ENTRIES) return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * trackmap_cwtool_track_present
 ****************************************************************************/
cw_bool_t
trackmap_cwtool_track_present(
	struct trackmap			*trm,
	cw_count_t			cwtool_track)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_cwtool_track(cwtool_track)) return (CW_BOOL_FALSE);
	if (trm->ent_by_cwtool_track[cwtool_track] == NULL) return (CW_BOOL_FALSE);
	return (CW_BOOL_TRUE);
	}



/****************************************************************************
 * trackmap_cwtool_track_ok
 ****************************************************************************/
cw_bool_t
trackmap_cwtool_track_ok(
	struct trackmap			*trm,
	cw_count_t			cwtool_track)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_cwtool_track(cwtool_track)) return (CW_BOOL_FALSE);
	if (trackmap_cwtool_track_present(trm, cwtool_track)) return (CW_BOOL_FALSE);
	return (CW_BOOL_TRUE);
	}



/****************************************************************************
 * trackmap_image_track_present
 ****************************************************************************/
cw_bool_t
trackmap_image_track_present(
	struct trackmap			*trm,
	cw_count_t			image_track)

	{
	cw_count_t			i;

	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (image_track == -1) return (CW_BOOL_FALSE);
	for (i = 0; i < trm->entries; i++)
		{
		if (trm->ent_by_index[i].image_track == image_track) return (CW_BOOL_TRUE);
		}
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * trackmap_image_track_ok
 ****************************************************************************/
cw_bool_t
trackmap_image_track_ok(
	struct trackmap			*trm,
	cw_count_t			image_track)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_image_track(image_track)) return (CW_BOOL_FALSE);
	if (trackmap_image_track_present(trm, image_track)) return (CW_BOOL_FALSE);
	return (CW_BOOL_TRUE);
	}



/****************************************************************************
 * trackmap_entry_append
 ****************************************************************************/
cw_bool_t
trackmap_entry_append(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	cw_index_t			i;

	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (trackmap_full(trm)) return (CW_BOOL_FAIL);
	if (! trackmap_cwtool_track_ok(trm, trm_ent->cwtool_track)) return (CW_BOOL_FAIL);
	if (! trackmap_image_track_ok(trm, trm_ent->image_track))   return (CW_BOOL_FAIL);
	if (! trackmap_check_format_track(trm_ent->format_track))   return (CW_BOOL_FAIL);
	if (! trackmap_check_format_side(trm_ent->format_side))     return (CW_BOOL_FAIL);
	i = trm->entries++;
	trm->ent_by_index[i] = *trm_ent;
	trm->ent_by_index[i].index = i;
	trm->ent_by_cwtool_track[trm_ent->cwtool_track] = &trm->ent_by_index[i];
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_entry_get_by_index
 ****************************************************************************/
struct trackmap_entry *
trackmap_entry_get_by_index(
	struct trackmap			*trm,
	cw_index_t			index)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	error_condition((index < 0) || (index >= trm->entries));
	return (&trm->ent_by_index[index]);
	}



/****************************************************************************
 * trackmap_entry_get_by_cwtool_track
 ****************************************************************************/
struct trackmap_entry *
trackmap_entry_get_by_cwtool_track(
	struct trackmap			*trm,
	cw_count_t			cwtool_track)

	{
	debug_error_condition(trm == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	error_condition(! trackmap_check_cwtool_track(cwtool_track));
	return (trm->ent_by_cwtool_track[cwtool_track]);
	}



/****************************************************************************
 * trackmap_entry_check
 ****************************************************************************/
cw_bool_t
trackmap_entry_check(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_index(trm_ent->index))               return (CW_BOOL_FAIL);
	if (! trackmap_check_cwtool_track(trm_ent->cwtool_track)) return (CW_BOOL_FAIL);
	if (! trackmap_check_image_track(trm_ent->image_track))   return (CW_BOOL_FAIL);
	if (! trackmap_check_format_track(trm_ent->format_track)) return (CW_BOOL_FAIL);
	if (! trackmap_check_format_side(trm_ent->format_side))   return (CW_BOOL_FAIL);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_entry_set_image_track
 ****************************************************************************/
cw_bool_t
trackmap_entry_set_image_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			image_track)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_image_track_ok(trm, image_track)) return (CW_BOOL_FAIL);
	trm_ent->image_track = image_track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_entry_set_format_track
 ****************************************************************************/
cw_bool_t
trackmap_entry_set_format_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			format_track)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_format_track(format_track)) return (CW_BOOL_FAIL);
	trm_ent->format_track = format_track;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_entry_set_format_side
 ****************************************************************************/
cw_bool_t
trackmap_entry_set_format_side(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_count_t			format_side)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	if (! trackmap_check_format_side(format_side)) return (CW_BOOL_FAIL);
	trm_ent->format_side = format_side;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * trackmap_entry_get_index
 ****************************************************************************/
cw_index_t
trackmap_entry_get_index(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm_ent->index);
	}



/****************************************************************************
 * trackmap_entry_get_cwtool_track
 ****************************************************************************/
cw_count_t
trackmap_entry_get_cwtool_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm_ent->cwtool_track);
	}



/****************************************************************************
 * trackmap_entry_get_image_track
 ****************************************************************************/
cw_count_t
trackmap_entry_get_image_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm_ent->image_track);
	}



/****************************************************************************
 * trackmap_entry_get_format_track
 ****************************************************************************/
cw_count_t
trackmap_entry_get_format_track(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm_ent->format_track);
	}



/****************************************************************************
 * trackmap_entry_get_format_side
 ****************************************************************************/
cw_count_t
trackmap_entry_get_format_side(
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	debug_error_condition(trm == NULL);
	debug_error_condition(trm_ent == NULL);
	error_condition(! (trm->flags & TRACKMAP_FLAG_INITIALIZED));
	return (trm_ent->format_side);
	}
/******************************************************** Karsten Scheibler */

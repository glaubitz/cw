/****************************************************************************
 ****************************************************************************
 *
 * config/disk.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "disk.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../config.h"
#include "../disk.h"
#include "../format.h"
#include "../trackmap.h"
#include "../image.h"
#include "../string.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define SCOPE_ENTER			(1 << 0)
#define SCOPE_LEAVE			(1 << 1)
#define SCOPE_TRACK			(1 << 2)
#define SCOPE_RW			(1 << 3)
#define SCOPE_READ			(1 << 4)
#define SCOPE_WRITE			(1 << 5)
#define SCOPE_PARAMS			(1 << 6)

struct disk_params
	{
	int				ofs;
	int				size;
	int				type;
	struct format_option		*fmt_opt_rd;
	struct format_option		*fmt_opt_wr;
	struct format_option		*fmt_opt_rw;
	};




/****************************************************************************
 *
 * forward declarations
 *
 ****************************************************************************/




static cw_bool_t
config_disk_directive(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	struct disk_params		*dsk_prm,
	cw_flag_t			scope);




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_disk_error_timeout
 ****************************************************************************/
static cw_void_t
config_disk_error_timeout(
	struct config			*cfg)

	{
	config_error(cfg, "invalid timeout value");
	}



/****************************************************************************
 * config_disk_error_format
 ****************************************************************************/
static cw_void_t
config_disk_error_format(
	struct config			*cfg)

	{
	config_error(cfg, "no format specified");
	}



/****************************************************************************
 * config_disk_trackvalue
 ****************************************************************************/
static cw_count_t
config_disk_trackvalue(
	struct config			*cfg)

	{
	cw_count_t			track = config_number(cfg, NULL, 0);

	if (! disk_check_track(track)) config_error(cfg, "invalid track value");
	return (track);
	}



/****************************************************************************
 * config_disk_enter
 ****************************************************************************/
static cw_bool_t
config_disk_enter(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	struct disk_params		*dsk_prm,
	cw_flag_t			scope)

	{
	scope &= ~SCOPE_ENTER;
	scope |= SCOPE_LEAVE;
	while (config_disk_directive(cfg, dsk, dsk_trk, dsk_prm, scope)) ;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_info
 ****************************************************************************/
static cw_bool_t
config_disk_info(
	struct config			*cfg,
	struct disk			*dsk)

	{
	cw_char_t			info[GLOBAL_MAX_NAME_SIZE];

	config_string(cfg, info, sizeof (info));
	if (! disk_set_info(dsk, info)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_copy
 ****************************************************************************/
static cw_bool_t
config_disk_copy(
	struct config			*cfg,
	struct disk			*dsk)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct disk			*dsk2;

	dsk2 = disk_search(config_name(cfg, "disk name expected", token, sizeof (token)));
	if (dsk2 == NULL) config_error(cfg, "unknown disk name '%s'", token);
	if (! disk_copy(dsk, dsk2)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_image
 ****************************************************************************/
static cw_bool_t
config_disk_image(
	struct config			*cfg,
	struct disk			*dsk)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct image_desc		*img_dsc;

	img_dsc = image_search_desc(config_name(cfg, "image name expected", token, sizeof (token)));
	if (img_dsc == NULL) config_error(cfg, "unknown image name '%s'", token);
	if (! disk_set_image(dsk, img_dsc)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_trackmap
 ****************************************************************************/
static cw_bool_t
config_disk_trackmap(
	struct config			*cfg,
	struct disk			*dsk)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct trackmap			*trm;

	trm = trackmap_search(config_name(cfg, "trackmap name expected", token, sizeof (token)));
	if (trm == NULL) config_error(cfg, "unknown trackmap name '%s'", token);
	if (! disk_set_trackmap(dsk, trm)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_track
 ****************************************************************************/
static cw_bool_t
config_disk_track(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk)

	{
	struct disk_track		dsk_trk2;
	cw_count_t			cwtool_track;

	dsk_trk2     = *dsk_trk;
	cwtool_track = config_disk_trackvalue(cfg);
	config_disk_directive(cfg, NULL, &dsk_trk2, NULL, SCOPE_ENTER | SCOPE_RW);
	if (! disk_set_track(dsk, &dsk_trk2, cwtool_track)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_track_range
 ****************************************************************************/
static cw_bool_t
config_disk_track_range(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk)

	{
	struct disk_track		dsk_trk2;
	cw_count_t			cwtool_track, end, step;

	dsk_trk2     = *dsk_trk;
	cwtool_track = config_disk_trackvalue(cfg);
	end          = config_disk_trackvalue(cfg);
	step         = config_disk_trackvalue(cfg);
	if (step < 1) config_error(cfg, "invalid step size");
	config_disk_directive(cfg, NULL, &dsk_trk2, NULL, SCOPE_ENTER | SCOPE_RW);
	for ( ; cwtool_track <= end; cwtool_track += step) if (! disk_set_track(dsk, &dsk_trk2, cwtool_track)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_format
 ****************************************************************************/
static cw_bool_t
config_disk_format(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct format_desc		*fmt_dsc;

	fmt_dsc = format_search_desc(config_name(cfg, "format name expected", token, sizeof (token)));
	if (fmt_dsc == NULL) config_error(cfg, "unknown format '%s'", token);
	if (! disk_set_format(dsk_trk, fmt_dsc)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_clock
 ****************************************************************************/
static cw_bool_t
config_disk_clock(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_clock(dsk_trk, config_number(cfg, NULL, 0))) config_error(cfg, "invalid clock value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_timeout_read
 ****************************************************************************/
static cw_bool_t
config_disk_timeout_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_timeout_read(dsk_trk, config_number(cfg, NULL, 0))) config_disk_error_timeout(cfg);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_timeout_write
 ****************************************************************************/
static cw_bool_t
config_disk_timeout_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_timeout_write(dsk_trk, config_number(cfg, NULL, 0))) config_disk_error_timeout(cfg);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_timeout
 ****************************************************************************/
static cw_bool_t
config_disk_timeout(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	cw_count_t			val = config_number(cfg, NULL, 0);

	if (! disk_set_timeout_read(dsk_trk, val)) config_disk_error_timeout(cfg);
	if (! disk_set_timeout_write(dsk_trk, val)) config_disk_error_timeout(cfg);
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_indexed_read
 ****************************************************************************/
static cw_bool_t
config_disk_indexed_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_indexed_read(dsk_trk, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_indexed_write
 ****************************************************************************/
static cw_bool_t
config_disk_indexed_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_indexed_write(dsk_trk, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_indexed
 ****************************************************************************/
static cw_bool_t
config_disk_indexed(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	cw_bool_t			val = config_boolean(cfg, NULL, 0);

	if (! disk_set_indexed_read(dsk_trk, val)) debug_error();
	if (! disk_set_indexed_write(dsk_trk, val)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_optional
 ****************************************************************************/
static cw_bool_t
config_disk_optional(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_optional(dsk_trk, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_flip_side
 ****************************************************************************/
static cw_bool_t
config_disk_flip_side(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_flip_side(dsk_trk, config_boolean(cfg, NULL, 0))) debug_error();
	config_warning_obsolete(cfg, "flip_side");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_side_offset
 ****************************************************************************/
static cw_bool_t
config_disk_side_offset(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_side_offset(dsk_trk, config_number(cfg, NULL, 0))) debug_error();
	config_warning_obsolete(cfg, "side_offset");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_skew
 ****************************************************************************/
static cw_bool_t
config_disk_skew(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_skew(dsk_trk, config_number(cfg, NULL, 0))) config_error(cfg, "invalid skew value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_interleave
 ****************************************************************************/
static cw_bool_t
config_disk_interleave(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_interleave(dsk_trk, config_number(cfg, NULL, 0))) config_error(cfg, "invalid interleave value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_params
 ****************************************************************************/
static cw_bool_t
config_disk_params(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	struct disk_params		*dsk_prm,
	cw_char_t			*token,
	cw_count_t			len)

	{
	cw_count_t			val;
	cw_bool_t			rd = CW_BOOL_TRUE;
	cw_bool_t			wr = CW_BOOL_TRUE;
	cw_bool_t			rw = CW_BOOL_TRUE;

	if (dsk_prm->ofs >= dsk_prm->size) config_error(cfg, "too many parameters");
	if (dsk_prm->type == FORMAT_OPTION_TYPE_BOOLEAN) val = config_boolean(cfg, token, len);
	else val = config_number(cfg, token, len);
	if (dsk_prm->fmt_opt_rd != NULL) rd = disk_set_read_option(dsk_trk, dsk_prm->fmt_opt_rd, val, dsk_prm->ofs);
	if (dsk_prm->fmt_opt_wr != NULL) wr = disk_set_write_option(dsk_trk, dsk_prm->fmt_opt_wr, val, dsk_prm->ofs);
	if (dsk_prm->fmt_opt_rw != NULL) rw = disk_set_rw_option(dsk_trk, dsk_prm->fmt_opt_rw, val, dsk_prm->ofs);
	if ((! rd) || (! wr) || (! rw)) config_error(cfg, "invalid parameter value '%s'", token);
	dsk_prm->ofs++;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_set_params
 ****************************************************************************/
static cw_bool_t
config_disk_set_params(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	struct format_option		*fmt_opt_rd,
	struct format_option		*fmt_opt_wr,
	struct format_option		*fmt_opt_rw)
	

	{
	struct disk_params		dsk_prm =
		{
		.fmt_opt_rd = fmt_opt_rd,
		.fmt_opt_wr = fmt_opt_wr,
		.fmt_opt_rw = fmt_opt_rw
		};

	/*
	 * UGLY: better use functions from format.c than directly accessing
	 *       struct format_option ?
	 */

	if (fmt_opt_rw != NULL)
		{
		dsk_prm.size = fmt_opt_rw->params;
		dsk_prm.type = fmt_opt_rw->type;
		}
	else
		{
		dsk_prm.size = (fmt_opt_rd != NULL) ? fmt_opt_rd->params : fmt_opt_wr->params;
		dsk_prm.type = (fmt_opt_rd != NULL) ? fmt_opt_rd->type : fmt_opt_wr->type;
		}
	debug_error_condition((dsk_prm.type != FORMAT_OPTION_TYPE_BOOLEAN) && (dsk_prm.type != FORMAT_OPTION_TYPE_INTEGER));

	/*
	 * if number of parameters is -1, then set it to number of sectors,
	 * directive sector_sizes is the only user of this
	 */

	if (dsk_prm.size < 0) dsk_prm.size = disk_get_sectors(dsk_trk);
	debug_error_condition(dsk_prm.size > CONFIG_MAX_PARAMS);

	/*
	 * instead of using a loop here and get the parameters directly
	 * from the config, we jump back to config_disk_directive() and use
	 * a seperate struct
	 */

	config_disk_directive(cfg, NULL, dsk_trk, &dsk_prm, SCOPE_ENTER | SCOPE_PARAMS);
	if (dsk_prm.ofs < dsk_prm.size) config_error(cfg, "too few parameters");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_disk_read
 ****************************************************************************/
static cw_bool_t
config_disk_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	cw_char_t			*name)

	{
	struct format_option		*fmt_opt;

	fmt_opt = format_search_option(disk_get_read_options(dsk_trk), name);
	if (fmt_opt == NULL) return (CW_BOOL_FAIL);
	if (format_option_is_obsolete(fmt_opt)) config_warning_obsolete(cfg, name);
	return (config_disk_set_params(cfg, dsk_trk, fmt_opt, NULL, NULL));
	}



/****************************************************************************
 * config_disk_write
 ****************************************************************************/
static cw_bool_t
config_disk_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	cw_char_t			*name)

	{
	struct format_option		*fmt_opt;

	fmt_opt = format_search_option(disk_get_write_options(dsk_trk), name);
	if (fmt_opt == NULL) return (CW_BOOL_FAIL);
	if (format_option_is_obsolete(fmt_opt)) config_warning_obsolete(cfg, name);
	return (config_disk_set_params(cfg, dsk_trk, NULL, fmt_opt, NULL));
	}



/****************************************************************************
 * config_disk_rw
 ****************************************************************************/
static cw_bool_t
config_disk_rw(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	cw_char_t			*name)

	{
	struct format_option		*fmt_opt_rd, *fmt_opt_wr, *fmt_opt_rw;

	fmt_opt_rd = format_search_option(disk_get_read_options(dsk_trk), name);
	fmt_opt_wr = format_search_option(disk_get_write_options(dsk_trk), name);
	fmt_opt_rw = format_search_option(disk_get_rw_options(dsk_trk), name);
	if (fmt_opt_rw == NULL)
		{
		if ((fmt_opt_rd == NULL) || (fmt_opt_wr == NULL)) return (CW_BOOL_FAIL);
		if (format_option_is_obsolete(fmt_opt_rd) || format_option_is_obsolete(fmt_opt_wr)) config_warning_obsolete(cfg, name);

		/*
		 * UGLY: better use functions from format.c than directly
		 *       accessing struct format_option ?
		 */

		debug_error_condition(fmt_opt_rd->params != fmt_opt_wr->params);
		debug_error_condition(fmt_opt_rd->type != fmt_opt_wr->type);
		}
	else
		{
		if (format_option_is_obsolete(fmt_opt_rw)) config_warning_obsolete(cfg, name);
		debug_error_condition((fmt_opt_rd != NULL) || (fmt_opt_wr != NULL));
		}
	return (config_disk_set_params(cfg, dsk_trk, fmt_opt_rd, fmt_opt_wr, fmt_opt_rw));
	}



/****************************************************************************
 * config_disk_directive
 ****************************************************************************/
static cw_bool_t
config_disk_directive(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	struct disk_params		*dsk_prm,
	cw_flag_t			scope)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	cw_count_t			len;

	len = config_token(cfg, token, sizeof (token));
	if (len == 0)
		{
		if (scope & SCOPE_ENTER) config_error(cfg, "directive expected");
		config_error(cfg, "} expected");
		}
	if ((scope & SCOPE_ENTER) && (string_equal(token, "{"))) return (config_disk_enter(cfg, dsk, dsk_trk, dsk_prm, scope));
	if ((scope & SCOPE_LEAVE) && (string_equal(token, "}"))) return (CW_BOOL_FAIL);
	if (scope & SCOPE_TRACK)
		{
		if (string_equal(token, "info"))        return (config_disk_info(cfg, dsk));
		if (string_equal(token, "copy"))        return (config_disk_copy(cfg, dsk));
		if (string_equal(token, "image"))       return (config_disk_image(cfg, dsk));
		if (string_equal(token, "trackmap"))    return (config_disk_trackmap(cfg, dsk));
		if (string_equal(token, "track"))       return (config_disk_track(cfg, dsk, dsk_trk));
		if (string_equal(token, "track_range")) return (config_disk_track_range(cfg, dsk, dsk_trk));
		}
	if (scope & SCOPE_RW)
		{
		if (string_equal(token, "format"))       return (config_disk_format(cfg, dsk_trk));
		if (string_equal(token, "clock"))        return (config_disk_clock(cfg, dsk_trk));
		if (string_equal(token, "timeout"))      return (config_disk_timeout(cfg, dsk_trk));
		if (string_equal(token, "indexed"))      return (config_disk_indexed(cfg, dsk_trk));
		if (string_equal(token, "optional"))     return (config_disk_optional(cfg, dsk_trk));
		if (string_equal(token, "flip_side"))    return (config_disk_flip_side(cfg, dsk_trk));
		if (string_equal(token, "side_offset"))  return (config_disk_side_offset(cfg, dsk_trk));
		if (string_equal(token, "read"))         return (config_disk_directive(cfg, NULL, dsk_trk, NULL, SCOPE_ENTER | SCOPE_READ));
		if (string_equal(token, "write"))        return (config_disk_directive(cfg, NULL, dsk_trk, NULL, SCOPE_ENTER | SCOPE_WRITE));
		if (disk_get_format(dsk_trk) == NULL)    config_disk_error_format(cfg);
		if (string_equal(token, "skew"))         return (config_disk_skew(cfg, dsk_trk));
		if (string_equal(token, "interleave"))   return (config_disk_interleave(cfg, dsk_trk));
		if (config_disk_rw(cfg, dsk_trk, token)) return (CW_BOOL_OK);
		}
	if (scope & SCOPE_READ)
		{
		if (string_equal(token, "timeout"))        return (config_disk_timeout_read(cfg, dsk_trk));
		if (string_equal(token, "indexed"))        return (config_disk_indexed_read(cfg, dsk_trk));
		if (disk_get_format(dsk_trk) == NULL)      config_disk_error_format(cfg);
		if (config_disk_read(cfg, dsk_trk, token)) return (CW_BOOL_OK);
		}
	if (scope & SCOPE_WRITE)
		{
		if (string_equal(token, "timeout"))         return (config_disk_timeout_write(cfg, dsk_trk));
		if (string_equal(token, "indexed"))         return (config_disk_indexed_write(cfg, dsk_trk));
		if (disk_get_format(dsk_trk) == NULL)       config_disk_error_format(cfg);
		if (config_disk_write(cfg, dsk_trk, token)) return (CW_BOOL_OK);
		}
	if ((scope & SCOPE_PARAMS) && (config_disk_params(cfg, dsk_trk, dsk_prm, token, len))) return (CW_BOOL_OK);
	config_error_invalid(cfg, token);

	/* never reached, only to make gcc happy */

	return (CW_BOOL_FAIL);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_disk
 ****************************************************************************/
cw_bool_t
config_disk(
	struct config			*cfg,
	cw_count_t			revision)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct disk			dsk;

	disk_init(&dsk, revision);
	config_name(cfg, "disk name expected", token, sizeof (token));
	if (! disk_set_name(&dsk, token)) config_error(cfg, "already defined disk '%s' within this scope", token);
	config_disk_directive(cfg, &dsk, disk_init_track_default(&dsk), NULL, SCOPE_ENTER | SCOPE_TRACK | SCOPE_RW);
	if (! disk_tracks_used(&dsk)) config_error(cfg, "no tracks used in disk '%s'", token);
	if (! disk_image_ok(&dsk)) config_error(cfg, "specified format and image do not have the same level in disk '%s'", token);
	if (! disk_trackmap_ok(&dsk)) config_error(cfg, "trackmap incomplete or illegal use of side_offset or flip_side in disk '%s'", token);
	if (! disk_trackmap_numbering_ok(&dsk)) config_error(cfg, "image_track numbering in trackmap not compatible with selected image format in disk '%s'", token);
	disk_insert(&dsk);
	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

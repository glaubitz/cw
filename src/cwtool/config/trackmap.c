/****************************************************************************
 ****************************************************************************
 *
 * config/trackmap.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "trackmap.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../trackmap.h"
#include "../config.h"
#include "../string.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define SCOPE_ENTER			(1 << 0)
#define SCOPE_LEAVE			(1 << 1)
#define SCOPE_MAP			(1 << 2)
#define SCOPE_TRACK			(1 << 3)




/****************************************************************************
 *
 * forward declarations
 *
 ****************************************************************************/




static cw_bool_t
config_trackmap_directive(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_flag_t			scope);




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_trackmap_enter
 ****************************************************************************/
static cw_bool_t
config_trackmap_enter(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_flag_t			scope)

	{
	scope &= ~SCOPE_ENTER;
	scope |= SCOPE_LEAVE;
	while (config_trackmap_directive(cfg, trm, trm_ent, scope)) ;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_trackmap_track
 ****************************************************************************/
static cw_bool_t
config_trackmap_track(
	struct config			*cfg,
	struct trackmap			*trm)

	{
	struct trackmap_entry		trm_ent;
	cw_count_t			cwtool_track;

	cwtool_track = config_positive_number(cfg, NULL, 0);
	if (! trackmap_cwtool_track_ok(trm, cwtool_track)) config_error(cfg, "invalid track value");
	trm_ent = TRACKMAP_ENTRY_INIT(cwtool_track);
	config_trackmap_directive(cfg, trm, &trm_ent, SCOPE_ENTER | SCOPE_TRACK);
	if (! trackmap_entry_append(trm, &trm_ent)) config_error(cfg, "invalid trackmap entry");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_trackmap_image_track
 ****************************************************************************/
static cw_bool_t
config_trackmap_image_track(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	if (! trackmap_entry_set_image_track(trm, trm_ent, config_positive_number(cfg, NULL, 0))) config_error(cfg, "invalid image_track value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_trackmap_format_track
 ****************************************************************************/
static cw_bool_t
config_trackmap_format_track(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	if (! trackmap_entry_set_format_track(trm, trm_ent, config_positive_number(cfg, NULL, 0))) config_error(cfg, "invalid format_track value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_trackmap_format_side
 ****************************************************************************/
static cw_bool_t
config_trackmap_format_side(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent)

	{
	if (! trackmap_entry_set_format_side(trm, trm_ent, config_positive_number(cfg, NULL, 0))) config_error(cfg, "invalid format_side value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_trackmap_directive
 ****************************************************************************/
static cw_bool_t
config_trackmap_directive(
	struct config			*cfg,
	struct trackmap			*trm,
	struct trackmap_entry		*trm_ent,
	cw_flag_t			scope)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];

	if (config_token(cfg, token, sizeof (token)) == 0)
		{
		if (scope & SCOPE_ENTER) config_error(cfg, "directive expected");
		config_error(cfg, "} expected");
		}
	if ((scope & SCOPE_ENTER) && (string_equal(token, "{"))) return (config_trackmap_enter(cfg, trm, trm_ent, scope));
	if ((scope & SCOPE_LEAVE) && (string_equal(token, "}"))) return (CW_BOOL_FAIL);
	if (scope & SCOPE_MAP)
		{
		if (string_equal(token, "track")) return (config_trackmap_track(cfg, trm));
		}
	if (scope & SCOPE_TRACK)
		{
		if (string_equal(token, "image_track"))  return (config_trackmap_image_track(cfg, trm, trm_ent));
		if (string_equal(token, "format_track")) return (config_trackmap_format_track(cfg, trm, trm_ent));
		if (string_equal(token, "format_side"))  return (config_trackmap_format_side(cfg, trm, trm_ent));
		}
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
 * config_trackmap
 ****************************************************************************/
cw_bool_t
config_trackmap(
	struct config			*cfg,
	cw_count_t			revision)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	struct trackmap			trm;

	config_name(cfg, "trackmap name expected", token, sizeof (token));
	if (trackmap_search(token) != NULL) config_error(cfg, "already defined trackmap '%s' within this scope", token);
	trackmap_init(&trm, token);
	config_trackmap_directive(cfg, &trm, NULL, SCOPE_ENTER | SCOPE_MAP);
	*trackmap_get(-1) = trm;
	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

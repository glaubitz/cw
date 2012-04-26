/****************************************************************************
 ****************************************************************************
 *
 * config/options.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "options.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../config.h"
#include "../string.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define SCOPE_ENTER			(1 << 0)
#define SCOPE_LEAVE			(1 << 1)
#define SCOPE_FLAGS			(1 << 2)




/****************************************************************************
 *
 * forward declarations
 *
 ****************************************************************************/




static cw_bool_t
config_options_directive(
	struct config			*cfg,
	cw_flag_t			scope);




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_options_enter
 ****************************************************************************/
static cw_bool_t
config_options_enter(
	struct config			*cfg,
	cw_flag_t			scope)

	{
	scope &= ~SCOPE_ENTER;
	scope |= SCOPE_LEAVE;
	while (config_options_directive(cfg, scope)) ;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_histogram_exponential
 ****************************************************************************/
static cw_bool_t
config_options_histogram_exponential(
	struct config			*cfg)

	{
	if (! options_set_histogram_exponential(config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_histogram_context
 ****************************************************************************/
static cw_bool_t
config_options_histogram_context(
	struct config			*cfg)

	{
	if (! options_set_histogram_context(config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_always_initialize
 ****************************************************************************/
static cw_bool_t
config_options_always_initialize(
	struct config			*cfg)

	{
	if (! options_set_always_initialize(config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_clock_adjust
 ****************************************************************************/
static cw_bool_t
config_options_clock_adjust(
	struct config			*cfg)

	{
	if (! options_set_clock_adjust(config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_disk_track_start
 ****************************************************************************/
static cw_bool_t
config_options_disk_track_start(
	struct config			*cfg)

	{
	if (! options_set_disk_track_start(config_number(cfg, NULL, 0))) config_error(cfg, "invalid disk_track_start value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_disk_track_end
 ****************************************************************************/
static cw_bool_t
config_options_disk_track_end(
	struct config			*cfg)

	{
	if (! options_set_disk_track_end(config_number(cfg, NULL, 0))) config_error(cfg, "invalid disk_track_end value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_output_track_start
 ****************************************************************************/
static cw_bool_t
config_options_output_track_start(
	struct config			*cfg)

	{
	if (! options_set_output_track_start(config_number(cfg, NULL, 0))) config_error(cfg, "invalid output_track_start value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_output_track_end
 ****************************************************************************/
static cw_bool_t
config_options_output_track_end(
	struct config			*cfg)

	{
	if (! options_set_output_track_end(config_number(cfg, NULL, 0))) config_error(cfg, "invalid output_track_end value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_track_size_limit
 ****************************************************************************/
static cw_bool_t
config_options_track_size_limit(
	struct config			*cfg)

	{
	if (! options_set_track_size_limit(config_number(cfg, NULL, 0))) config_error(cfg, "invalid track_size_limit value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_options_directive
 ****************************************************************************/
static cw_bool_t
config_options_directive(
	struct config			*cfg,
	cw_flag_t			scope)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];

	if (config_token(cfg, token, sizeof (token)) == 0)
		{
		if (scope & SCOPE_ENTER) config_error(cfg, "directive expected");
		config_error(cfg, "} expected");
		}
	if ((scope & SCOPE_ENTER) && (string_equal(token, "{"))) return (config_options_enter(cfg, scope));
	if ((scope & SCOPE_LEAVE) && (string_equal(token, "}"))) return (CW_BOOL_FAIL);
	if (scope & SCOPE_FLAGS)
		{
		if (string_equal(token, "histogram_exponential")) return (config_options_histogram_exponential(cfg));
		if (string_equal(token, "histogram_context"))     return (config_options_histogram_context(cfg));
		if (string_equal(token, "always_initialize"))     return (config_options_always_initialize(cfg));
		if (string_equal(token, "clock_adjust"))          return (config_options_clock_adjust(cfg));
		if (string_equal(token, "disk_track_start"))      return (config_options_disk_track_start(cfg));
		if (string_equal(token, "disk_track_end"))        return (config_options_disk_track_end(cfg));
		if (string_equal(token, "output_track_start"))    return (config_options_output_track_start(cfg));
		if (string_equal(token, "output_track_end"))      return (config_options_output_track_end(cfg));
		if (string_equal(token, "track_size_limit"))      return (config_options_track_size_limit(cfg));
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
 * config_options
 ****************************************************************************/
cw_bool_t
config_options(
	struct config			*cfg)

	{
	config_options_directive(cfg, SCOPE_ENTER | SCOPE_FLAGS);
	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

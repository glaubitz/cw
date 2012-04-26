/****************************************************************************
 ****************************************************************************
 *
 * config/drive.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "drive.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../config.h"
#include "../drive.h"
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
config_drive_directive(
	struct config			*cfg,
	struct drive			*drv,
	cw_flag_t			scope);




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_drive_enter
 ****************************************************************************/
static cw_bool_t
config_drive_enter(
	struct config			*cfg,
	struct drive			*drv,
	cw_flag_t			scope)

	{
	scope &= ~SCOPE_ENTER;
	scope |= SCOPE_LEAVE;
	while (config_drive_directive(cfg, drv, scope)) ;
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_info
 ****************************************************************************/
static cw_bool_t
config_drive_info(
	struct config			*cfg,
	struct drive			*drv)

	{
	cw_char_t			info[GLOBAL_MAX_NAME_SIZE];

	config_string(cfg, info, sizeof (info));
	if (! drive_set_info(drv, info)) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_settle_time
 ****************************************************************************/
static cw_bool_t
config_drive_settle_time(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_settle_time(drv, config_number(cfg, NULL, 0))) config_error(cfg, "invalid settle_time value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_step_time
 ****************************************************************************/
static cw_bool_t
config_drive_step_time(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_step_time(drv, config_number(cfg, NULL, 0))) config_error(cfg, "invalid step_time value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_wpulse_length
 ****************************************************************************/
static cw_bool_t
config_drive_wpulse_length(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_wpulse_length(drv, config_number(cfg, NULL, 0))) config_error(cfg, "invalid wpulse_length value");
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_inverted_diskchange
 ****************************************************************************/
static cw_bool_t
config_drive_inverted_diskchange(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_inverted_diskchange(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_ignore_diskchange
 ****************************************************************************/
static cw_bool_t
config_drive_ignore_diskchange(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_ignore_diskchange(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_density
 ****************************************************************************/
static cw_bool_t
config_drive_density(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_density(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_double_step
 ****************************************************************************/
static cw_bool_t
config_drive_double_step(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_double_step(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * config_drive_directive
 ****************************************************************************/
static cw_bool_t
config_drive_directive(
	struct config			*cfg,
	struct drive			*drv,
	cw_flag_t			scope)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];

	if (config_token(cfg, token, sizeof (token)) == 0)
		{
		if (scope & SCOPE_ENTER) config_error(cfg, "directive expected");
		config_error(cfg, "} expected");
		}
	if ((scope & SCOPE_ENTER) && (string_equal(token, "{"))) return (config_drive_enter(cfg, drv, scope));
	if ((scope & SCOPE_LEAVE) && (string_equal(token, "}"))) return (CW_BOOL_FAIL);
	if (scope & SCOPE_FLAGS)
		{
		if (string_equal(token, "info"))                return (config_drive_info(cfg, drv));
		if (string_equal(token, "settle_time"))         return (config_drive_settle_time(cfg, drv));
		if (string_equal(token, "step_time"))           return (config_drive_step_time(cfg, drv));
		if (string_equal(token, "wpulse_length"))       return (config_drive_wpulse_length(cfg, drv));
		if (string_equal(token, "inverted_diskchange")) return (config_drive_inverted_diskchange(cfg, drv));
		if (string_equal(token, "ignore_diskchange"))   return (config_drive_ignore_diskchange(cfg, drv));
		if (string_equal(token, "density"))             return (config_drive_density(cfg, drv));
		if (string_equal(token, "double_step"))         return (config_drive_double_step(cfg, drv));
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
 * config_drive
 ****************************************************************************/
cw_bool_t
config_drive(
	struct config			*cfg,
	cw_count_t			revision)

	{
	cw_char_t			token[GLOBAL_MAX_PATH_SIZE];
	struct drive			drv;

	drive_init(&drv, revision);
	config_path(cfg, "drive path expected", token, sizeof (token));
	if (! drive_set_path(&drv, token)) config_error(cfg, "already defined drive '%s' within this scope", token);
	config_drive_directive(cfg, &drv, SCOPE_ENTER | SCOPE_FLAGS);
	drive_insert(&drv);
	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

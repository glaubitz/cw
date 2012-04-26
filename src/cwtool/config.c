/****************************************************************************
 ****************************************************************************
 *
 * config.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdarg.h>

#include "config.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "parse.h"
#include "string.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_top_directive
 ****************************************************************************/
static cw_bool_t
config_top_directive(
	struct config			*cfg,
	cw_count_t			revision)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];

	debug_message(GENERIC, 2, "looking for next top directive, revision = %d", revision);
	if (! config_token(cfg, token, sizeof (token))) return (CW_BOOL_FAIL);
	if (string_equal(token, "disk"))     return (config_disk(cfg, revision));
	if (string_equal(token, "drive"))    return (config_drive(cfg, revision));
	if (string_equal(token, "options"))  return (config_options(cfg));
	if (string_equal(token, "trackmap")) return (config_trackmap(cfg, revision));
	config_error_invalid(cfg, token);

	/* never reached, only to make gcc happy */

	return (CW_BOOL_FAIL);
	}



/****************************************************************************
 * config_parse
 ****************************************************************************/
static cw_void_t
config_parse(
	struct config			*cfg,
	const cw_char_t			*path)

	{
	static cw_count_t		revision;

	revision++;
	verbose_message(GENERIC, 1, "reading config from '%s'", path);
	while (config_top_directive(cfg, revision)) ;
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_default
 ****************************************************************************/
const cw_char_t *
config_default(
	cw_index_t			offset)

	{
	static const cw_char_t		string[] = "\n\n"
					#include "cwtoolrc.c"
					;

	/*
	 * the two first \n are important to keep line numbers with
	 * cwtool_dump()
	 */

	error_condition((offset != 0) && (offset != 1));
	return (&string[offset]);
	}



/****************************************************************************
 * config_warning
 ****************************************************************************/
cw_void_t
config_warning(
	struct config			*cfg,
	const cw_char_t			*format,
	...)

	{
	cw_char_t			message[4096];

	string_vsnprintf(message, sizeof (message), format);
	parse_warning(&cfg->prs, "%s", message);
	}



/****************************************************************************
 * config_warning_obsolete
 ****************************************************************************/
cw_void_t
config_warning_obsolete(
	struct config			*cfg,
	const cw_char_t			*token)

	{
/*	parse_warning_obsolete(&cfg->prs, token);*/
	}



/****************************************************************************
 * config_error
 ****************************************************************************/
cw_void_t
config_error(
	struct config			*cfg,
	const cw_char_t			*format,
	...)

	{
	cw_char_t			message[4096];

	string_vsnprintf(message, sizeof (message), format);
	parse_error(&cfg->prs, "%s", message);
	}



/****************************************************************************
 * config_error_invalid
 ****************************************************************************/
cw_void_t
config_error_invalid(
	struct config			*cfg,
	const cw_char_t			*token)

	{
	parse_error_invalid(&cfg->prs, token);
	}



/****************************************************************************
 * config_token
 ****************************************************************************/
cw_count_t
config_token(
	struct config			*cfg,
	cw_char_t			*token,
	cw_size_t			size)

	{
	return (parse_token(&cfg->prs, token, size));
	}



/****************************************************************************
 * config_string
 ****************************************************************************/
cw_count_t
config_string(
	struct config			*cfg,
	cw_char_t			*string,
	cw_size_t			size)

	{
	return (parse_string(&cfg->prs, string, size));
	}



/****************************************************************************
 * config_name
 ****************************************************************************/
cw_char_t *
config_name(
	struct config			*cfg,
	const cw_char_t			*error,
	cw_char_t			*token,
	cw_size_t			size)

	{
	static const cw_char_t		valid[] = "_.-+";
	cw_char_t			c;
	cw_index_t			i, j;

	if (! config_string(cfg, token, size)) config_error(cfg, error);
	for (i = 0; token[i] != '\0'; i++)
		{
		c = token[i];
		if ((c >= 'a') && (c <= 'z')) continue;
		if ((c >= 'A') && (c <= 'Z')) continue;
		for (j = 0; valid[j] != '\0'; j++) if (c == valid[j]) break;
		if (valid[j] != '\0') continue;
		if ((i > 0) && (c >= '0') && (c <= '9')) continue;
		config_error(cfg, "invalid character in name");
		}
	return (token);
	}




/****************************************************************************
 * config_path
 ****************************************************************************/
cw_char_t *
config_path(
	struct config			*cfg,
	const cw_char_t			*error,
	cw_char_t			*token,
	cw_size_t			size)

	{
	static const cw_char_t		valid[] = "_/.-";
	cw_char_t			c;
	cw_index_t			i, j;

	if (! config_string(cfg, token, size)) config_error(cfg, error);
	if (token[0] != '/') config_error(cfg, "path has to start with '/'"); 
	for (i = 1; token[i] != '\0'; i++)
		{
		c = token[i];
		if ((c >= '0') && (c <= '9')) continue;
		if ((c >= 'a') && (c <= 'z')) continue;
		if ((c >= 'A') && (c <= 'Z')) continue;
		for (j = 0; valid[j] != '\0'; j++) if (c == valid[j]) break;
		if (valid[j] != '\0') continue;
		config_error(cfg, "invalid character in path");
		}
	return (token);
	}



/****************************************************************************
 * config_number
 ****************************************************************************/
cw_s32_t
config_number(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len)

	{
	return (parse_number(&cfg->prs, token, len));
	}



/****************************************************************************
 * config_positive_number
 ****************************************************************************/
cw_s32_t
config_positive_number(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len)

	{
	return (parse_number_range(&cfg->prs, token, len, 0, 0x7fffffff));
	}



/****************************************************************************
 * config_boolean
 ****************************************************************************/
cw_bool_t
config_boolean(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len)

	{
	return (parse_boolean(&cfg->prs, token, len));
	}



/****************************************************************************
 * config_parse_memory
 ****************************************************************************/
cw_void_t
config_parse_memory(
	const cw_char_t			*path,
	const cw_char_t			*text,
	cw_size_t			size)

	{
	struct config			cfg;

	parse_init_memory(
		&cfg.prs,
		parse_valid_chars_config(),
		path,
		text,
		size);
	config_parse(&cfg, path);
	}



/****************************************************************************
 * config_parse_path
 ****************************************************************************/
cw_void_t
config_parse_path(
	const cw_char_t			*path,
	cw_bool_t			file_may_not_exist)

	{
	struct config			cfg;
	cw_bool_t			success;

	success = parse_init_path(
		&cfg.prs,
		parse_valid_chars_config(),
		path,
		file_may_not_exist);
	if (success) config_parse(&cfg, path);
	}
/******************************************************** Karsten Scheibler */

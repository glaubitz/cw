/****************************************************************************
 ****************************************************************************
 *
 * config.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_CONFIG_H
#define CWTOOL_CONFIG_H

#include "types.h"
#include "global.h"
#include "parse.h"
#include "config/disk.h"
#include "config/drive.h"
#include "config/options.h"
#include "config/trackmap.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define CONFIG_MAX_PARAMS		GLOBAL_NR_SECTORS


struct config
	{
	struct parse			prs;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern const cw_char_t *
config_default(
	cw_index_t			offset);


extern cw_void_t
config_warning(
	struct config			*cfg,
	const cw_char_t			*format,
	...);

extern cw_void_t
config_warning_obsolete(
	struct config			*cfg,
	const cw_char_t			*token);

extern cw_void_t
config_error(
	struct config			*cfg,
	const cw_char_t			*format,
	...);

extern cw_void_t
config_error_invalid(
	struct config			*cfg,
	const cw_char_t			*token);

extern cw_count_t
config_token(
	struct config			*cfg,
	cw_char_t			*token,
	cw_size_t			size);

extern cw_count_t
config_string(
	struct config			*cfg,
	cw_char_t			*string,
	cw_size_t			size);

extern cw_char_t *
config_name(
	struct config			*cfg,
	const cw_char_t			*error,
	cw_char_t			*token,
	cw_size_t			size);

extern cw_char_t *
config_path(
	struct config			*cfg,
	const cw_char_t			*error,
	cw_char_t			*token,
	cw_size_t			size);

extern cw_s32_t
config_number(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len);

extern cw_s32_t
config_positive_number(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len);

extern cw_bool_t
config_boolean(
	struct config			*cfg,
	cw_char_t			*token,
	cw_count_t			len);

extern cw_void_t
config_parse_memory(
	const cw_char_t			*path,
	const cw_char_t			*text,
	cw_size_t			size);

extern cw_void_t
config_parse_path(
	const cw_char_t			*path,
	cw_bool_t			file_may_not_exist);



#endif /* !CWTOOL_CONFIG_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * parse.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_PARSE_H
#define CWTOOL_PARSE_H

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct file;

#define PARSE_TEXT_SIZE			0x40000

#define PARSE_MODE_MEMORY		1
#define PARSE_MODE_FILE			2
#define PARSE_MODE_PATH			3

#define PARSE_FLAG_INITIALIZED		(1 << 0)
#define PARSE_FLAG_HEX_ONLY		(1 << 1)

struct parse
	{
	const cw_char_t			*valid_chars;
	struct file			*fil;
	const cw_char_t			*path;
	cw_char_t			*text;
	cw_size_t			size;
	cw_count_t			limit;
	cw_index_t			ofs;
	cw_count_t			line;
	cw_count_t			line_ofs;
	cw_count_t			max_number;
	cw_mode_t			mode;
	cw_flag_t			flags;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern const cw_char_t *
parse_valid_chars_config(
	cw_void_t);

extern const cw_char_t *
parse_valid_chars_raw_text(
	cw_void_t);

extern cw_void_t
parse_init_memory(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	const cw_char_t			*path,
	const cw_char_t			*text,
	cw_size_t			size);

extern cw_void_t
parse_init_file(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	struct file			*file);

extern cw_bool_t
parse_init_path(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	const cw_char_t			*path,
	cw_bool_t			file_may_not_exist);

extern cw_void_t
parse_deinit(
	struct parse			*prs);

extern cw_void_t
parse_hex_only(
	struct parse			*prs,
	cw_bool_t			hex_only);

extern cw_void_t
parse_max_number(
	struct parse			*prs,
	cw_count_t			max_number);

extern cw_void_t
parse_flush_text_buffer(
	struct parse			*prs);

extern cw_void_t
parse_fill_text_buffer(
	struct parse			*prs,
	const cw_char_t			*text,
	cw_size_t			size);

extern cw_void_t
parse_warning(
	struct parse			*prs,
	const cw_char_t			*format,
	...);

extern cw_void_t
parse_warning_obsolete(
	struct parse			*prs,
	const cw_char_t			*token);

extern cw_void_t
parse_error(
	struct parse			*prs,
	const cw_char_t			*format,
	...);

extern cw_void_t
parse_error_invalid(
	struct parse			*prs,
	const cw_char_t			*token);

extern cw_count_t
parse_token(
	struct parse			*prs,
	cw_char_t			*token,
	cw_size_t			token_size);

extern cw_count_t
parse_string(
	struct parse			*prs,
	cw_char_t			*string,
	cw_size_t			string_size);

extern cw_s32_t
parse_number(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len);

extern cw_s32_t
parse_number_range(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len,
	cw_s32_t			min,
	cw_s32_t			max);

extern cw_bool_t
parse_boolean(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len);



#endif /* !CWTOOL_PARSE_H */
/******************************************************** Karsten Scheibler */

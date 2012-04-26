/****************************************************************************
 ****************************************************************************
 *
 * parse.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "parse.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "file.h"
#include "string.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * parse_is_space
 ****************************************************************************/
static cw_bool_t
parse_is_space(
	cw_char_t			c)

	{
	if ((c == '\t') || (c == '\n') || (c == '\r') || (c == ' ')) return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * parse_get_char
 ****************************************************************************/
static cw_char_t
parse_get_char(
	struct parse			*prs,
	cw_bool_t			allow_all_chars)

	{
	cw_char_t			c;
	cw_index_t			i;

	if (prs->ofs >= prs->limit)
		{
		if (prs->mode == PARSE_MODE_MEMORY) return ('\0'); 
		prs->limit = file_read(prs->fil, prs->text, prs->size);
		prs->ofs = 0;
		if (prs->limit == 0) return ('\0');
		}
	c = prs->text[prs->ofs++];
	if (c == '\0') parse_error(prs, "got \\0 character");
	if (allow_all_chars) goto ok;
	if ((c >= '0') && (c <= '9')) goto ok;
	if ((c >= 'A') && (c <= 'Z')) goto ok;
	if ((c >= 'a') && (c <= 'z')) goto ok;
	for (i = 0; prs->valid_chars[i] != '\0'; i++)
		{
		if (c == prs->valid_chars[i]) goto ok;
		}
	parse_error(prs, "illegal character");
ok:
	if (c == '\t') prs->line_ofs += 8;
	else prs->line_ofs++;
	if (c == '\n') prs->line++, prs->line_ofs = 0;
	return (c);
	}



/****************************************************************************
 * parse_comment
 ****************************************************************************/
static cw_bool_t
parse_comment(
	cw_char_t			c,
	cw_bool_t			*comment)

	{
	if ((*comment) && (c != '\0') && (c != '\n')) return (CW_BOOL_TRUE);
	*comment = CW_BOOL_FALSE;
	if (c != '#') return (CW_BOOL_FALSE);
	*comment = CW_BOOL_TRUE;
	return (CW_BOOL_TRUE);
	}



/****************************************************************************
 * parse_number_hex
 ****************************************************************************/
static cw_s32_t
parse_number_hex(
	struct parse			*prs,
	cw_char_t			*token)

	{
	cw_char_t			c;
	cw_s32_t			num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c >= '0') && (c <= '9')) c -= '0';
		else if ((c >= 'a') && (c <= 'f')) c -= 'a' - 10;
		else parse_error(prs, "not a hexadecimal number");
		if ((num > (0x7fffffff - 15) / 16) || ((prs->max_number > 0) && (num > prs->max_number)))
			{
			parse_error(prs, "hexadecimal number too large");
			}
		num = 16 * num + c;
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * parse_number_dec
 ****************************************************************************/
static cw_s32_t
parse_number_dec(
	struct parse			*prs,
	cw_char_t			*token)

	{
	cw_char_t			c;
	cw_s32_t			num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c < '0') || (c > '9')) parse_error(prs, "not a decimal number");
		if ((num > (0x7fffffff - 9) / 10) || ((prs->max_number > 0) && (num > prs->max_number)))
			{
			parse_error(prs, "decimal number too large");
			}
		num = 10 * num + c - '0';
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * parse_number_bin
 ****************************************************************************/
static cw_s32_t
parse_number_bin(
	struct parse			*prs,
	char				*token)

	{
	cw_char_t			c;
	cw_s32_t			num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c < '0') || (c > '1')) parse_error(prs, "not a binary number");
		if ((num > (0x7fffffff - 1) / 2) || ((prs->max_number > 0) && (num > prs->max_number)))
			{
			parse_error(prs, "binary number too large");
			}
		num = 2 * num + c - '0';
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * parse_init_file2
 ****************************************************************************/
static cw_void_t
parse_init_file2(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	struct file			*fil,
	cw_mode_t			mode)

	{
	*prs = (struct parse)
		{
		.valid_chars = valid_chars,
		.fil         = fil,
		.path        = file_get_path(fil),
		.text        = malloc(PARSE_TEXT_SIZE * sizeof (cw_char_t)),
		.size        = PARSE_TEXT_SIZE,
		.mode        = mode,
		.flags       = PARSE_FLAG_INITIALIZED
		};
	if (prs->text == NULL) error_oom();
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * parse_valid_chars_config
 ****************************************************************************/
const cw_char_t *
parse_valid_chars_config(
	cw_void_t)

	{
	return ("\n\r\t _{}#$%/+-\"");
	}



/****************************************************************************
 * parse_valid_chars_raw_text
 ****************************************************************************/
const cw_char_t *
parse_valid_chars_raw_text(
	cw_void_t)

	{
	return ("\n\r\t _{}#$%");
	}



/****************************************************************************
 * parse_init_memory
 ****************************************************************************/
cw_void_t
parse_init_memory(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	const cw_char_t			*path,
	const cw_char_t			*text,
	cw_size_t			size)

	{
	*prs = (struct parse)
		{
		.valid_chars = valid_chars,
		.path        = path,
		.text        = (char *) text,
		.size        = size,
		.limit       = size,
		.mode        = PARSE_MODE_MEMORY,
		.flags       = PARSE_FLAG_INITIALIZED
		};
	}



/****************************************************************************
 * parse_init_file
 ****************************************************************************/
cw_void_t
parse_init_file(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	struct file			*fil)

	{
	parse_init_file2(prs, valid_chars, fil, PARSE_MODE_FILE);
	}



/****************************************************************************
 * parse_init_path
 ****************************************************************************/
cw_bool_t
parse_init_path(
	struct parse			*prs,
	const cw_char_t			*valid_chars,
	const cw_char_t			*path,
	cw_bool_t			file_may_not_exist)

	{
	struct file			*fil;
	cw_flag_t			flags = FILE_FLAG_NONE;
	cw_bool_t			success;

	fil = malloc(sizeof (struct file));
	if (fil == NULL) error_oom();
	if (file_may_not_exist) flags = FILE_FLAG_RETURN;
	success = file_open(fil, path, FILE_MODE_READ, flags);
	if (success) parse_init_file2(prs, valid_chars, fil, PARSE_MODE_PATH);
	else free(fil);
	return (success);
	}



/****************************************************************************
 * parse_deinit
 ****************************************************************************/
cw_void_t
parse_deinit(
	struct parse			*prs)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	if ((prs->mode == PARSE_MODE_FILE) || (prs->mode == PARSE_MODE_PATH)) free(prs->text);
	if (prs->mode == PARSE_MODE_PATH)
		{
		file_close(prs->fil);
		free(prs->fil);
		}
	*prs = (struct parse) { };
	}



/****************************************************************************
 * parse_hex_only
 ****************************************************************************/
cw_void_t
parse_hex_only(
	struct parse			*prs,
	cw_bool_t			hex_only)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	if (hex_only) prs->flags |= PARSE_FLAG_HEX_ONLY;
	else prs->flags &= ~PARSE_FLAG_HEX_ONLY;
	}



/****************************************************************************
 * parse_max_number
 ****************************************************************************/
cw_void_t
parse_max_number(
	struct parse			*prs,
	cw_count_t			max_number)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	prs->max_number = max_number;
	}



/****************************************************************************
 * parse_flush_text_buffer
 ****************************************************************************/
cw_void_t
parse_flush_text_buffer(
	struct parse			*prs)

	{
	cw_count_t			remaining, ofs;

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	error_condition((prs->mode != PARSE_MODE_FILE) && (prs->mode != PARSE_MODE_PATH));
	remaining = prs->limit - prs->ofs;
	ofs = file_seek(prs->fil, -1, FILE_FLAG_NONE) - remaining;
	file_seek(prs->fil, ofs, FILE_FLAG_NONE);
	prs->ofs = 0;
	prs->limit = 0;
	}



/****************************************************************************
 * parse_fill_text_buffer
 ****************************************************************************/
cw_void_t
parse_fill_text_buffer(
	struct parse			*prs,
	const cw_char_t			*text,
	cw_size_t			size)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	error_condition((prs->mode != PARSE_MODE_FILE) && (prs->mode != PARSE_MODE_PATH));
	error_condition(prs->limit != 0);
	error_condition(prs->size < size);
	memcpy(prs->text, text, size);
	prs->limit = size;
	}



/****************************************************************************
 * parse_warning
 ****************************************************************************/
cw_void_t
parse_warning(
	struct parse			*prs,
	const cw_char_t			*format,
	...)

	{
	cw_char_t			message[4096];

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	string_vsnprintf(message, sizeof (message), format);
	error_warning("parse warning in '%s' around line %d, char %d",
		prs->path,
		prs->line + 1,
		prs->line_ofs + 1);
	error_warning("%s", message);
	}



/****************************************************************************
 * parse_warning_obsolete
 ****************************************************************************/
cw_void_t
parse_warning_obsolete(
	struct parse			*prs,
	const cw_char_t			*token)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	parse_warning(prs, "using obsolete directive '%s'", token);
	}



/****************************************************************************
 * parse_error
 ****************************************************************************/
cw_void_t
parse_error(
	struct parse			*prs,
	const cw_char_t			*format,
	...)

	{
	cw_char_t			message[4096];

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	string_vsnprintf(message, sizeof (message), format);
	error_warning("parse error in '%s' around line %d, char %d",
		prs->path,
		prs->line + 1,
		prs->line_ofs + 1);
	error_message("%s", message);
	}



/****************************************************************************
 * parse_error_invalid
 ****************************************************************************/
cw_void_t
parse_error_invalid(
	struct parse			*prs,
	const cw_char_t			*token)

	{
	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	parse_error(prs, "invalid directive '%s' within this scope", token);
	}



/****************************************************************************
 * parse_token
 ****************************************************************************/
cw_count_t
parse_token(
	struct parse			*prs,
	cw_char_t			*token,
	cw_size_t			token_size)

	{
	cw_bool_t			comment = CW_BOOL_FALSE;
	cw_char_t			c;
	cw_index_t			i;

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	for (i = 0, token[0] = '\0'; i < token_size - 1; )
		{
		c = parse_get_char(prs, comment);
		if (parse_comment(c, &comment)) continue;
		if ((c == '\0') || (parse_is_space(c)))
			{
			if ((c != '\0') && (i == 0)) continue;
			debug_message(GENERIC, 3, "got token '%s'", token);
			return (i);
			}
		token[i++] = c;
		token[i] = '\0';
		}
	parse_error(prs, "token too long");

	/* never reached, only to make gcc happy */

	return (0);
	}



/****************************************************************************
 * parse_string
 ****************************************************************************/
cw_count_t
parse_string(
	struct parse			*prs,
	cw_char_t			*string,
	cw_size_t			string_size)

	{
	cw_bool_t			comment = CW_BOOL_FALSE;
	cw_bool_t			quote = CW_BOOL_FALSE;
	cw_char_t			c;
	cw_index_t			i;

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	for (i = 0, string[0] = '\0'; i < string_size - 1; )
		{
		c = parse_get_char(prs, comment | quote);
		if ((! comment) && (c == '"'))
			{
			if (quote) return (i);
			quote = CW_BOOL_TRUE;
			continue;
			}
		if (! quote)
			{
			if (parse_comment(c, &comment)) continue;
			if (parse_is_space(c)) continue;
			c = '\0';
			}
		if (c == '\0') parse_error(prs, "\" expected");
		if (c == '\n') parse_error(prs, "string exceeds line");
		string[i++] = c;
		string[i] = '\0';
		}
	parse_error(prs, "string too long");

	/* never reached, only to make gcc happy */

	return (0);
	}



/****************************************************************************
 * parse_number
 ****************************************************************************/
cw_s32_t
parse_number(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len)

	{
	cw_char_t			token2[GLOBAL_MAX_NAME_SIZE];
	cw_s32_t			num, sign = 0;

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	if (token == NULL) token = token2, len = parse_token(prs, token2, sizeof (token2));
	if (len <= 0) parse_error(prs, "number expected");
	if ((len > 1) && (token[0] == '-')) sign = -1;
	if ((len > 1) && (token[0] == '+')) sign = 1;
	if (sign != 0) len--, token++;
	if (prs->flags & PARSE_FLAG_HEX_ONLY) num = parse_number_hex(prs, token);
	else if ((len > 2) && (token[0] == '0') && (token[1] == 'x')) num = parse_number_hex(prs, &token[2]);
	else if ((len > 1) && (token[0] == '$')) num = parse_number_hex(prs, &token[1]);
	else if ((len > 1) && (token[0] == '%')) num = parse_number_bin(prs, &token[1]);
	else num = parse_number_dec(prs, token);
	if (sign != 0) num = sign * num;
	debug_message(GENERIC, 3, "got number %d == 0x%x", num, num);
	return (num);
	}



/****************************************************************************
 * parse_number_range
 ****************************************************************************/
cw_s32_t
parse_number_range(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len,
	cw_s32_t			min,
	cw_s32_t			max)

	{
	cw_s32_t			num = parse_number(prs, token, len);

	if (num < min) parse_error(prs, "number smaller than %d", min);
	if (num > max) parse_error(prs, "number greater than %d", max);
	return (num);
	}



/****************************************************************************
 * parse_boolean
 ****************************************************************************/
cw_bool_t
parse_boolean(
	struct parse			*prs,
	cw_char_t			*token,
	cw_count_t			len)

	{
	cw_char_t			token2[GLOBAL_MAX_NAME_SIZE];
	cw_s32_t			num;

	error_condition(! (prs->flags & PARSE_FLAG_INITIALIZED));
	if (token == NULL) token = token2, len = parse_token(prs, token2, sizeof (token2));
	if (len <= 0) parse_error(prs, "boolean expected");
	if ((string_equal(token, "no")) || (string_equal(token, "off"))) return (CW_BOOL_FALSE);
	if ((string_equal(token, "yes")) || (string_equal(token, "on"))) return (CW_BOOL_TRUE);
	num = parse_number(prs, token, len);
	if ((num < 0) || (num > 1)) parse_error(prs, "not a boolean value '%s'", token);
	if (num == 1) return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}
/******************************************************** Karsten Scheibler */

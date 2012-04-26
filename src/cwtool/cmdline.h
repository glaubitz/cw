/****************************************************************************
 ****************************************************************************
 *
 * cmdline.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_CMDLINE_H
#define CWTOOL_CMDLINE_H

#include "types.h"
#include "global.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define CMDLINE_MODE_DEFAULT		CW_MODE_NONE
#define CMDLINE_MODE_VERSION		1
#define CMDLINE_MODE_DUMP		2
#define CMDLINE_MODE_INITIALIZE		3
#define CMDLINE_MODE_LIST		4
#define CMDLINE_MODE_STATISTICS		5
#define CMDLINE_MODE_READ		6
#define CMDLINE_MODE_WRITE		7

#define CMDLINE_NR_CONFIGS		128

#define CMDLINE_CONFIG_TYPE_FILE	1
#define CMDLINE_CONFIG_TYPE_EVALUATE	2

struct cmdline_config
	{
	cw_type_t			type;
	char				*data;
	};

#define CMDLINE_FLAG_NO_RCFILES		(1 << 0)
#define CMDLINE_FLAG_IGNORE_SIZE	(1 << 1)

struct cmdline
	{
	cw_mode_t			mode;
	cw_flag_t			flags;
	cw_count_t			retry;
	cw_char_t			*disk_name;
	cw_char_t			*file[GLOBAL_NR_IMAGES];
	cw_count_t			files;
	cw_char_t			*output;
	struct cmdline_config		cfg[CMDLINE_NR_CONFIGS];
	cw_count_t			configs;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_bool_t
cmdline_parse(
	cw_char_t			**argv);

extern cw_mode_t
cmdline_get_mode(
	cw_void_t);

extern cw_bool_t
cmdline_get_flag(
	cw_flag_t			mask);

extern cw_count_t
cmdline_get_retry(
	cw_void_t);

extern cw_char_t *
cmdline_get_output(
	cw_void_t);

extern cw_char_t *
cmdline_get_disk_name(
	cw_void_t);

extern cw_char_t *
cmdline_get_file(
	cw_index_t			index);

extern cw_char_t **
cmdline_get_all_files(
	cw_void_t);

extern cw_count_t
cmdline_get_files(
	cw_void_t);

extern cw_bool_t
cmdline_read_config(
	cw_void_t);



#endif /* !CWTOOL_CMDLINE_H */
/******************************************************** Karsten Scheibler */

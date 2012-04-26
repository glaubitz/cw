/****************************************************************************
 ****************************************************************************
 *
 * cmdline.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdlib.h>

#include "cmdline.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "string.h"
#include "config.h"
#include "file.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




static struct cmdline			cmd = { .retry = 5 };




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * cmdline_fill_space
 ****************************************************************************/
static cw_void_t
cmdline_fill_space(
	cw_char_t			*string)

	{
	while (*string != '\0') *string++ = ' ';
	}



/****************************************************************************
 * cmdline_usage
 ****************************************************************************/
static cw_void_t
cmdline_usage(
	cw_void_t)

	{
	cw_char_t			space1[] = GLOBAL_VERSION_STRING;
	cw_char_t			space2[] = GLOBAL_PROGRAM_NAME;

	cmdline_fill_space(space1);
	cmdline_fill_space(space2);
	printf("%s | written by Karsten Scheibler\n"
		"%s | http://unusedino.de/cw/\n"
		"%s | cw@unusedino.de\n\n"
		"Usage: %s -h\n"
		"or:    %s -V\n"
		"or:    %s -D\n"
		"or:    %s -I [-v] [-n] [-f <file>] [-e <config>]\n"
		"or:    %s -L [-v] [-n] [-f <file>] [-e <config>]\n"
		"or:    %s -S [-v] [-n] [-f <file>] [-e <config>]\n"
		"       %s    [--] <diskname> <srcfile|device>\n"
		"or:    %s -R [-v] [-n] [-f <file>] [-e <config>] [-r <num>]\n"
		"       %s    [-o <file>] [--] <diskname> <srcfile|device>\n"
		"       %s    [<srcfile> ... ] <dstfile>\n"
		"or:    %s -W [-v] [-n] [-f <file>] [-e <config>] [-s]\n"
		"       %s    [--] <diskname> <srcfile> <dstfile|device>\n\n"
		"  -V            print out version\n"
		"  -D            dump builtin config\n"
		"  -I            initialize configured drives\n"
		"  -L            list available disk names\n"
		"  -S            print out statistics\n"
		"  -R            read disk\n"
		"  -W            write disk\n"
		"  -v            be more verbose\n"
		"  -n            do not read rc files\n"
		"  -f <file>     read additional config file\n"
		"  -e <config>   evaluate given string as config\n"
		"  -r <num>      number of retries if errors occur\n"
		"  -o <file>     output raw data of bad sectors to file\n"
		"  -s            ignore size\n"
		"  -h            this help\n",
		global_version_string(), space1, space1, global_program_name(),
		global_program_name(), global_program_name(), global_program_name(),
		global_program_name(), global_program_name(), space2,
		global_program_name(), space2, space2, global_program_name(),
		space2);
	exit(0);
	}



/****************************************************************************
 * cmdline_check_arg
 ****************************************************************************/
static cw_char_t *
cmdline_check_arg(
	cw_char_t			*option,
	cw_char_t			*msg,
	cw_char_t			*file)

	{
	if (file == NULL) error_message("%s expects a %s", option, msg);
	return (file);
	}



/****************************************************************************
 * cmdline_check_stdin
 ****************************************************************************/
static cw_char_t *
cmdline_check_stdin(
	cw_char_t			*option,
	cw_char_t			*file)

	{
	static cw_count_t		stdin_count;

	cmdline_check_arg(option, "file name", file);
	if (string_equal(file, "-")) stdin_count++;
	if (stdin_count > 1) error_message("%s used stdin although already used before", option);
	return (file);
	}



/****************************************************************************
 * cmdline_check_stdout
 ****************************************************************************/
static cw_char_t *
cmdline_check_stdout(
	cw_char_t			*option,
	cw_char_t			*file)

	{
	static cw_count_t		stdout_count;

	cmdline_check_arg(option, "file name", file);
	if (string_equal(file, "-")) stdout_count++;
	if (stdout_count > 1) error_message("%s used stdout although already used before", option);
	return (file);
	}



/****************************************************************************
 * cmdline_min_params
 ****************************************************************************/
static cw_count_t
cmdline_min_params(
	cw_void_t)

	{
	if (cmd.mode == CMDLINE_MODE_READ)       return (3);
	if (cmd.mode == CMDLINE_MODE_WRITE)      return (3);
	if (cmd.mode == CMDLINE_MODE_STATISTICS) return (2);
	return (0);
	}



/****************************************************************************
 * cmdline_max_params
 ****************************************************************************/
static cw_count_t
cmdline_max_params(
	cw_void_t)

	{
	if (cmd.mode == CMDLINE_MODE_READ)       return (GLOBAL_NR_IMAGES);
	if (cmd.mode == CMDLINE_MODE_WRITE)      return (3);
	if (cmd.mode == CMDLINE_MODE_STATISTICS) return (2);
	return (0);
	}



/****************************************************************************
 * cmdline_read_rc_files
 ****************************************************************************/
static cw_void_t
cmdline_read_rc_files(
	cw_void_t)

	{
	cw_char_t			*home, path[GLOBAL_MAX_PATH_SIZE];

	config_parse_path("/etc/cwtoolrc", CW_BOOL_TRUE);
	home = getenv("HOME");
	if (home == NULL) return;
	string_snprintf(path, sizeof (path), "%s/.cwtoolrc", home);
	config_parse_path(path, CW_BOOL_TRUE);
	}



/****************************************************************************
 * cmdline_verbose_level
 ****************************************************************************/
static cw_void_t
cmdline_verbose_level(
	cw_void_t)

	{
	cw_count_t			generic_level, level;

	generic_level = verbose_get_level(VERBOSE_CLASS_GENERIC);
	if ((cmd.mode == CMDLINE_MODE_INITIALIZE) ||
		(cmd.mode == CMDLINE_MODE_LIST) ||
		(cmd.mode == CMDLINE_MODE_READ) ||
		(cmd.mode == CMDLINE_MODE_WRITE))
		{
		level = verbose_get_level(VERBOSE_CLASS_CWTOOL_ILRW);
		if (level < VERBOSE_LEVEL_1) verbose_set_level(VERBOSE_CLASS_CWTOOL_ILRW, level + 1);
		else verbose_set_level(VERBOSE_CLASS_GENERIC, generic_level + 1);
		}
	else if (cmd.mode == CMDLINE_MODE_STATISTICS)
		{
		level = verbose_get_level(VERBOSE_CLASS_CWTOOL_S);
		if (level < VERBOSE_LEVEL_2) verbose_set_level(VERBOSE_CLASS_CWTOOL_S, level + 1);
		else verbose_set_level(VERBOSE_CLASS_GENERIC, generic_level + 1);
		}
	else
		{
		verbose_set_level(VERBOSE_CLASS_GENERIC, generic_level + 1);
		}
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * cmdline_parse
 ****************************************************************************/
cw_bool_t
cmdline_parse(
	cw_char_t			**argv)

	{
	cw_char_t			*arg;
	cw_bool_t			ignore = CW_BOOL_FALSE;
	cw_count_t			args = 0, params = 0;

	for (argv++; (arg = *argv++) != NULL; args++)
		{
		if ((arg[0] != '-') || (string_equal(arg, "-")) || (ignore))
			{
			if (cmd.mode == CMDLINE_MODE_DEFAULT) goto bad_option;
			if (params >= cmdline_max_params()) error_message("too many parameters given");
			if (params >= 1)
				{
				if (cmd.files > 0) cmdline_check_stdin("<srcfile>", cmd.file[cmd.files - 1]);
				cmd.file[cmd.files++] = arg;
				}
			else cmd.disk_name = arg;
			params++;
			}
		else if (string_equal(arg, "--"))
			{
			ignore = CW_BOOL_TRUE;
			}
		else if (string_equal2(arg, "-h", "--help"))
			{
			cmdline_usage();
			}
		else if ((string_equal2(arg, "-V", "--version")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_VERSION;
			}
		else if ((string_equal2(arg, "-D", "--dump")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_DUMP;
			}
		else if ((string_equal2(arg, "-I", "--initialize")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_INITIALIZE;
			}
		else if ((string_equal2(arg, "-L", "--list")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_LIST;
			}
		else if ((string_equal2(arg, "-S", "--statistics")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_STATISTICS;
			}
		else if ((string_equal2(arg, "-R", "--read")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_READ;
			}
		else if ((string_equal2(arg, "-W", "--write")) && (args == 0))
			{
			cmd.mode = CMDLINE_MODE_WRITE;
			}
		else if ((cmd.mode == CMDLINE_MODE_DEFAULT) || (cmd.mode == CMDLINE_MODE_VERSION) || (cmd.mode == CMDLINE_MODE_DUMP))
			{
			goto bad_option;
			}
		else if (string_equal2(arg, "-v", "--verbose"))
			{
			cmdline_verbose_level();
			}
		else if (string_equal2(arg, "-d", "--debug"))
			{
			debug_set_all_levels(debug_get_level(DEBUG_CLASS_GENERIC) + 1);
			}
		else if (string_equal2(arg, "-n", "--no-rcfiles"))
			{
			cmd.flags |= CMDLINE_FLAG_NO_RCFILES;
			}
		else if (string_equal2(arg, "-f", "--file"))
			{
			if (cmd.configs >= CMDLINE_NR_CONFIGS) error_message("too many config files specified");
			cmd.cfg[cmd.configs++] = (struct cmdline_config)
				{
				.type = CMDLINE_CONFIG_TYPE_FILE,
				.data = cmdline_check_stdin("-f/--file", *argv++)
				};
			}
		else if (string_equal2(arg, "-e", "--evaluate"))
			{
			if (cmd.configs >= CMDLINE_NR_CONFIGS) error_message("too many -e/--evaluate options");
			cmd.cfg[cmd.configs++] = (struct cmdline_config)
				{
				.type = CMDLINE_CONFIG_TYPE_EVALUATE,
				.data = cmdline_check_arg("-e/--evaluate", "parameter", *argv++)
				};
			}
		else if ((string_equal2(arg, "-r", "--retry")) && (cmd.mode == CMDLINE_MODE_READ))
			{
			cw_count_t	i = 0;

			if (*argv != NULL) i = sscanf(*argv++, "%d", &cmd.retry);
			if ((i != 1) || (cmd.retry < 0) || (cmd.retry > GLOBAL_NR_RETRIES)) error_message("-r/--retry expects a valid number of retries");
			}
		else if ((string_equal2(arg, "-o", "--output")) && (cmd.mode == CMDLINE_MODE_READ))
			{
			if (cmd.output != NULL) error_message("-o/--output already specified");
			cmd.output = cmdline_check_stdout("-o/--output", *argv++);
			options_set_output(CW_BOOL_TRUE);
			}
		else if ((string_equal2(arg, "-s", "--ignore-size")) && (cmd.mode == CMDLINE_MODE_WRITE))
			{
			cmd.flags |= CMDLINE_FLAG_IGNORE_SIZE;
			}
		else
			{
		bad_option:
			error_message("unrecognized option '%s'", arg);
			}
		}
	if ((params < cmdline_min_params()) || (cmd.mode == CMDLINE_MODE_DEFAULT)) error_message("too few parameters given");
	if (params >= 2) cmdline_check_stdout("<dstfile>", cmd.file[cmd.files - 1]);

	return (CW_BOOL_OK);
	}



/****************************************************************************
 * cmdline_get_mode
 ****************************************************************************/
cw_mode_t
cmdline_get_mode(
	cw_void_t)

	{
	return (cmd.mode);
	}



/****************************************************************************
 * cmdline_get_flag
 ****************************************************************************/
cw_bool_t
cmdline_get_flag(
	cw_flag_t			mask)

	{
	return (((cmd.flags & mask) != 0) ? CW_BOOL_TRUE : CW_BOOL_FALSE);
	}



/****************************************************************************
 * cmdline_get_retry
 ****************************************************************************/
cw_count_t
cmdline_get_retry(
	cw_void_t)

	{
	return (cmd.retry);
	}



/****************************************************************************
 * cmdline_get_output
 ****************************************************************************/
cw_char_t *
cmdline_get_output(
	cw_void_t)

	{
	return (cmd.output);
	}



/****************************************************************************
 * cmdline_get_disk_name
 ****************************************************************************/
cw_char_t *
cmdline_get_disk_name(
	cw_void_t)

	{
	return (cmd.disk_name);
	}



/****************************************************************************
 * cmdline_get_file
 ****************************************************************************/
cw_char_t *
cmdline_get_file(
	cw_index_t			index)

	{
	if ((index < 0) || (index >= cmd.files)) return (NULL);
	return (cmd.file[index]);
	}



/****************************************************************************
 * cmdline_get_all_files
 ****************************************************************************/
cw_char_t **
cmdline_get_all_files(
	cw_void_t)

	{
	return (cmd.file);
	}



/****************************************************************************
 * cmdline_get_files
 ****************************************************************************/
cw_count_t
cmdline_get_files(
	cw_void_t)

	{
	return (cmd.files);
	}



/****************************************************************************
 * cmdline_read_config
 ****************************************************************************/
cw_bool_t
cmdline_read_config(
	cw_void_t)

	{
	cw_char_t			number[128];
	cw_count_t			e;
	cw_index_t			i;

	/* read builtin config and rc-files */

	config_parse_memory("(builtin config)", config_default(0), string_length(config_default(0)));
	if (! (cmd.flags & CMDLINE_FLAG_NO_RCFILES)) cmdline_read_rc_files();
	
	/* read configs specified on cmdline */

	for (e = i = 0; i < cmd.configs; i++)
		{
		if (cmd.cfg[i].type == CMDLINE_CONFIG_TYPE_FILE)
			{
			config_parse_path(cmd.cfg[i].data, CW_BOOL_FALSE);
			continue;
			}
		string_snprintf(number, sizeof (number), "-e/--evaluate #%d", ++e);
		config_parse_memory(number, cmd.cfg[i].data, string_length(cmd.cfg[i].data));
		}

	/* done */

	return (CW_BOOL_OK);
	}
/******************************************************** Karsten Scheibler */

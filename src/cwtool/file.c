/****************************************************************************
 ****************************************************************************
 *
 * file.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "file.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "string.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * file_tmp
 ****************************************************************************/
static cw_char_t *
file_tmp(
	cw_size_t			size)

	{
	static cw_count_t		count;
	struct timeval			tv;
	cw_char_t			*path    = malloc(size);
	cw_char_t			*tmp_dir = getenv("TMPDIR");
	cw_count_t			len;

	if (path == NULL) error_oom();
	if (tmp_dir == NULL) tmp_dir = "/tmp";
	if (gettimeofday(&tv, NULL) == -1) error_perror_message("error while gettimeofday()");
	len = snprintf(path, size, "%s/%s-%010d-%010ld-%06ld-%05d", tmp_dir, global_program_name(), count++, tv.tv_sec, tv.tv_usec, getpid());
	if ((len == -1) || (len >= size)) error_message("path for tmp file too long");
	return (path);
	}



/****************************************************************************
 * file_try_again
 ****************************************************************************/
static cw_bool_t
file_try_again(
	cw_int_t			num)

	{
	struct timeval			tv = { .tv_sec = 0, .tv_usec = 100000 };

	/* if we got EAGAIN or EINTR we wait some time before trying again */

	if ((num != EAGAIN) && (num != EINTR)) return (CW_BOOL_FALSE);
	select(0, NULL, NULL, NULL, &tv);
	return (CW_BOOL_TRUE);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * file_open
 ****************************************************************************/
cw_bool_t
file_open(
	struct file			*fil,
	const cw_char_t			*path,
	cw_mode_t			mode,
	cw_flag_t			flags)

	{
	debug_error_condition((mode != FILE_MODE_READ) && (mode != FILE_MODE_WRITE) && (mode != FILE_MODE_CREATE) && (mode != FILE_MODE_TMP));
	*fil = (struct file)
		{
		.path = (cw_char_t *) path,
		.fd   = -1,
		.mode = mode
		};

	/*
	 * check if we really have to open a file or just take the fds of
	 * stdin and stdout
	 */

	if (mode == FILE_MODE_READ)
		{
		if (! string_equal(path, "-"))
			{
			verbose_message(GENERIC, 2, "opening '%s' for reading", path);
			fil->fd = open(path, O_RDONLY);
			}
		else fil->fd = STDIN_FILENO;
		}
	else if (mode == FILE_MODE_WRITE)
		{
		if (! string_equal(path, "-"))
			{
			verbose_message(GENERIC, 2, "opening '%s' for writing", path);
			fil->fd = open(path, O_WRONLY);
			}
		else fil->fd = STDOUT_FILENO;
		}
	else if (mode == FILE_MODE_CREATE)
		{
		if (! string_equal(path, "-"))
			{
			verbose_message(GENERIC, 2, "truncating or creating '%s' for writing", path);
			fil->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			}
		else fil->fd = STDOUT_FILENO;
		}
	else
		{
		if (path == NULL)
			{
			fil->allocated = CW_BOOL_TRUE;
			path = file_tmp(GLOBAL_MAX_PATH_SIZE);
			fil->path = (cw_char_t *) path;
			}
		verbose_message(GENERIC, 2, "opening temporary file '%s' for reading and writing", path);
		fil->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, S_IRUSR | S_IWUSR);
		}

	/* check if open was successful */

	if (fil->fd == -1)
		{
		if (flags & FILE_FLAG_RETURN) return (CW_BOOL_FAIL);
		error_perror_message("error while opening '%s'", path);
		}

	/*
	 * tmp files are unlinked after creation, just keeping the fd open is
	 * enough for what is done here
	 */

	if (mode == FILE_MODE_TMP)
		{
		verbose_message(GENERIC, 2, "unlinking temporary file '%s' but keeping it open, so it will consume disk space", path);
		if (unlink(path) == -1) error_perror_message("error while unlinking '%s'", path);
		}
	return (CW_BOOL_OK);
	}



/****************************************************************************
 * file_close
 ****************************************************************************/
cw_void_t
file_close(
	struct file			*fil)

	{
	if (close(fil->fd) == -1) error_perror_message("error while closing '%s'", fil->path);
	if (fil->allocated) free(fil->path);
	*fil = (struct file) { .fd = -1 };
	}



/****************************************************************************
 * file_get_path
 ****************************************************************************/
const cw_char_t *
file_get_path(
	struct file			*fil)

	{
	return (fil->path);
	}



/****************************************************************************
 * file_is_readable
 ****************************************************************************/
cw_bool_t
file_is_readable(
	struct file			*fil)

	{
	if (fil->mode == FILE_MODE_READ) return (CW_BOOL_TRUE);
	if (fil->mode == FILE_MODE_TMP)  return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * file_is_writable
 ****************************************************************************/
cw_bool_t
file_is_writable(
	struct file			*fil)

	{
	if (fil->mode == FILE_MODE_WRITE)  return (CW_BOOL_TRUE);
	if (fil->mode == FILE_MODE_CREATE) return (CW_BOOL_TRUE);
	if (fil->mode == FILE_MODE_TMP)    return (CW_BOOL_TRUE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * file_ioctl2
 ****************************************************************************/
cw_int_t
file_ioctl2(
	struct file			*fil,
	cw_mode_t			cmd,
	cw_ptr_t			arg,
	cw_flag_t			flags)

	{
	cw_int_t			result;

	/*
	 * probably EAGAIN and EINTR as checked in file_try_again() never
	 * occur in combination with ioctl()
	 */

	while (1)
		{
		result = ioctl(fil->fd, cmd, arg);
		if (result != -1) break;
		if (file_try_again(errno)) continue;
		if (flags & FILE_FLAG_RETURN) break;
		error_perror_message("error while accessing device '%s'", fil->path);
		}
	return (result);
	}



/****************************************************************************
 * file_seek
 ****************************************************************************/
cw_count_t
file_seek(
	struct file			*fil,
	cw_count_t			ofs,
	cw_flag_t			flags)

	{
	cw_int_t			result;
	cw_int_t			whence = SEEK_SET;

	/*
	 * probably EAGAIN and EINTR as checked in file_try_again() never
	 * occur in combination with lseek()
	 */

	if (ofs < 0) ofs = 0, whence = SEEK_CUR;
	while (1)
		{
		result = lseek(fil->fd, ofs, whence);
		if (result != -1) break;
		if (file_try_again(errno)) continue;
		if (flags & FILE_FLAG_RETURN) break;
		error_perror_message("error while seeking '%s'", fil->path);
		}
	return (result);
	}



/****************************************************************************
 * file_read
 ****************************************************************************/
cw_count_t
file_read(
	struct file			*fil,
	cw_void_t			*data,
	cw_size_t			size)

	{
	cw_int_t			result = 1;
	cw_count_t			ofs = 0;

	debug_error_condition(! file_is_readable(fil));
	while ((result > 0) && (size > 0))
		{
		result = read(fil->fd, data, size);
		if (result == -1)
			{
			if (file_try_again(errno)) continue;
			error_perror_message("error while reading from '%s'", fil->path);
			}
		data += result;
		size -= result;
		ofs += result;
		}
	return (ofs);
	}



/****************************************************************************
 * file_read_strict
 ****************************************************************************/
cw_count_t
file_read_strict(
	struct file			*fil,
	cw_void_t			*data,
	cw_size_t			size)

	{
	if (file_read(fil, data, size) != size) error_message("file '%s' truncated", fil->path);
	return (size);
	}



/****************************************************************************
 * file_write
 ****************************************************************************/
cw_count_t
file_write(
	struct file			*fil,
	const cw_void_t			*data,
	cw_size_t			size)

	{
	cw_int_t			result;
	cw_count_t			ofs = 0;

	debug_error_condition(! file_is_writable(fil));
	while (size > 0)
		{
		result = write(fil->fd, data, size);
		if (result == -1)
			{
			if (file_try_again(errno)) continue;
			error_perror_message("error while writing to '%s'", fil->path);
			}
		data += result;
		size -= result;
		ofs += result;
		}
	return (ofs);
	}



/****************************************************************************
 * file_write_string
 ****************************************************************************/
cw_count_t
file_write_string(
	struct file			*fil,
	const cw_char_t			*string)

	{
	return (file_write(fil, string, string_length(string)));
	}



/****************************************************************************
 * file_write_sprintf
 ****************************************************************************/
cw_count_t
file_write_sprintf(
	struct file			*fil,
	const cw_char_t			*format,
	...)

	{
	va_list				args;
	cw_char_t			string[0x10000];
	cw_count_t			len;

	va_start(args, format);
	len = vsnprintf(string, sizeof (string), format, args);
	va_end(args);
	if ((len == -1) || (len >= sizeof (string))) error_message("file_write_sprintf() size exceeded");
	return (file_write(fil, string, len));
	}
/******************************************************** Karsten Scheibler */

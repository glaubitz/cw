/****************************************************************************
 ****************************************************************************
 *
 * file.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FILE_H
#define CWTOOL_FILE_H

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define FILE_MODE_READ			1
#define FILE_MODE_WRITE			2
#define FILE_MODE_CREATE		3
#define FILE_MODE_TMP			4

#define FILE_FLAG_NONE			0
#define FILE_FLAG_RETURN		(1 << 0)

struct file
	{
	cw_char_t			*path;
	cw_int_t			fd;
	cw_mode_t			mode;
	cw_bool_t			allocated;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_bool_t
file_open(
	struct file			*fil,
	const cw_char_t			*path,
	cw_mode_t			mode,
	cw_flag_t			flags);

extern cw_void_t
file_close(
	struct file			*fil);

extern const cw_char_t *
file_get_path(
	struct file			*fil);

extern cw_bool_t
file_is_readable(
	struct file			*fil);

extern cw_bool_t
file_is_writable(
	struct file			*fil);

extern cw_int_t
file_ioctl2(
	struct file			*fil,
	cw_mode_t			cmd,
	cw_ptr_t			arg,
	cw_flag_t			flags);

extern cw_count_t
file_seek(
	struct file			*fil,
	cw_count_t			ofs,
	cw_flag_t			flags);

extern cw_count_t
file_read(
	struct file			*fil,
	cw_void_t			*data,
	cw_size_t			size);

extern cw_count_t
file_read_strict(
	struct file			*fil,
	cw_void_t			*data,
	cw_size_t			size);

extern cw_count_t
file_write(
	struct file			*fil,
	const cw_void_t			*data,
	cw_size_t			size);

extern cw_count_t
file_write_string(
	struct file			*fil,
	const cw_char_t			*string);

extern cw_count_t
file_write_sprintf(
	struct file			*fil,
	const cw_char_t			*format,
	...);

#define file_ioctl(fil, cmd, arg, fl)	file_ioctl2(fil, cmd, (cw_ptr_t) arg, fl)



#endif /* !CWTOOL_FILE_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * drive.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "drive.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "file.h"
#include "setvalue.h"
#include "string.h"




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * drive_init_device
 ****************************************************************************/
static int
drive_init_device(
	struct drive			*drv)

	{
	struct file			fil;
	struct cw_floppyinfo		fli = CW_FLOPPYINFO_INIT;

	verbose_message(GENERIC, 1, "initializing '%s'", drv->path);
	file_open(&fil, drv->path, FILE_MODE_WRITE, FILE_FLAG_NONE);
	file_ioctl(&fil, CW_IOC_GFLPARM, &fli, FILE_FLAG_NONE);
	fli.step_time     = drv->fli.step_time;
	fli.settle_time   = drv->fli.settle_time;
	fli.wpulse_length = drv->fli.wpulse_length;
	fli.nr_tracks     = drv->fli.nr_tracks;
	fli.nr_sides      = drv->fli.nr_sides;
	fli.flags         = drv->fli.flags;
	file_ioctl(&fil, CW_IOC_SFLPARM, &fli, FILE_FLAG_NONE);
	file_close(&fil);
	return (1);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * drive_get
 ****************************************************************************/
struct drive *
drive_get(
	int				i)

	{
	static struct drive		drv[GLOBAL_NR_DRIVES];
	static int			drives;

	if (i == -1)
		{
		if (drives >= GLOBAL_NR_DRIVES) error_message("too many drives defined");
		debug_message(GENERIC, 1, "request for unused drive struct, drives = %d", drives);
		return (&drv[drives++]);
		}
	if ((i >= 0) && (i < drives)) return (&drv[i]);
	return (NULL);
	}



/****************************************************************************
 * drive_search
 ****************************************************************************/
struct drive *
drive_search(
	const char			*path)

	{
	struct drive			*drv;
	int				i;

	for (i = 0; (drv = drive_get(i)) != NULL; i++) if (string_equal(path, drv->path)) break;
	return (drv);
	}



/****************************************************************************
 * drive_init
 ****************************************************************************/
int
drive_init(
	struct drive			*drv,
	int				revision)

	{
	*drv = (struct drive)
		{
		.revision = revision,
		.fli      =
			{
			.settle_time   = CW_DEFAULT_SETTLE_TIME,
			.step_time     = CW_DEFAULT_STEP_TIME,
			.wpulse_length = CW_DEFAULT_WPULSE_LENGTH,
			.nr_tracks     = CW_NR_TRACKS,
			.nr_sides      = CW_NR_SIDES,
			.flags         = CW_FLOPPYINFO_FLAG_NONE
			}
		};
	return (1);
	}



/****************************************************************************
 * drive_insert
 ****************************************************************************/
int
drive_insert(
	struct drive			*drv)

	{
	struct drive			*drv2;

	drv2 = drive_search(drv->path);
	if (drv2 != NULL) *drv2 = *drv;
	else *drive_get(-1) = *drv;
	return (1);
	}



/****************************************************************************
 * drive_set_path
 ****************************************************************************/
int
drive_set_path(
	struct drive			*drv,
	const char			*path)

	{
	struct drive			*drv2 = drive_search(path);

	if (drv2 != NULL) if (drv->revision <= drv2->revision) return (0);
	string_copy(drv->path, GLOBAL_MAX_PATH_SIZE, path);
	return (1);
	}



/****************************************************************************
 * drive_set_info
 ****************************************************************************/
int
drive_set_info(
	struct drive			*drv,
	const char			*info)

	{
	string_copy(drv->info, GLOBAL_MAX_NAME_SIZE, info);
	return (1);
	}



/****************************************************************************
 * drive_set_settle_time
 ****************************************************************************/
int
drive_set_settle_time(
	struct drive			*drv,
	int				val)

	{
	return (setvalue_int(&drv->fli.settle_time, val, CW_MIN_SETTLE_TIME, CW_MAX_SETTLE_TIME));
	}



/****************************************************************************
 * drive_set_step_time
 ****************************************************************************/
int
drive_set_step_time(
	struct drive			*drv,
	int				val)

	{
	return (setvalue_int(&drv->fli.step_time, val, CW_MIN_STEP_TIME, CW_MAX_STEP_TIME));
	}



/****************************************************************************
 * drive_set_wpulse_length
 ****************************************************************************/
int
drive_set_wpulse_length(
	struct drive			*drv,
	int				val)

	{
	return (setvalue_int(&drv->fli.wpulse_length, val, CW_MIN_WPULSE_LENGTH, CW_MAX_WPULSE_LENGTH));
	}



/****************************************************************************
 * drive_set_flag
 ****************************************************************************/
int
drive_set_flag(
	struct drive			*drv,
	int				val,
	int				bit)

	{
	return (setvalue_uint_bit(&drv->fli.flags, val, bit));
	}



/****************************************************************************
 * drive_get_path
 ****************************************************************************/
const char *
drive_get_path(
	struct drive			*drv)

	{
	debug_error_condition(drv == NULL);
	return (drv->path);
	}



/****************************************************************************
 * drive_get_info
 ****************************************************************************/
const char *
drive_get_info(
	struct drive			*drv)

	{
	debug_error_condition(drv == NULL);
	return (drv->info);
	}



/****************************************************************************
 * drive_init_all_devices
 ****************************************************************************/
int
drive_init_all_devices(
	void)

	{
	struct drive			*drv;
	int				i;

	for (i = 0; (drv = drive_get(i)) != NULL; i++) drive_init_device(drv);
	return (1);
	}
/******************************************************** Karsten Scheibler */

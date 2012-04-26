/****************************************************************************
 ****************************************************************************
 *
 * cwio.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "cwio.h"
#include "ioctl.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#if CW_STRUCT_VERSION < 2
#define CWIO_NR_TRACKS			CW_MAX_TRACK
#define CWIO_NR_SIDES			CW_MAX_SIDE
#define CWIO_MAX_TRACK_SIZE		CW_MAX_SIZE
#else /* CW_STRUCT_VERSION */
#define CWIO_NR_TRACKS			CW_NR_TRACKS
#define CWIO_NR_SIDES			CW_NR_SIDES
#define CWIO_MAX_TRACK_SIZE		CW_MAX_TRACK_SIZE
#endif /* CW_STRUCT_VERSION */

#define CWIO_DEVICE_FLAG_INITIALIZED	(1 << 0)
#define CWIO_DEVICE_FLAG_OPEN		(1 << 1)

struct cwio_device
	{
	char				path[CWIO_MAX_PATH_LEN];
	int				type;
	int				fd;
	int				flags;
	int				mode;
	struct cw_floppyinfo		fli;
	};

#define CWIO_DATA_FLAG_INITIALIZED	(1 << 0)

struct cwio_data
	{
	int				flags;
	int				track;
	int				side;
	int				clock;
	int				mode;
	int				timeout;
	void				*data;
	int				size;
	};

static const char			program_name[]               = "cwio";
static const char			error_device_null[]          = "cwio_dev == NULL";
static const char			error_data_null[]            = "cwio_data == NULL";
static const char			error_device_not_open[]      = "device not open";
static const char			error_data_not_initialized[] = "cwio_data not initialized";




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * cwio_perror
 ****************************************************************************/
static void
cwio_perror(
	const char			*msg)

	{
	fprintf(stderr, "%s: %s\n", program_name, msg);
	perror(program_name);
	exit(1);
	}



/****************************************************************************
 * cwio_error
 ****************************************************************************/
static void
cwio_error(
	const char			*msg)

	{
	fprintf(stderr, "%s: %s\n", program_name, msg);
	exit(1);
	}



/****************************************************************************
 * cwio_device_check_type
 ****************************************************************************/
static void
cwio_device_check_type(
	int				type)

	{
	if (type == CWIO_DEVICE_TYPE_ANY) return;
	if (type == CWIO_DEVICE_TYPE_DEVICE) return;
	cwio_error("device type not supported");
	}



/****************************************************************************
 * cwio_data_get_track
 ****************************************************************************/
static int
cwio_data_get_track(
	int				track)

	{
	if ((track >= 0) && (track < CWIO_NR_TRACKS)) return (track);
	cwio_error("invalid track value");

	/* never reached */

	return (0);
	}



/****************************************************************************
 * cwio_data_get_side
 ****************************************************************************/
static int
cwio_data_get_side(
	int				side)

	{
	if ((side >= 0) && (side < CWIO_NR_SIDES)) return (side);
	cwio_error("invalid side value");

	/* never reached */

	return (0);
	}



/****************************************************************************
 * cwio_data_get_clock
 ****************************************************************************/
static int
cwio_data_get_clock(
	int				clock)

	{
	if (clock == CWIO_DATA_CLOCK_14MHZ) return (CW_TRACKINFO_CLOCK_14MHZ);
	if (clock == CWIO_DATA_CLOCK_28MHZ) return (CW_TRACKINFO_CLOCK_28MHZ);
	if (clock == CWIO_DATA_CLOCK_56MHZ) return (CW_TRACKINFO_CLOCK_56MHZ);
	cwio_error("invalid CWIO_DATA_CLOCK value");

	/* never reached */

	return (0);
	}



/****************************************************************************
 * cwio_data_get_mode
 ****************************************************************************/
static int
cwio_data_get_mode(
	int				mode)

	{
	if (mode == CWIO_DATA_MODE_NORMAL)      return (CW_TRACKINFO_MODE_NORMAL);
	if (mode == CWIO_DATA_MODE_INDEX_WAIT)  return (CW_TRACKINFO_MODE_INDEX_WAIT);
	if (mode == CWIO_DATA_MODE_INDEX_STORE) return (CW_TRACKINFO_MODE_INDEX_STORE);
	cwio_error("invalid CWIO_DATA_MODE value");

	/* never reached */

	return (0);
	}



/****************************************************************************
 * cwio_data_get_timeout
 ****************************************************************************/
static int
cwio_data_get_timeout(
	int				timeout)

	{
	if ((timeout >= CW_MIN_TIMEOUT) && (timeout <= CW_MAX_TIMEOUT)) return (timeout);
	cwio_error("invalid timeout value");

	/* never reached */

	return (0);
	}



/****************************************************************************
 * cwio_data_check_track
 ****************************************************************************/
static void
cwio_data_check_track(
	int				track)

	{
	cwio_data_get_track(track);
	}



/****************************************************************************
 * cwio_data_check_side
 ****************************************************************************/
static void
cwio_data_check_side(
	int				side)

	{
	cwio_data_get_side(side);
	}



/****************************************************************************
 * cwio_data_check_clock
 ****************************************************************************/
static void
cwio_data_check_clock(
	int				clock)

	{
	cwio_data_get_clock(clock);
	}



/****************************************************************************
 * cwio_data_check_mode
 ****************************************************************************/
static void
cwio_data_check_mode(
	int				mode)

	{
	cwio_data_get_mode(mode);
	}



/****************************************************************************
 * cwio_data_check_timeout
 ****************************************************************************/
static void
cwio_data_check_timeout(
	int				timeout)

	{
	cwio_data_get_timeout(timeout);
	}



/****************************************************************************
 * cwio_data_set_trackinfo
 ****************************************************************************/
static void
cwio_data_set_trackinfo(
	struct cwio_data		*cwio_data,
	struct cw_trackinfo		*tri,
	void				*data)

	{
	tri->track      = cwio_data_get_track(cwio_data->track);
#if CW_STRUCT_VERSION >= 2
	tri->track_seek = cwio_data_get_track(cwio_data->track);
#endif /* CW_STRUCT_VERSION */
	tri->side       = cwio_data_get_side(cwio_data->side);
	tri->clock      = cwio_data_get_clock(cwio_data->clock);
	tri->mode       = cwio_data_get_mode(cwio_data->mode);
	tri->timeout    = cwio_data_get_timeout(cwio_data->timeout);
	tri->data       = data;
	tri->size       = cwio_data->size;
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * cwio_device_struct_size
 ****************************************************************************/
int
cwio_device_struct_size(
	void)

	{
	return (sizeof (struct cwio_device));
	}



/****************************************************************************
 * cwio_device_set_params
 ****************************************************************************/
int
cwio_device_set_params(
	struct cwio_device		*cwio_dev,
	const char			*path,
	int				type)

	{
	if (cwio_dev == NULL) cwio_error(error_device_null);
	cwio_device_check_type(type);
	*cwio_dev = (struct cwio_device)
		{
		.type  = type,
		.fd    = -1,
		.flags = CWIO_DEVICE_FLAG_INITIALIZED,
		.fli   = CW_FLOPPYINFO_INIT
		};
	strncpy(cwio_dev->path, path, CWIO_MAX_PATH_LEN - 1);
	cwio_dev->path[CWIO_MAX_PATH_LEN - 1] = '\0';

	return (CWIO_DEVICE_TYPE_DEVICE);
	}



/****************************************************************************
 * cwio_device_has_double_step
 ****************************************************************************/
int
cwio_device_has_double_step(
	struct cwio_device		*cwio_dev)

	{
	if (cwio_dev == NULL) cwio_error(error_device_null);
	if (! (cwio_dev->flags & CWIO_DEVICE_FLAG_OPEN)) cwio_error(error_device_not_open);
	if (cwio_dev->fli.flags & CW_FLOPPYINFO_FLAG_DOUBLE_STEP) return (1);
	return (0);
	}



/****************************************************************************
 * cwio_data_struct_size
 ****************************************************************************/
int
cwio_data_struct_size(
	void)

	{
	return (sizeof (struct cwio_data));
	}



/****************************************************************************
 * cwio_data_set_params
 ****************************************************************************/
int
cwio_data_set_params(
	struct cwio_data		*cwio_data,
	int				track,
	int				side,
	int				clock,
	int				mode,
	int				timeout,
	void				*data,
	int				size)

	{
	if (cwio_data == NULL) cwio_error(error_data_null);
	cwio_data_check_track(track);
	cwio_data_check_side(side);
	cwio_data_check_clock(clock);
	cwio_data_check_mode(mode);
	cwio_data_check_timeout(timeout);
	*cwio_data = (struct cwio_data)
		{
		.flags   = CWIO_DATA_FLAG_INITIALIZED,
		.track   = track,
		.side    = side,
		.clock   = clock,
		.mode    = mode,
		.timeout = timeout,
		.data    = data,
		.size    = size
		};

	return (0);
	}



/****************************************************************************
 * cwio_open
 ****************************************************************************/
int
cwio_open(
	struct cwio_device		*cwio_dev,
	int				mode)

	{
	int				m = O_RDONLY;
	int				result;

	if (cwio_dev == NULL) cwio_error(error_device_null);
	if (! (cwio_dev->flags & CWIO_DEVICE_FLAG_INITIALIZED)) cwio_error("cwio_dev not initialized");
	if (cwio_dev->flags & CWIO_DEVICE_FLAG_OPEN) cwio_error("device already open");

	if (mode == CWIO_MODE_WRITE) m = O_WRONLY;
	cwio_dev->fd = open(cwio_dev->path, m);
	if (cwio_dev->fd == -1) cwio_perror("error while opening device");
	cwio_dev->flags |= CWIO_DEVICE_FLAG_OPEN;
	cwio_dev->mode = mode;

	result = ioctl(cwio_dev->fd, CW_IOC_GFLPARM, &cwio_dev->fli);
	if (result == -1) cwio_perror("error while getting floppy parameters");

	return (0);
	}



/****************************************************************************
 * cwio_close
 ****************************************************************************/
int
cwio_close(
	struct cwio_device		*cwio_dev)

	{
	if (cwio_dev == NULL) cwio_error(error_device_null);
	if (! (cwio_dev->flags & CWIO_DEVICE_FLAG_OPEN)) cwio_error(error_device_not_open);
	close(cwio_dev->fd);
	cwio_dev->flags &= ~CWIO_DEVICE_FLAG_OPEN;
	cwio_dev->fd = -1;
	return (0);
	}



/****************************************************************************
 * cwio_read
 ****************************************************************************/
int
cwio_read(
	struct cwio_device		*cwio_dev,
	struct cwio_data		*cwio_data)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				result;

	if (cwio_dev == NULL) cwio_error(error_device_null);
	if (cwio_data == NULL) cwio_error(error_data_null);
	if (! (cwio_dev->flags & CWIO_DEVICE_FLAG_OPEN)) cwio_error(error_device_not_open);
	if (cwio_dev->mode != CWIO_MODE_READ) cwio_error("device not opened for reading");
	if (! (cwio_data->flags & CWIO_DATA_FLAG_INITIALIZED)) cwio_error(error_data_not_initialized);

	cwio_data_set_trackinfo(cwio_data, &tri, cwio_data->data);
	result = ioctl(cwio_dev->fd, CW_IOC_READ, &tri);
	if (result == -1) cwio_perror("error while reading track");

	/* return how many bytes we have got */

	return (result);
	}



/****************************************************************************
 * cwio_write
 ****************************************************************************/
int
cwio_write(
	struct cwio_device		*cwio_dev,
	struct cwio_data		*cwio_data)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				result;
	unsigned char			*data;
	int				i;
#if CW_STRUCT_VERSION < 2
	unsigned char			buffer[CWIO_BUFFER_SIZE];
#endif /* CW_STRUCT_VERSION */

	if (cwio_dev == NULL) cwio_error(error_device_null);
	if (cwio_data == NULL) cwio_error(error_data_null);
	if (! (cwio_dev->flags & CWIO_DEVICE_FLAG_OPEN)) cwio_error(error_device_not_open);
	if (cwio_dev->mode != CWIO_MODE_WRITE) cwio_error("device not opened for writing");
	if (! (cwio_data->flags & CWIO_DATA_FLAG_INITIALIZED)) cwio_error(error_data_not_initialized);
	if (cwio_data->mode == CWIO_DATA_MODE_INDEX_STORE) cwio_error("index storing not supported with writing");
	if (cwio_data->size >= CWIO_MAX_TRACK_SIZE - CW_WRITE_OVERHEAD) cwio_error("track too large for writing");

	data = cwio_data->data;
	for (i = 0; i < cwio_data->size; i++)
		{
		if ((data[i] < 0x03) || (data[i] > 0x7e)) cwio_error("data contains values too small or too large for writing");
		}

	/*
	 * until cw-0.12 write values are subtracted from 0x80 in the driver.
	 * this changed to 0x7f in cw-0.13
	 */

#if CW_STRUCT_VERSION < 2
	for (i = 0; i < cwio_data->size; i++) buffer[i] = data[i] + 1;
	data = buffer;
#endif /* CW_STRUCT_VERSION */

	cwio_data_set_trackinfo(cwio_data, &tri, data);
	result = ioctl(cwio_dev->fd, CW_IOC_WRITE, &tri);
	if (result == -1) cwio_perror("error while writing track");

	/* how many bytes were written */

	return (result);
	}
/******************************************************** Karsten Scheibler */

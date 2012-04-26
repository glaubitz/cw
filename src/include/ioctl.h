/****************************************************************************
 ****************************************************************************
 *
 * ioctl.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_IOCTL_H
#define CW_IOCTL_H

/*
 * stdlib.h is not really needed here, but some glibc-versions cause compile
 * time warnings like "__NFDBITS redefined" if stdlib.h is included later
 */

#ifndef __KERNEL__
#include <stdlib.h>
#endif /* !__KERNEL__ */

#include <linux/ioctl.h>

#include "types.h"

#define CW_IOC_MAGIC			0xca
#define CW_IOC_GFLPARM			_IOR(CW_IOC_MAGIC, 0, struct cw_floppyinfo)
#define CW_IOC_SFLPARM			_IOW(CW_IOC_MAGIC, 1, struct cw_floppyinfo)
#define CW_IOC_READ			_IOW(CW_IOC_MAGIC, 2, struct cw_trackinfo)
#define CW_IOC_WRITE			_IOW(CW_IOC_MAGIC, 3, struct cw_trackinfo)

/*
 * if structure or semantics of data changes, which is exchanged between
 * the driver and cwtool, then CW_STRUCT_VERSION is increased. so only
 * matching driver and cwtool versions work together
 *
 *  0    cw-0.08 - cw-0.10
 *  1    cw-0.11 - cw-0.12
 *  2    cw-0.13 -
 */

#define CW_STRUCT_VERSION		2

#define CW_NR_CONTROLLERS		4
#define CW_NR_FLOPPIES_PER_CONTROLLER	2
#define CW_NR_FLOPPIES			(CW_NR_CONTROLLERS * CW_NR_FLOPPIES_PER_CONTROLLER)
#define CW_NR_TRACKS			86
#define CW_NR_SIDES			2
#define CW_NR_CLOCKS			3
#define CW_NR_MODES			3
#define CW_MAX_TRACK_SIZE		0x20000
#define CW_WRITE_OVERHEAD		8
#define CW_MIN_TIMEOUT			50
#define CW_DEFAULT_TIMEOUT		500
#define CW_MAX_TIMEOUT			8000
#define CW_MIN_SETTLE_TIME		1
#define CW_DEFAULT_SETTLE_TIME		25
#define CW_MAX_SETTLE_TIME		1000
#define CW_MIN_STEP_TIME		3
#define CW_DEFAULT_STEP_TIME		6
#define CW_MAX_STEP_TIME		1000
#define CW_MIN_RPM			100
#define CW_MAX_RPM			500
#define CW_WPULSE_LENGTH_MULTIPLIER	35310		/* 35.31 ns = 35310 ps */
#define CW_MIN_WPULSE_LENGTH		(1 * CW_WPULSE_LENGTH_MULTIPLIER)
#define CW_DEFAULT_WPULSE_LENGTH	(10 * CW_WPULSE_LENGTH_MULTIPLIER)
#define CW_MAX_WPULSE_LENGTH		(31 * CW_WPULSE_LENGTH_MULTIPLIER)

#define CW_FLOPPYINFO_FLAG_NONE			0
#define CW_FLOPPYINFO_FLAG_INVERTED_DISKCHANGE	(1 << 0)
#define CW_FLOPPYINFO_FLAG_IGNORE_DISKCHANGE	(1 << 1)
#define CW_FLOPPYINFO_FLAG_DENSITY		(1 << 2)
#define CW_FLOPPYINFO_FLAG_DOUBLE_STEP		(1 << 3)
#define CW_FLOPPYINFO_FLAG_ALL			((1 << 4) - 1)
#define CW_FLOPPYINFO_INIT			(struct cw_floppyinfo) { .version = CW_STRUCT_VERSION }

struct cw_floppyinfo
	{
	cw_count_t			version;
	cw_msecs_t			settle_time;
	cw_msecs_t			step_time;
	cw_psecs_t			wpulse_length;
	cw_snum_t			nr_tracks;
	cw_snum_t			nr_sides;
	cw_snum_t			nr_clocks;
	cw_snum_t			nr_modes;
	cw_size_t			max_size;
	cw_count_t			rpm;
	cw_flag_t			flags;
	};

#define CW_TRACKINFO_CLOCK_14MHZ	0
#define CW_TRACKINFO_CLOCK_28MHZ	1
#define CW_TRACKINFO_CLOCK_56MHZ	2
#define CW_TRACKINFO_MODE_NORMAL	0
#define CW_TRACKINFO_MODE_INDEX_WAIT	1
#define CW_TRACKINFO_MODE_INDEX_STORE	2
#define CW_TRACKINFO_INIT		(struct cw_trackinfo) { .version = CW_STRUCT_VERSION }

struct cw_trackinfo
	{
	cw_count_t			version;
	cw_snum_t			track_seek;
	cw_snum_t			track;
	cw_snum_t			side;
	cw_snum_t			clock;
	cw_snum_t			mode;
	cw_snum_t			reserved[3];
	cw_flag_t			flags;
	cw_msecs_t			timeout;
	cw_raw_t			*data;
	cw_size_t			size;
	};



#endif /* !CW_IOCTL_H */
/******************************************************** Karsten Scheibler */

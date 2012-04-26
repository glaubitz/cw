/****************************************************************************
 ****************************************************************************
 *
 * global.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_GLOBAL_H
#define CWTOOL_GLOBAL_H

#include "types.h"
#include "version.h"
#include "ioctl.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define GLOBAL_VERSION			VERSION
#define GLOBAL_VERSION_DATE		VERSION_DATE
#define GLOBAL_PROGRAM_NAME		"cwtool"
#define GLOBAL_VERSION_STRING		GLOBAL_PROGRAM_NAME " " GLOBAL_VERSION "-" GLOBAL_VERSION_DATE

#define GLOBAL_MAX_NAME_SIZE		64
#define GLOBAL_MAX_PATH_SIZE		4096
#define GLOBAL_MIN_TRACK_SIZE		1024
#define GLOBAL_MAX_TRACK_SIZE		(CW_MAX_TRACK_SIZE - CW_WRITE_OVERHEAD)
#define GLOBAL_NR_TRACKS		(CW_NR_SIDES * CW_NR_TRACKS)
#define GLOBAL_NR_SECTORS		128
#define GLOBAL_NR_TRACKMAPS		256
#define GLOBAL_NR_DISKS			GLOBAL_NR_TRACKMAPS
#define GLOBAL_NR_DRIVES		CW_NR_FLOPPIES
#define GLOBAL_NR_IMAGES		64
#define GLOBAL_NR_RETRIES		10
#define GLOBAL_MAX_CONFIG_SIZE		0x10000

#define GLOBAL_NR_BOUNDS		8
#define GLOBAL_NR_PULSE_LENGTHS		0x80
#define GLOBAL_MIN_PULSE_LENGTH		0x03
#define GLOBAL_MAX_PULSE_LENGTH		(GLOBAL_NR_PULSE_LENGTHS - 1)
#define GLOBAL_PULSE_LENGTH_MASK	GLOBAL_MAX_PULSE_LENGTH
#define GLOBAL_PULSE_INDEX_MASK		0x80

#if ((GLOBAL_PULSE_LENGTH_MASK & (GLOBAL_PULSE_LENGTH_MASK + 1)) != 0)
#error "GLOBAL_PULSE_LENGTH_MASK is not a valid bit mask"
#endif




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern const cw_char_t *
global_program_name(
	cw_void_t);

extern const cw_char_t *
global_version_string(
	cw_void_t);



#endif /* !CWTOOL_GLOBAL_H */
/******************************************************** Karsten Scheibler */

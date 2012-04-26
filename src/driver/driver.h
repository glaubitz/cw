/****************************************************************************
 ****************************************************************************
 *
 * driver.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_DRIVER_H
#define CW_DRIVER_H

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "types.h"
#include "ioctl.h"
#include "hardware.h"
#include "floppy.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct cw_controller
	{
	int				num;
	int				ready;
	struct cw_hardware		hrd;
	struct cw_floppies		fls;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern cw_count_t			cw_debug_level;

extern struct cw_controller *
cw_driver_get_controller(
	cw_index_t			index);

extern struct cw_hardware *
cw_driver_get_hardware(
	cw_index_t			index);

extern struct cw_floppies *
cw_driver_get_floppies(
	cw_index_t			index);

extern cw_void_t
cw_driver_register_hardware(
	struct cw_hardware		*hrd);

extern cw_void_t
cw_driver_unregister_hardware(
	struct cw_hardware		*hrd);



#endif /* !CW_DRIVER_H */
/******************************************************** Karsten Scheibler */

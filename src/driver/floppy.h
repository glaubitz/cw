/****************************************************************************
 ****************************************************************************
 *
 * floppy.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_FLOPPY_H
#define CW_FLOPPY_H

#include <linux/fs.h>

#include "types.h"
#include "ioctl.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct cw_floppies;
struct cw_controller;

#define CW_FLOPPY_MODEL_NONE		0
#define CW_FLOPPY_MODEL_AUTO		1
#define CW_FLOPPY_FORMAT_RAW		31

struct cw_floppy
	{
	int				num;
	struct cw_floppies		*fls;
	int				busy;
	wait_queue_head_t		busy_wq;
	int				model;
	int				format;
	int				track;
	int				track_wanted;
	int				track_dirty;
	int				motor;
	int				motor_request;
	struct timer_list		motor_timer;
	cw_raw_t			*track_data;
	struct cw_floppyinfo		fli;
	};

struct cw_floppies
	{
	struct cw_controller		*cnt;
	struct cw_floppy		flp[CW_NR_FLOPPIES_PER_CONTROLLER];
	spinlock_t			lock;
	volatile int			open;
	int				busy;
	wait_queue_head_t		busy_wq;
	wait_queue_head_t		step_wq;
	struct timer_list		step_timer;
	int				mux;
	struct timer_list		mux_timer;
	int				rw_latch;
	int				rw_timeout;
	int				rw_jiffies;
	wait_queue_head_t		rw_wq;
	struct timer_list		rw_timer;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern struct file_operations		cw_floppy_fops;
extern int				cw_floppy_init(struct cw_floppies *);
extern void				cw_floppy_exit(struct cw_floppies *);



#endif /* !CW_FLOPPY_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * floppy.c
 *
 ****************************************************************************
 ****************************************************************************/





#include "config.h"	/* includes <linux/config.h> if needed */
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "floppy.h"
#include "driver.h"
#include "hardware.h"
#include "message.h"
#include "ioctl.h"



#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define CW_FLOPPY_NO_SLEEP_ON
#endif /* LINUX_VERSION_CODE */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38)
#define CW_FLOPPY_UNLOCKED_IOCTL
#endif /* LINUX_VERSION_CODE */

#ifdef CW_FLOPPY_NO_SLEEP_ON
#define do_sleep_on(cond, wq)					\
	do							\
		{						\
		DEFINE_WAIT(w);					\
								\
		prepare_to_wait(wq, &w, TASK_UNINTERRUPTIBLE);	\
		if (cond) schedule();				\
		finish_wait(wq, &w);				\
		}						\
	while (0)
#else /* CW_FLOPPY_NO_SLEEP_ON */
#define do_sleep_on(cond, wq)		sleep_on(wq)
#endif /* CW_FLOPPY_NO_SLEEP_ON */

#define get_controller(minor)		((minor >> 6) & 3)
#define get_floppy(minor)		((minor >> 5) & 1)
#define get_format(minor)		(minor & 0x1f)
#define cnt_num				flp->fls->cnt->num
#define cnt_hrd				flp->fls->cnt->hrd



/****************************************************************************
 * cw_floppy_lock
 ****************************************************************************/
static int
cw_floppy_lock(
	wait_queue_head_t		*wq,
	spinlock_t			*lock,
	int				*busy,
	int				nonblock)

	{
	wait_queue_t			w;
	unsigned long			flags;
	int				result = 1;

	/*
	 * create a waitqueue entry and append it to the given waitqueue.
	 * if *busy indicates that the given resource is in use, we either
	 * return -EBUSY (if non blocking behaviour is wanted) or call
	 * schedule(). because we changed our current state, the
	 * schedule()-call will only return if someone woke up the given
	 * waitqueue, which happens in the unlock-functions
	 */

	init_waitqueue_entry(&w, current);
	add_wait_queue(wq, &w);
	while (1)
		{
		set_current_state(TASK_UNINTERRUPTIBLE);
		spin_lock_irqsave(lock, flags);
		if (! *busy) result = 0, *busy = 1;
		else if (nonblock) result = -EBUSY;
		spin_unlock_irqrestore(lock, flags);
		if (result <= 0) break;
		schedule();
		}

	/*
	 * if we had not called schedule() we have to change our current
	 * state back to TASK_RUNNING
	 */

	set_current_state(TASK_RUNNING);
	remove_wait_queue(wq, &w);
	return (result);
	}



/****************************************************************************
 * cw_floppy_lock_controller
 ****************************************************************************/
static int
cw_floppy_lock_controller(
	struct cw_floppies		*fls,
	int				nonblock)

	{
	int				result;

	cw_debug(1, "[c%d] trying to lock controller", fls->cnt->num);
	result = cw_floppy_lock(&fls->busy_wq, &fls->lock, &fls->busy, nonblock);
	if (result == 0) cw_debug(1, "[c%d] locking controller", fls->cnt->num);
	return (result);
	}



/****************************************************************************
 * cw_floppy_unlock_controller
 ****************************************************************************/
static void
cw_floppy_unlock_controller(
	struct cw_floppies		*fls)

	{
	cw_debug(1, "[c%d] unlocking controller", fls->cnt->num);
	fls->busy = 0;
	wake_up(&fls->busy_wq);
	}



/****************************************************************************
 * cw_floppy_lock_floppy
 ****************************************************************************/
static int
cw_floppy_lock_floppy(
	struct cw_floppy		*flp,
	int				nonblock)

	{
	int				result;

	cw_debug(1, "[c%df%d] trying to lock floppy", cnt_num, flp->num);
	result = cw_floppy_lock(&flp->busy_wq, &flp->fls->lock, &flp->busy, nonblock);
	if (result == 0) cw_debug(1, "[c%df%d] locking floppy", cnt_num, flp->num);
	return (result);
	}



/****************************************************************************
 * cw_floppy_unlock_floppy
 ****************************************************************************/
static void
cw_floppy_unlock_floppy(
	struct cw_floppy		*flp)

	{
	cw_debug(1, "[c%df%d] unlocking floppy", cnt_num, flp->num);
	flp->busy = 0;
	wake_up(&flp->busy_wq);
	}



/****************************************************************************
 * cw_floppy_default_parameters
 ****************************************************************************/
static struct cw_floppyinfo
cw_floppy_default_parameters(
	void)

	{
	struct cw_floppyinfo		fli =
		{
		.version       = CW_STRUCT_VERSION,
		.settle_time   = CW_DEFAULT_SETTLE_TIME,
		.step_time     = CW_DEFAULT_STEP_TIME,
		.wpulse_length = CW_DEFAULT_WPULSE_LENGTH,
		.nr_tracks     = CW_NR_TRACKS,
		.nr_sides      = CW_NR_SIDES,
		.nr_clocks     = CW_NR_CLOCKS,
		.nr_modes      = CW_NR_MODES,
		.max_size      = CW_MAX_TRACK_SIZE,
		.rpm           = 0,
		.flags         = 0
		};

	return (fli);
	}



/****************************************************************************
 * cw_floppy_get_parameters
 ****************************************************************************/
static int
cw_floppy_get_parameters(
	struct cw_floppy		*flp,
	struct cw_floppyinfo		*fli,
	int				nonblock)

	{
	int				result;

	if (fli->version != CW_STRUCT_VERSION) return (-EINVAL);
	result = cw_floppy_lock_floppy(flp, nonblock);
	if (result < 0) return (result);
	*fli = flp->fli;
	cw_floppy_unlock_floppy(flp);
	return (0);
	}



/****************************************************************************
 * cw_floppy_set_parameters
 ****************************************************************************/
static int
cw_floppy_set_parameters(
	struct cw_floppy		*flp,
	struct cw_floppyinfo		*fli,
	int				nonblock)

	{
	int				result;

	/* check if all values are valid */

	if (fli->version != CW_STRUCT_VERSION) return (-EINVAL);
	if ((fli->settle_time < CW_MIN_SETTLE_TIME) || (fli->settle_time > CW_MAX_SETTLE_TIME) ||
		(fli->step_time < CW_MIN_STEP_TIME) || (fli->step_time > CW_MAX_STEP_TIME) ||
		(fli->wpulse_length < CW_MIN_WPULSE_LENGTH) || (fli->wpulse_length > CW_MAX_WPULSE_LENGTH) ||
		(fli->nr_tracks < 1) || (fli->nr_tracks > CW_NR_TRACKS) ||
		(fli->nr_sides < 1) || (fli->nr_sides > CW_NR_SIDES) ||
		(fli->nr_clocks != flp->fli.nr_clocks) ||
		(fli->nr_modes != flp->fli.nr_modes) ||
		(fli->max_size != flp->fli.max_size) ||
		((fli->rpm != 0) && (fli->rpm < CW_MIN_RPM)) || (fli->rpm > CW_MAX_RPM) ||
		(fli->flags > CW_FLOPPYINFO_FLAG_ALL)) return (-EINVAL);

	/* copy values to driver data structures */

	result = cw_floppy_lock_floppy(flp, nonblock);
	if (result < 0) return (result);
	fli->wpulse_length /= CW_WPULSE_LENGTH_MULTIPLIER;
	fli->wpulse_length *= CW_WPULSE_LENGTH_MULTIPLIER;
	cw_debug(1, "[c%df%d] settle_time = %d, step_time = %d, wpulse_length = %d, nr_tracks = %d, nr_sides = %d, rpm = %d, flags = 0x%08x",
		 cnt_num, flp->num, fli->settle_time, fli->step_time,
		 fli->wpulse_length, fli->nr_tracks, fli->nr_sides,
		 fli->rpm, fli->flags);
	flp->fli.settle_time   = fli->settle_time;
	flp->fli.step_time     = fli->step_time;
	flp->fli.wpulse_length = fli->wpulse_length;
	flp->fli.nr_tracks     = fli->nr_tracks;
	flp->fli.nr_sides      = fli->nr_sides;
	flp->fli.rpm           = fli->rpm;
	flp->fli.flags         = fli->flags;
	cw_floppy_unlock_floppy(flp);
	return (0);
	}



/****************************************************************************
 * cw_floppy_check_parameters
 ****************************************************************************/
static int
cw_floppy_check_parameters(
	struct cw_floppyinfo		*fli,
	struct cw_trackinfo		*tri,
	int				write)

	{
	if (tri->version != CW_STRUCT_VERSION) return (-EINVAL);
	if ((tri->track_seek >= fli->nr_tracks) || (tri->track >= fli->nr_tracks) ||
		(tri->side >= fli->nr_sides) || (tri->clock >= fli->nr_clocks) ||
		(tri->mode >= fli->nr_modes) || (tri->timeout < CW_MIN_TIMEOUT) ||
		(tri->timeout > CW_MAX_TIMEOUT)) return (-EINVAL);
	if ((write) && ((tri->size > fli->max_size - CW_WRITE_OVERHEAD) ||
		(tri->mode == CW_TRACKINFO_MODE_INDEX_STORE))) return (-EINVAL);
	return (0);
	}



/****************************************************************************
 * cw_floppy_get_density
 ****************************************************************************/
static int
cw_floppy_get_density(
	struct cw_floppy		*flp)

	{
	return ((flp->fli.flags & CW_FLOPPYINFO_FLAG_DENSITY) ? 1 : 0);
	}



/****************************************************************************
 * cwfloppy_add_timer
 ****************************************************************************/
#define cwfloppy_add_timer(t, e, d)	cw_floppy_add_timer2(t, e, d)



/****************************************************************************
 * cw_floppy_add_timer2
 ****************************************************************************/
static void
cw_floppy_add_timer2(
	struct timer_list		*timer,
	unsigned long			expires,
	void				*data)

	{
	timer->expires = expires;
	timer->data    = (cw_ptr_t) data;
	add_timer(timer);
	}



/****************************************************************************
 * cw_floppy_motor_wait
 ****************************************************************************/
static void
cw_floppy_motor_wait(
	struct cw_floppy		*flp)

	{
	cw_debug(2, "[c%df%d] will wait for motor spin up", cnt_num, flp->num);
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(HZ);
	cw_debug(2, "[c%df%d] spin up done", cnt_num, flp->num);
	}



/****************************************************************************
 * cw_floppy_motor_on
 ****************************************************************************/
static void
cw_floppy_motor_on(
	struct cw_floppy		*flp)

	{
	unsigned long			flags;

	/*
	 * delete pending cw_floppy_motor_timer_func(),
	 * set flp->motor_request = 0 under flp->fls->lock, this will
	 * ensure that no parallel running cw_floppy_mux_timer_func()
	 * already read flp->motor_request and changes the state of
	 * flp->motor after we checked flp->motor
	 */

	cw_debug(1, "[c%df%d] motor on", cnt_num, flp->num);
	del_timer_sync(&flp->motor_timer);
	spin_lock_irqsave(&flp->fls->lock, flags);
	flp->motor_request = 0;
	spin_unlock_irqrestore(&flp->fls->lock, flags);

	/* motor is still on, nothing to do */

	if (flp->motor) return;

	/*
	 * switch floppy motor on, wait for spin up. this floppy stays
	 * locked, so only the other floppy on this controller may be used
	 * in the mean time
	 */

	spin_lock_irqsave(&flp->fls->lock, flags);
	cw_hardware_floppy_motor_on(&cnt_hrd, flp->num);
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	cw_floppy_motor_wait(flp);
	flp->motor = 1;
	}



/****************************************************************************
 * cw_floppy_motor_timer_func
 ****************************************************************************/
static void
cw_floppy_motor_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;

	/* just signal cw_floppy_mux_timer_func() to switch motor off */

	cw_debug(2, "[c%df%d] preparing to switch motor off", cnt_num, flp->num);
	flp->motor_request = 1;
	}



/****************************************************************************
 * cw_floppy_motor_off
 ****************************************************************************/
static void
cw_floppy_motor_off(
	struct cw_floppy		*flp)

	{

	/*
	 * always called after cw_floppy_motor_on(), so there is no
	 * cw_floppy_motor_timer_func() running from a previous
	 * cw_floppy_motor_off()-call
	 */

	cw_debug(2, "[c%df%d] registering motor_timer", cnt_num, flp->num);
	cwfloppy_add_timer(&flp->motor_timer, jiffies + 2 * HZ, flp);
	}



/****************************************************************************
 * cw_floppy_step_timer_func
 ****************************************************************************/
static void
cw_floppy_step_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;
	int				direction = 1;
	int				time = flp->fli.settle_time;

	/* check if already at wanted position */

	if (flp->track_wanted == flp->track)
		{
		wake_up(&flp->fls->step_wq);
		return;
		}

	/* check if explicit track0 sensing is wanted */

	if (flp->track_wanted < 0)
		{
		cw_debug(2, "[c%df%d] doing explicit track0 sensing", cnt_num, flp->num);
		if (cw_hardware_floppy_track0(&cnt_hrd))
			{
			flp->track        = 0;
			flp->track_wanted = 0;
			goto done;
			}
		}

	/* step one track */

	if (flp->track_wanted < flp->track) direction = 0;
	cw_debug(2, "[c%df%d] track_wanted = %d, track = %d, direction = %d", cnt_num, flp->num, flp->track_wanted, flp->track, direction);

	/*
	 * the spin_is_locked() call may be obsolete, but for the case that
	 * this timer routine interrupts another one of cwfloppy, which
	 * currently holds flp->fls->lock. without spin_is_locked() this
	 * would result in a dead lock. i think timer routines interrupting
	 * other timer routines will never happen on current linux kernels
	 */

	if (! spin_is_locked(&flp->fls->lock))
		{
		spin_lock(&flp->fls->lock);
		cw_hardware_floppy_step(&cnt_hrd, 1, direction);
		udelay(50);
		cw_hardware_floppy_step(&cnt_hrd, 0, direction);
		spin_unlock(&flp->fls->lock);
		flp->track += direction ? 1 : -1;
		if (flp->track_wanted != flp->track) time = flp->fli.step_time;
		}
	else cw_debug(2, "[c%df%d] will try again", cnt_num, flp->num);

	/* schedule next call */
done:
	cwfloppy_add_timer(&flp->fls->step_timer, jiffies + (time * HZ + 999) / 1000, flp);
	}



/****************************************************************************
 * cw_floppy_step
 ****************************************************************************/
static int
cw_floppy_step(
	struct cw_floppy		*flp,
	int				track_wanted)

	{
	flp->track_wanted = track_wanted;
	if (flp->track_wanted == flp->track) return (0);
	cw_debug(1, "[c%df%d] registering floppies_step_timer", cnt_num, flp->num);
	cwfloppy_add_timer(&flp->fls->step_timer, jiffies + 2, flp);
	do_sleep_on(flp->track_wanted != flp->track, &flp->fls->step_wq);
	cw_debug(1, "[c%df%d] track_wanted(%d) == track(%d)", cnt_num, flp->num, flp->track_wanted, flp->track);
	return (1);
	}



/****************************************************************************
 * cw_floppy_dummy_step
 ****************************************************************************/
static int
cw_floppy_dummy_step(
	struct cw_floppy		*flp)

	{
	int				track = flp->track;
	int				ofs = (track > 0) ? 1 : -1;

	cw_floppy_step(flp, track - ofs);
	cw_floppy_step(flp, track + ofs);
	return (1);
	}



/****************************************************************************
 * cw_floppy_calibrate
 ****************************************************************************/
static int
cw_floppy_calibrate(
	struct cw_floppy		*flp)

	{
	flp->track = 0;
	cw_floppy_step(flp, 3);
	cw_floppy_step(flp, -CW_NR_TRACKS);
	flp->track_dirty = 0;
	return (1);
	}



/****************************************************************************
 * cw_floppy_get_model
 ****************************************************************************/
static int
cw_floppy_get_model(
	struct cw_floppy		*flp)

	{

	/*
	 * turn the motor on, because some drives will not seek if the motor
	 * is off
	 */

	cw_hardware_floppy_motor_on(&cnt_hrd, flp->num);
	cw_floppy_motor_wait(flp);

	/*
	 * a floppy is present if we get a track0 signal while moving
	 * the head (seek), this routine is only called from cw_floppy_init(),
	 * so no concurrent accesses to the control register via
	 * cwhardware_*()-functions may happen, so no protection with
	 * flp->fls->lock is needed
	 */

	cw_hardware_floppy_select(&cnt_hrd, flp->num, 0, cw_floppy_get_density(flp));
	cw_floppy_calibrate(flp);

	/* this automatically deselects the floppy */

	cw_hardware_floppy_motor_off(&cnt_hrd, flp->num);
	if (flp->track == 0) return (CW_FLOPPY_MODEL_AUTO);
	return (CW_FLOPPY_MODEL_NONE);
	}



/****************************************************************************
 * cw_floppy_mux_timer_func
 ****************************************************************************/
static void
cw_floppy_mux_timer_func(
	unsigned long			arg)

	{
	struct cw_floppies		*fls = (struct cw_floppies *) arg;
	struct cw_floppy		*flp;
	int				f, open;

	/*
	 * the spin_is_locked() call may be obsolete, but for the case that
	 * this timer routine interrupts another one of cwfloppy, which
	 * currently holds flp->fls->lock. without spin_is_locked() this
	 * would result in a dead lock. i think timer routines interrupting
	 * other timer routines will never happen on current linux kernels
	 */

	if (spin_is_locked(&fls->lock)) goto done;
	spin_lock(&fls->lock);
	for (f = 0, open = fls->open; f < CW_NR_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp = &fls->flp[f];
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;
		if (! flp->motor_request)
			{
			open += flp->motor;
			continue;
			}
		cw_hardware_floppy_motor_off(&cnt_hrd, flp->num);
		flp->motor_request = 0;
		flp->motor         = 0;
		}
	if ((! fls->mux) && (! open))
		{
		cw_hardware_floppy_host_select(&cnt_hrd);
		cw_hardware_floppy_mux_on(&fls->cnt->hrd);
		fls->mux = 1;
		}
	spin_unlock(&fls->lock);

	/* wait at least 100 ms until next check */
done:
	cwfloppy_add_timer(&fls->mux_timer, jiffies + (100 * HZ + 999) / 1000, fls);
	}



/****************************************************************************
 * cw_floppy_rw_timer_func
 ****************************************************************************/
static void
cw_floppy_rw_timer_func(
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) arg;
	int				diff, busy;

	/*
	 * check if operation finished (with indexed read floppy gets busy
	 * only after the index pulse, we are done only if last state was
	 * busy and floppy is now not busy)
	 */

	busy = cw_hardware_floppy_busy(&cnt_hrd);
	if ((! busy) && (flp->fls->rw_latch))
		{
		flp->fls->rw_timeout = 0;
		goto done;
		}
	flp->fls->rw_latch = busy;

	/*
	 * on jiffies overflow some precision is lost for timeout
	 * calculation (we may wait a little bit longer than requested)
	 */

	if (flp->fls->rw_jiffies > jiffies) diff = 0;
	else diff = jiffies - flp->fls->rw_jiffies;
	flp->fls->rw_timeout -= (1000 * diff) / HZ;
	flp->fls->rw_jiffies = jiffies;
	cw_debug(2, "[c%df%d] timeout = %d ms, latch = %d", cnt_num, flp->num, flp->fls->rw_timeout, flp->fls->rw_latch);

	/* check if operation timed out */

	if (flp->fls->rw_timeout <= 0)
		{
		cw_hardware_floppy_abort(&cnt_hrd);
		flp->fls->rw_timeout = -1;
done:
		wake_up(&flp->fls->rw_wq);
		return;
		}

	/* wait at least 10 ms until next check */

	cwfloppy_add_timer(&flp->fls->rw_timer, jiffies + (10 * HZ + 999) / 1000, flp);
	}



/****************************************************************************
 * cw_floppy_read_write_track
 ****************************************************************************/
static int
cw_floppy_read_write_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri,
	int				nonblock,
	int				write)

	{
	int				result, aborted = 0, stepped = 0;
	unsigned long			flags;

	/*
	 * motor on, lock controller, select floppy and move head to
	 * track_seek. with this "preposition track" it is possible to
	 * specify the direction from which a track is reached, because
	 * depending on drive hardware it may influence the final head
	 * position if the track was stepped on from left or right
	 */

	cw_floppy_motor_on(flp);
	result = cw_floppy_lock_controller(flp->fls, nonblock);
	if (result < 0) goto done2;
	spin_lock_irqsave(&flp->fls->lock, flags);
	cw_hardware_floppy_select(&cnt_hrd, flp->num, tri->side, cw_floppy_get_density(flp));
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	if (flp->track_dirty) stepped = cw_floppy_calibrate(flp);
	stepped |= cw_floppy_step(flp, tri->track_seek);

	/*
	 * check if disk is in drive. a set disk change flag means the disk
	 * was removed from drive. if a disk was inserted in the mean time,
	 * we need to step to update the flag
	 */

	if (! (flp->fli.flags & CW_FLOPPYINFO_FLAG_IGNORE_DISKCHANGE))
		{
		int			invert;

		invert = (flp->fli.flags & CW_FLOPPYINFO_FLAG_INVERTED_DISKCHANGE) ? 1 : 0;
		result = -EIO;
		while ((cw_hardware_floppy_disk_changed(&cnt_hrd) ^ invert) != 0)
			{
			if (stepped) goto done;
			stepped = cw_floppy_dummy_step(flp);
			}
		}

	/* move head to final destination */

	cw_floppy_step(flp, tri->track);
	
	/* start reading or writing now */

	if (write)
		{
		if (cw_hardware_floppy_write_protected(&cnt_hrd)) result = -EROFS;
		else result = cw_hardware_floppy_write_track(&cnt_hrd, tri->clock, tri->mode, flp->fli.wpulse_length, flp->track_data, tri->size);
		if (result < 0) goto done;
		}
	else cw_hardware_floppy_read_track_start(&cnt_hrd, tri->clock, tri->mode);

	/* wait until operation finished or timed out */

	cw_debug(1, "[c%df%d] registering floppies_rw_timer", cnt_num, flp->num);
	flp->fls->rw_latch   = cw_hardware_floppy_busy(&cnt_hrd);
	flp->fls->rw_jiffies = jiffies;
	flp->fls->rw_timeout = tri->timeout;
	cwfloppy_add_timer(&flp->fls->rw_timer, jiffies + 2, flp);
	do_sleep_on(flp->fls->rw_timeout > 0, &flp->fls->rw_wq);
	if (flp->fls->rw_timeout < 0) aborted = 1;
	cw_debug(1, "[c%df%d] track operation done, aborted = %d", cnt_num, flp->num, aborted);

	/*
	 * do remaining actions
	 *
	 * on read:  get the data from catweasel memory,
	 * on write: decrease the written bytes by one if the write was
	 *           aborted to signal the application that not all bytes
	 *           were written to disk
	 */

	if (write) result -= aborted;
	else result = cw_hardware_floppy_read_track_copy(&cnt_hrd, flp->track_data, tri->size);

	/* done */
done:
	cw_floppy_unlock_controller(flp->fls);
done2:
	cw_floppy_motor_off(flp);
	return (result);
	}



/****************************************************************************
 * cw_floppy_read_track
 ****************************************************************************/
static int
cw_floppy_read_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri,
	int				nonblock)

	{
	int				result;

	/* check parameters */

	result = cw_floppy_check_parameters(&flp->fli, tri, 0);
	if (result < 0) return (result);
	if (tri->size == 0) return (0);
	if (! access_ok(VERIFY_WRITE, tri->data, tri->size)) return (-EFAULT);

	/* read track and copy data to user space */

	result = cw_floppy_lock_floppy(flp, nonblock);
	if (result < 0) return (result);
	result = cw_floppy_read_write_track(flp, tri, nonblock, 0);
	if ((result > 0) && (copy_to_user(tri->data, flp->track_data, result) != 0)) result = -EFAULT;
	cw_floppy_unlock_floppy(flp);
	return (result);
	}



/****************************************************************************
 * cw_floppy_write_track
 ****************************************************************************/
static int
cw_floppy_write_track(
	struct cw_floppy		*flp,
	struct cw_trackinfo		*tri,
	int				nonblock)

	{
	int				result;

	/* check parameters */

	result = cw_floppy_check_parameters(&flp->fli, tri, 1);
	if (result < 0) return (result);
	if (tri->size == 0) return (0);
	if (! access_ok(VERIFY_READ, tri->data, tri->size)) return (-EFAULT);

	/* copy data from user space and write track */

	result = cw_floppy_lock_floppy(flp, nonblock);
	if (result < 0) return (result);
	result = -EFAULT;
	if (copy_from_user(flp->track_data, tri->data, tri->size) == 0) result = cw_floppy_read_write_track(flp, tri, nonblock, 1);
	cw_floppy_unlock_floppy(flp);
	return (result);
	}



/****************************************************************************
 * cw_floppy_char_open
 ****************************************************************************/
static int
cw_floppy_char_open(
	struct inode			*inode,
	struct file			*file)

	{
	struct cw_floppies		*fls;
	struct cw_floppy		*flp;
	int				f, minor = MINOR(inode->i_rdev);
	int				select1, select2;
	unsigned long			flags;

	cw_debug(1, "[c%df%d] open()", get_controller(minor), get_floppy(minor));
	fls = cw_driver_get_floppies(get_controller(minor));
	f   = get_floppy(minor);
	if ((fls == NULL) || (f >= CW_NR_FLOPPIES_PER_CONTROLLER)) return (-ENODEV);
	flp = &fls->flp[f];
	if ((get_format(minor) != CW_FLOPPY_FORMAT_RAW) || (flp->model == CW_FLOPPY_MODEL_NONE)) return (-ENODEV);

	/*
	 * check if host controller accessed the floppies in the past or is
	 * still accessing them, host select is latched, so we need to read
	 * it twice to get current state. if host controller accessed a
	 * floppy the head position may have changed, we mark it as dirty
	 * and recalibrate the floppy on next access (this is only relevant
	 * with mk4 controllers)
	 */

	spin_lock_irqsave(&fls->lock, flags);
	select1 = cw_hardware_floppy_host_select(&cnt_hrd);
	select2 = cw_hardware_floppy_host_select(&cnt_hrd);
	select1 |= select2;
	if (fls->mux)
		{
		if (select1 & 1) fls->flp[0].track_dirty = 1;
		if (select1 & 2) fls->flp[1].track_dirty = 1;
		if (select2)
			{
			spin_unlock_irqrestore(&fls->lock, flags);
			return (-EBUSY);
			}
		cw_hardware_floppy_mux_off(&cnt_hrd);
		fls->mux = 0;
		}
	fls->open++;
	spin_unlock_irqrestore(&fls->lock, flags);

	/* open succeeded */

	cw_debug(1, "[c%df%d] open() succeeded", cnt_num, flp->num);
	file->private_data = flp;
	return (0);
	}



/****************************************************************************
 * cw_floppy_char_release
 ****************************************************************************/
static int
cw_floppy_char_release(
	struct inode			*inode,
	struct file			*file)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) file->private_data;
	unsigned long			flags;

	cw_debug(1, "[c%df%d] close()", cnt_num, flp->num);
	spin_lock_irqsave(&flp->fls->lock, flags);
	flp->fls->open--;
	spin_unlock_irqrestore(&flp->fls->lock, flags);
	return (0);
	}



/****************************************************************************
 * cw_floppy_char_unlocked_ioctl
 ****************************************************************************/
static ssize_t
cw_floppy_char_unlocked_ioctl(
	struct file			*file,
	unsigned int			cmd,
	unsigned long			arg)

	{
	struct cw_floppy		*flp = (struct cw_floppy *) file->private_data;
	struct cw_trackinfo		tri;
	struct cw_floppyinfo		fli;
	int				nonblock = (file->f_flags & O_NONBLOCK) ? 1 : 0;
	int				result   = -ENOTTY;

	if (cmd == CW_IOC_GFLPARM)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_GFLPARM, ...)", cnt_num, flp->num);

		/*
		 * need to allow O_WRONLY here, because on write we use
		 * CW_IOC_GFLPARM while holding the device open with
		 * O_WRONLY
		 */

		result = -EFAULT;
		if (copy_from_user(&fli, (void *) arg, sizeof (struct cw_floppyinfo)) == 0) result = cw_floppy_get_parameters(flp, &fli, nonblock);
		if ((result == 0) && (copy_to_user((void *) arg, &fli, sizeof (struct cw_floppyinfo)) != 0)) result = -EFAULT;
		}
	else if (cmd == CW_IOC_SFLPARM)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_SFLPARM, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_RDONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&fli, (void *) arg, sizeof (struct cw_floppyinfo)) == 0) result = cw_floppy_set_parameters(flp, &fli, nonblock);
		}
	else if (cmd == CW_IOC_READ)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_READ, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_WRONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&tri, (void *) arg, sizeof (struct cw_trackinfo)) == 0) result = cw_floppy_read_track(flp, &tri, nonblock);
		}
	else if (cmd == CW_IOC_WRITE)
		{
		cw_debug(1, "[c%df%d] ioctl(CW_IOC_WRITE, ...)", cnt_num, flp->num);
		if ((file->f_flags & O_ACCMODE) == O_RDONLY) return (-EPERM);
		result = -EFAULT;
		if (copy_from_user(&tri, (void *) arg, sizeof (struct cw_trackinfo)) == 0) result = cw_floppy_write_track(flp, &tri, nonblock);
		}
	return (result);
	}



#ifndef CW_FLOPPY_UNLOCKED_IOCTL
/****************************************************************************
 * cw_floppy_char_ioctl
 ****************************************************************************/
static ssize_t
cw_floppy_char_ioctl(
	struct inode			*inode,
	struct file			*file,
	unsigned int			cmd,
	unsigned long			arg)

	{
	return (cw_floppy_char_unlocked_ioctl(file, cmd, arg));
	}
#endif /* !CW_FLOPPY_UNLOCKED_IOCTL */



/****************************************************************************
 * cw_floppy_fops
 ****************************************************************************/
struct file_operations			cw_floppy_fops =
	{
	.owner          = THIS_MODULE,
	.open           = cw_floppy_char_open,
	.release        = cw_floppy_char_release,
#ifdef CW_FLOPPY_UNLOCKED_IOCTL
	.unlocked_ioctl = cw_floppy_char_unlocked_ioctl
#else /* CW_FLOPPY_UNLOCKED_IOCTL */
	.ioctl          = cw_floppy_char_ioctl
#endif /* CW_FLOPPY_UNLOCKED_IOCTL */
	};



/****************************************************************************
 * cw_floppy_init
 ****************************************************************************/
int
cw_floppy_init(
	struct cw_floppies		*fls)

	{
	struct cw_floppy		*flp;
	int				f;

	/*
	 * read host select status twice, because it is latched, the second
	 * read gives the current state
	 */

	cw_hardware_floppy_host_select(&fls->cnt->hrd);
	if (cw_hardware_floppy_host_select(&fls->cnt->hrd))
		{
		cw_error("[c%d] could not access floppy, host controller currently uses it", fls->cnt->num);
		return (-EBUSY);
		}

	/* disallow host controller accesses */

	cw_hardware_floppy_mux_off(&fls->cnt->hrd);

	/* per controller initialization */

	spin_lock_init(&fls->lock);
	init_waitqueue_head(&fls->busy_wq);
	init_waitqueue_head(&fls->step_wq);
	init_waitqueue_head(&fls->rw_wq);
	init_timer(&fls->step_timer);
	init_timer(&fls->mux_timer);
	init_timer(&fls->rw_timer);
	fls->step_timer.function = cw_floppy_step_timer_func;
	fls->mux_timer.function  = cw_floppy_mux_timer_func;
	fls->rw_timer.function   = cw_floppy_rw_timer_func;

	/* per floppy initialization */

	for (f = 0; f < CW_NR_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp      = &fls->flp[f];
		flp->num = f;
		flp->fls = fls;
		cw_debug(1, "[c%df%d] initializing", cnt_num, flp->num);
		init_waitqueue_head(&flp->busy_wq);
		init_timer(&flp->motor_timer);
		flp->fli                  = cw_floppy_default_parameters();
		flp->motor_timer.function = cw_floppy_motor_timer_func;
		flp->model                = cw_floppy_get_model(flp);
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;

		cw_notice("[c%df%d] floppy found at 0x%04x", cnt_num, flp->num, cnt_hrd.iobase);
		flp->track_data = (cw_raw_t *) vmalloc(flp->fli.max_size);
		if (flp->track_data != NULL) continue;

		cw_error("[c%df%d] could not allocate memory for track buffer", cnt_num, flp->num);
		return (-ENOMEM);
		}

	/* now host controller accesses are allowed again */

	cw_hardware_floppy_mux_on(&fls->cnt->hrd);
	fls->mux = 1;
	cwfloppy_add_timer(&fls->mux_timer, jiffies + 2, fls);
	return (0);
	}



/****************************************************************************
 * cw_floppy_exit
 ****************************************************************************/
void
cw_floppy_exit(
	struct cw_floppies		*fls)

	{
	struct cw_floppy		*flp;
	int				f;

	del_timer_sync(&fls->mux_timer);
	for (f = 0; f < CW_NR_FLOPPIES_PER_CONTROLLER; f++)
		{
		flp = &fls->flp[f];
		if (flp->model == CW_FLOPPY_MODEL_NONE) continue;

		cw_debug(1, "[c%df%d] deinitializing", cnt_num, flp->num);
		del_timer_sync(&flp->motor_timer);
		flp->motor_request = 0;
		if (flp->motor) cw_hardware_floppy_motor_off(&cnt_hrd, flp->num);
		if (flp->track_data == NULL) continue;

		vfree(flp->track_data);
		}
	cw_hardware_floppy_mux_on(&fls->cnt->hrd);
	fls->mux = 1;
	}
/******************************************************** Karsten Scheibler */

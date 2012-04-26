/****************************************************************************
 ****************************************************************************
 *
 * hardware.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_HARDWARE_H
#define CW_HARDWARE_H

#include "config.h"	/* includes <linux/config.h> if needed */
#include <linux/pci.h>

#include "types.h"




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




struct cw_controller;

#define CW_HARDWARE_MODEL_NONE		0
#define CW_HARDWARE_MODEL_MK3		3
#define CW_HARDWARE_MODEL_MK4		4

#define CW_HARDWARE_FLAG_NONE		0
#define CW_HARDWARE_FLAG_WPULSE_LENGTH	(1 << 0)

struct cw_hardware
	{
	struct cw_controller		*cnt;
	int				model;
	int				iobase;
	int				flags;
	volatile unsigned long		control_register;
	};




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




#ifdef CONFIG_PCI
extern struct pci_driver		cw_hardware_mk3_pci_driver;
extern struct pci_driver		cw_hardware_mk4_pci_driver;
#endif /* CONFIG_PCI */
extern int				cw_hardware_floppy_host_select(struct cw_hardware *);
extern void				cw_hardware_floppy_mux_on(struct cw_hardware *);
extern void				cw_hardware_floppy_mux_off(struct cw_hardware *);
extern void				cw_hardware_floppy_motor_on(struct cw_hardware *, int);
extern void				cw_hardware_floppy_motor_off(struct cw_hardware *, int);
extern void				cw_hardware_floppy_select(struct cw_hardware *, int, int, int);
extern void				cw_hardware_floppy_step(struct cw_hardware *, int, int);
extern int				cw_hardware_floppy_track0(struct cw_hardware *);
extern int				cw_hardware_floppy_write_protected(struct cw_hardware *);
extern int				cw_hardware_floppy_disk_changed(struct cw_hardware *);
extern void				cw_hardware_floppy_abort(struct cw_hardware *);
extern int				cw_hardware_floppy_busy(struct cw_hardware *);
extern void				cw_hardware_floppy_read_track_start(struct cw_hardware *, int, int);
extern int				cw_hardware_floppy_read_track_copy(struct cw_hardware *, unsigned char *, int);
extern int				cw_hardware_floppy_write_track(struct cw_hardware *, int, int, cw_psecs_t, unsigned char *, int);



#endif /* !CW_HARDWARE_H */
/******************************************************** Karsten Scheibler */

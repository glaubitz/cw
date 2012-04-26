/****************************************************************************
 ****************************************************************************
 *
 * drive.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_DRIVE_H
#define CWTOOL_DRIVE_H

#include "types.h"
#include "global.h"

struct drive
	{
	char				path[GLOBAL_MAX_PATH_SIZE];
	char				info[GLOBAL_MAX_NAME_SIZE];
	int				revision;
	struct cw_floppyinfo		fli;
	};

extern struct drive			*drive_get(int);
extern struct drive			*drive_search(const char *);
extern int				drive_init(struct drive *, int);
extern int				drive_insert(struct drive *);
extern int				drive_set_path(struct drive *, const char *);
extern int				drive_set_info(struct drive *, const char *);
extern int				drive_set_settle_time(struct drive *, int);
extern int				drive_set_step_time(struct drive *, int);
extern int				drive_set_wpulse_length(struct drive *, int);
extern int				drive_set_flag(struct drive *, int, int);
extern const char			*drive_get_path(struct drive *);
extern const char			*drive_get_info(struct drive *);
extern int				drive_init_all_devices(void);

#define drive_set_inverted_diskchange(d, v)	drive_set_flag(d, v, CW_FLOPPYINFO_FLAG_INVERTED_DISKCHANGE)
#define drive_set_ignore_diskchange(d, v)	drive_set_flag(d, v, CW_FLOPPYINFO_FLAG_IGNORE_DISKCHANGE)
#define drive_set_density(d, v)			drive_set_flag(d, v, CW_FLOPPYINFO_FLAG_DENSITY)
#define drive_set_double_step(d, v)		drive_set_flag(d, v, CW_FLOPPYINFO_FLAG_DOUBLE_STEP)



#endif /* !CWTOOL_DRIVE_H */
/******************************************************** Karsten Scheibler */

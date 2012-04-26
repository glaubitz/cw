/****************************************************************************
 ****************************************************************************
 *
 * format/desc.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_DESC_H
#define CWTOOL_FORMAT_DESC_H

#include "types.h"
#include "container.h"

union format;
struct fifo;
struct disk_sector;

/* UGLY: better use struct format_operations ? */

struct format_desc
	{
	char				*name;
	int				level;
	void				(*set_defaults)(union format *);
	int				(*set_read_option)(union format *, int, int, int);
	int				(*set_write_option)(union format *, int, int, int);
	int				(*set_rw_option)(union format *, int, int, int);
	int				(*get_sectors)(union format *);
	int				(*get_sector_size)(union format *, int);
	int				(*get_flags)(union format *);
	int				(*get_data_offset)(union format *);
	int				(*get_data_size)(union format *);
	int				(*track_statistics)(union format *, struct fifo *, int, int, int);
	int				(*track_read)(union format *, struct container *, struct fifo *, struct fifo *, struct disk_sector *, int, int, int);
	int				(*track_write)(union format *, struct fifo *, struct disk_sector *, struct fifo *, unsigned char *, int, int, int);
	struct format_option		*fmt_opt_rd;
	struct format_option		*fmt_opt_wr;
	struct format_option		*fmt_opt_rw;
	};



#endif /* !CWTOOL_FORMAT_DESC_H */
/******************************************************** Karsten Scheibler */

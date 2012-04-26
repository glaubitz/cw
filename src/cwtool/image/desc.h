/****************************************************************************
 ****************************************************************************
 *
 * image/desc.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_DESC_H
#define CWTOOL_IMAGE_DESC_H

#include "types.h"

union image;
struct image_track;
struct disk_sector;
struct fifo;

/* UGLY: better use struct image_operations ? */

struct image_desc
	{
	char				*name;
	int				level;
	cw_flag_t			flags;
	int				(*open)(union image *, char *, int, int);
	int				(*close)(union image *);
	int				(*offset)(union image *);
	int				(*track_read)(union image *, struct image_track *, struct fifo *, struct disk_sector *, int, int);
	int				(*track_write)(union image *, struct image_track *, struct fifo *, struct disk_sector *, int, int);
	int				(*track_done)(union image *, struct image_track *, int);
	};



#endif /* !CWTOOL_IMAGE_DESC_H */
/******************************************************** Karsten Scheibler */

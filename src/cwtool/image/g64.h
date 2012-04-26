/****************************************************************************
 ****************************************************************************
 *
 * image/g64.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_G64_H
#define CWTOOL_IMAGE_G64_H

#include "types.h"
#include "../file.h"
#include "desc.h"

#define IMAGE_G64_MAX_TRACK		84

struct image_g64_track
	{
	unsigned char			*data;
	int				size;
	int				speed;
	int				used;
	};

struct image_g64
	{
	struct file			fil;
	struct image_g64_track		trk[IMAGE_G64_MAX_TRACK];
	int				flags;
	};

extern struct image_desc		image_g64_desc;



#endif /* !CWTOOL_IMAGE_G64_H */
/******************************************************** Karsten Scheibler */

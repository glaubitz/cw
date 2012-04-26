/****************************************************************************
 ****************************************************************************
 *
 * image/d64.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_D64_H
#define CWTOOL_IMAGE_D64_H

#include "types.h"
#include "../global.h"
#include "../file.h"
#include "desc.h"

struct image_d64
	{
	struct file			fil;
	int				offset;
	int				flags;
	int				sectors[GLOBAL_NR_TRACKS];
	unsigned char			errors[GLOBAL_NR_TRACKS][GLOBAL_NR_SECTORS];
	};

extern struct image_desc		image_d64_desc;
extern struct image_desc		image_d64_noerror_desc;



#endif /* !CWTOOL_IMAGE_D64_H */
/******************************************************** Karsten Scheibler */

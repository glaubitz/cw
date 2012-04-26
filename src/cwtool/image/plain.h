/****************************************************************************
 ****************************************************************************
 *
 * image/plain.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_PLAIN_H
#define CWTOOL_IMAGE_PLAIN_H

#include "types.h"
#include "../file.h"
#include "desc.h"

struct image_plain
	{
	struct file			fil;
	int				offset;
	int				flags;
	};

extern struct image_desc		image_plain_desc;



#endif /* !CWTOOL_IMAGE_PLAIN_H */
/******************************************************** Karsten Scheibler */

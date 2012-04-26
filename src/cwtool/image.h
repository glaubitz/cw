/****************************************************************************
 ****************************************************************************
 *
 * image.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_H
#define CWTOOL_IMAGE_H

#include "types.h"
#include "global.h"
#include "image/raw.h"
#include "image/g64.h"
#include "image/d64.h"
#include "image/plain.h"
#include "image/desc.h"

union image
	{
	struct image_raw		raw;
	struct image_g64		g64;
	struct image_d64		d64;
	struct image_plain		pln;
	};

#define IMAGE_FLAG_NONE			0
#define IMAGE_FLAG_SAME_TRACK		(1 << 0)
#define IMAGE_FLAG_CONTINUOUS_TRACK	(1 << 1)
#define IMAGE_FLAG_84_TRACKS		(1 << 2)

/*
 * although every image gets struct image_track, this struct is currently
 * only relevant for image "raw", all the other image formats don't access
 * it
 */

#define IMAGE_TRACK_INIT(t)		(struct image_track) { .timeout_read = t, .timeout_write = t }
#define IMAGE_TRACK_FLAG_INDEXED_READ	(1 << 0)
#define IMAGE_TRACK_FLAG_INDEXED_WRITE	(1 << 1)
#define IMAGE_TRACK_FLAG_FLIP_SIDE	(1 << 2)
#define IMAGE_TRACK_FLAG_OPTIONAL	(1 << 3)

struct image_track
	{
	unsigned char			flags;
	unsigned char			clock;
	unsigned char			side_offset;
	unsigned char			reserved;
	unsigned short			timeout_read;
	unsigned short			timeout_write;
	};

#define IMAGE_MODE_READ			1
#define IMAGE_MODE_WRITE		2

#define IMAGE_FLAG_NONE			0
#define IMAGE_FLAG_IGNORE_SIZE		(1 << 0)

struct file;

extern struct image_desc		*image_search_desc(const char *);
extern int				image_open(union image *, struct file *, char *, int);
extern int				image_close(union image *, struct file *);



#endif /* !CWTOOL_IMAGE_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * image/plain.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "plain.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../fifo.h"
#include "../file.h"
#include "../image.h"



#define FLAG_IGNORE_SIZE		(1 << 0)
#define FLAG_END_SEEN			(1 << 1)



/****************************************************************************
 * image_plain_open
 ****************************************************************************/
static int
image_plain_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	image_open(img, &img->pln.fil, path, mode);
	if (flags & IMAGE_FLAG_IGNORE_SIZE) img->pln.flags = FLAG_IGNORE_SIZE;
	return (1);
	}



/****************************************************************************
 * image_plain_close
 ****************************************************************************/
static int
image_plain_close(
	union image			*img)

	{
	unsigned char			buffer[1];

	if (file_is_writable(&img->pln.fil)) goto done;
	if (img->pln.flags & (FLAG_IGNORE_SIZE | FLAG_END_SEEN)) goto done;
	if (file_read(&img->pln.fil, buffer, 1) == 0) goto done;
	error_warning("file '%s' is larger than needed", file_get_path(&img->pln.fil));
done:
	return (image_close(img, &img->pln.fil));
	}



/****************************************************************************
 * image_plain_offset
 ****************************************************************************/
static int
image_plain_offset(
	union image			*img)

	{
	return (img->pln.offset);
	}



/****************************************************************************
 * image_plain_read
 ****************************************************************************/
static int
image_plain_read(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_limit(ffo);

	debug_error_condition(! file_is_readable(&img->pln.fil));
	fifo_set_wr_ofs(ffo, size);
	if (img->pln.flags & FLAG_END_SEEN) goto done;
	verbose_message(GENERIC, 1, "reading plain track %d with %d bytes from '%s'", track, size, file_get_path(&img->pln.fil));
	size = file_read(&img->pln.fil, fifo_get_data(ffo), size);
	img->pln.offset += size;
	if (size == fifo_get_limit(ffo)) goto done;
	if (! (img->pln.flags & FLAG_IGNORE_SIZE)) error_warning("file '%s' is shorter than needed", file_get_path(&img->pln.fil));
	img->pln.flags |= FLAG_END_SEEN;
done:
	return (1);
	}



/****************************************************************************
 * image_plain_write
 ****************************************************************************/
static int
image_plain_write(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);

	debug_error_condition(! file_is_writable(&img->pln.fil));
	verbose_message(GENERIC, 1, "writing plain track %d with %d bytes to '%s'", track, size, file_get_path(&img->pln.fil));
	file_write(&img->pln.fil, fifo_get_data(ffo), size);
	fifo_set_rd_ofs(ffo, size);
	img->pln.offset += size;
	return (1);
	}



/****************************************************************************
 * image_plain_done
 ****************************************************************************/
static int
image_plain_done(
	union image			*img,
	struct image_track		*img_trk,
	int				track)

	{
	debug_error();
	return (1);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_plain_desc
 ****************************************************************************/
struct image_desc			image_plain_desc =
	{
	.name        = "plain",
	.level       = 3,
	.flags       = IMAGE_FLAG_CONTINUOUS_TRACK,
	.open        = image_plain_open,
	.close       = image_plain_close,
	.offset      = image_plain_offset,
	.track_read  = image_plain_read,
	.track_write = image_plain_write,
	.track_done  = image_plain_done
	};
/******************************************************** Karsten Scheibler */

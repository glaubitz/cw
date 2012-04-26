/****************************************************************************
 ****************************************************************************
 *
 * image/d64.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "d64.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../disk.h"
#include "../fifo.h"
#include "../file.h"
#include "../image.h"



#define FLAG_NOERROR			(1 << 0)
#define FLAG_IGNORE_SIZE		(1 << 1)
#define FLAG_END_SEEN			(1 << 2)



/****************************************************************************
 * image_d64_open
 ****************************************************************************/
static int
image_d64_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	image_open(img, &img->d64.fil, path, mode);
	if (flags & IMAGE_FLAG_IGNORE_SIZE) img->d64.flags = FLAG_IGNORE_SIZE;
	return (1);
	}



/****************************************************************************
 * image_d64_noerror_open
 ****************************************************************************/
static int
image_d64_noerror_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	image_open(img, &img->d64.fil, path, mode);
	img->d64.flags = FLAG_NOERROR;
	if (flags & IMAGE_FLAG_IGNORE_SIZE) img->d64.flags |= FLAG_IGNORE_SIZE;
	return (1);
	}



/****************************************************************************
 * image_d64_close
 ****************************************************************************/
static int
image_d64_close(
	union image			*img)

	{
	unsigned char			*buffer;
	int				size, e, s, t;

	if (file_is_readable(&img->d64.fil))
		{

		/* check if end of file is already reached */

		if (img->d64.flags & (FLAG_IGNORE_SIZE | FLAG_END_SEEN)) goto done;

		/*
		 * read remaining bytes, either there are no bytes or we
		 * get exactly one byte per sector for error information
		 */

		for (t = s = 0; t < GLOBAL_NR_TRACKS; t++) s += img->d64.sectors[t];
		buffer = (unsigned char *) alloca(s + 1);
		if (buffer == NULL) error_oom();
		size = file_read(&img->d64.fil, buffer, s + 1);
		if (size == s) verbose_message(GENERIC, 1, "ignoring %d bytes of error information from '%s'", size, file_get_path(&img->d64.fil));
		if ((size == 0) || (size == s)) goto done;
		error_warning("file '%s' is larger than needed", file_get_path(&img->d64.fil));
		}

	/* done if no error information should be written */

	if (img->d64.flags & FLAG_NOERROR) goto done;

	/* count errors in image */

	for (e = t = s = 0; t < GLOBAL_NR_TRACKS; t++)
		{
		size = img->d64.sectors[t];
		if (size == 0) continue;
		for (s = 0; s < size; s++) e += img->d64.errors[t][s];
		}

	/* append error information, if errors were found */

	if (e > 0) for (t = 0; t < GLOBAL_NR_TRACKS; t++)
		{
		size = img->d64.sectors[t];
		if (size == 0) continue;
		verbose_message(GENERIC, 1, "appending error information for track %d with %d bytes to '%s'", t, size, file_get_path(&img->d64.fil));
		file_write(&img->d64.fil, img->d64.errors[t], size);
		}
done:
	return (image_close(img, &img->d64.fil));
	}



/****************************************************************************
 * image_d64_offset
 ****************************************************************************/
static int
image_d64_offset(
	union image			*img)

	{
	return (img->d64.offset);
	}



/****************************************************************************
 * image_d64_read
 ****************************************************************************/
static int
image_d64_read(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_limit(ffo);

	debug_error_condition(! file_is_readable(&img->d64.fil));
	fifo_set_wr_ofs(ffo, size);
	if (img->d64.flags & FLAG_END_SEEN) goto done;
	verbose_message(GENERIC, 1, "reading D64 track %d with %d bytes from '%s'", track, size, file_get_path(&img->d64.fil));
	size = file_read(&img->d64.fil, fifo_get_data(ffo), size);
	img->d64.offset += size;
	img->d64.sectors[track] = sectors;
	if (size == fifo_get_limit(ffo)) goto done;
	if (! (img->d64.flags & FLAG_IGNORE_SIZE)) error_warning("file '%s' is shorter than needed", file_get_path(&img->d64.fil));
	img->d64.flags |= FLAG_END_SEEN;
done:
	return (1);
	}



/****************************************************************************
 * image_d64_write
 ****************************************************************************/
static int
image_d64_write(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);
	int				s;

	debug_error_condition(! file_is_writable(&img->d64.fil));
	debug_error_condition((track < 0) || (track >= GLOBAL_NR_TRACKS));
	debug_error_condition((sectors <= 0) || (sectors > GLOBAL_NR_SECTORS));
	verbose_message(GENERIC, 1, "writing D64 track %d with %d bytes to '%s'", track, size, file_get_path(&img->d64.fil));
	file_write(&img->d64.fil, fifo_get_data(ffo), size);
	fifo_set_rd_ofs(ffo, size);
	img->d64.offset += size;
	img->d64.sectors[track] = sectors;

	/*
	 * UGLY: directly accessing struct disk_sector is bad, better use
	 *       functions from disk.c ?
	 */

	for (s = 0; s < sectors; s++) img->d64.errors[track][s] = (dsk_sct[s].err.errors > 0) ? 5 : 0;
	return (1);
	}



/****************************************************************************
 * image_d64_done
 ****************************************************************************/
static int
image_d64_done(
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
 * image_d64_desc
 ****************************************************************************/
struct image_desc			image_d64_desc =
	{
	.name        = "d64",
	.level       = 3,
	.flags       = IMAGE_FLAG_CONTINUOUS_TRACK,
	.open        = image_d64_open,
	.close       = image_d64_close,
	.offset      = image_d64_offset,
	.track_read  = image_d64_read,
	.track_write = image_d64_write,
	.track_done  = image_d64_done
	};



/****************************************************************************
 * image_d64_noerror_desc
 ****************************************************************************/
struct image_desc			image_d64_noerror_desc =
	{
	.name        = "d64_noerror",
	.level       = 3,
	.flags       = IMAGE_FLAG_CONTINUOUS_TRACK,
	.open        = image_d64_noerror_open,
	.close       = image_d64_close,
	.offset      = image_d64_offset,
	.track_read  = image_d64_read,
	.track_write = image_d64_write,
	.track_done  = image_d64_done
	};
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * image/g64.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g64.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"
#include "../fifo.h"
#include "../file.h"
#include "../image.h"
#include "../import.h"
#include "../export.h"



#define FLAG_IGNORE_SIZE		(1 << 0)

#define MAGIC_SIZE			8
#define TRACK_SIZE			7928

struct g64_header
	{
	unsigned char			version;
	unsigned char			tracks;
	unsigned char			track_size[2];
	};




/****************************************************************************
 *
 * low level functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_g64_allocate_track
 ****************************************************************************/
static struct image_g64_track
image_g64_allocate_track(
	int				size,
	int				speed,
	int				used)

	{
	struct image_g64_track		img_g64_trk;

	img_g64_trk = (struct image_g64_track)
		{
		.data  = (unsigned char *) malloc(size),
		.size  = size,
		.speed = speed,
		.used  = used
		};
	if (img_g64_trk.data == NULL) error_oom();
	return (img_g64_trk);
	}



/****************************************************************************
 * image_g64_read_fill
 ****************************************************************************/
static int
image_g64_read_fill(
	struct image_g64		*img_g64,
	int				size)

	{
	unsigned char			buffer[0x10000];
	int				l, s = size;

	debug_error_condition(size < 0);
	while (s > 0)
		{
		l = size;
		if (l > sizeof (buffer)) l = sizeof (buffer);
		verbose_message(GENERIC, 2, "reading %d fill bytes", l);
		s -= file_read_strict(&img_g64->fil, buffer, l);
		}
	return (size);
	}



/****************************************************************************
 * image_g64_write_fill
 ****************************************************************************/
static int
image_g64_write_fill(
	struct image_g64		*img_g64,
	int				value,
	int				size)

	{
	unsigned char			buffer[0x10000];
	int				l, s = size;

	debug_error_condition(size < 0);
	memset(buffer, value, (size > sizeof (buffer)) ? sizeof (buffer) : size);
	while (s > 0)
		{
		l = size;
		if (l > sizeof (buffer)) l = sizeof (buffer);
		verbose_message(GENERIC, 2, "writing %d fill bytes", l);
		s -= file_write(&img_g64->fil, buffer, l);
		}
	return (size);
	}



/****************************************************************************
 * image_g64_read_data
 ****************************************************************************/
static void
image_g64_read_data(
	struct image_g64		*img_g64,
	int				tracks,
	int				max_track_size,
	int				offset)

	{
	unsigned char			buffer[4];
	unsigned int			track_offsets[IMAGE_G64_MAX_TRACK];
	unsigned int			speed_offsets[IMAGE_G64_MAX_TRACK];
	int				i, s, t;

	/* read track offsets */

	for (t = 0; t < tracks; t++, offset += 4)
		{
		verbose_message(GENERIC, 2, "reading G64 track offset for track %d from '%s'", t, file_get_path(&img_g64->fil));
		file_read_strict(&img_g64->fil, buffer, 4);
		track_offsets[t] = import_u32_le(buffer);
		verbose_message(GENERIC, 2, "got track offset %d", track_offsets[t]);
		}

	/* read speed offsets */

	for (t = 0; t < tracks; t++, offset += 4)
		{
		verbose_message(GENERIC, 2, "reading G64 speed offset for track %d from '%s'", t, file_get_path(&img_g64->fil));
		file_read_strict(&img_g64->fil, buffer, 4);
		speed_offsets[t] = import_u32_le(buffer);
		verbose_message(GENERIC, 2, "got speed offset %d", speed_offsets[t]);
		if (speed_offsets[t] > 3)  error_message("file '%s' uses unsupported speed zone map", file_get_path(&img_g64->fil));
		}

	/* read track data */

	while (1)
		{

		/* find smallest track offset */

		for (i = s = 0, t = -1; i < tracks; i++)
			{
			if (track_offsets[i] == 0) continue;
			if ((t != -1) && (s < track_offsets[i])) continue;
			s = track_offsets[i];
			t = i;
			}
		if (t == -1) break;
		verbose_message(GENERIC, 2, "reading G64 data for track %d from '%s'", t, file_get_path(&img_g64->fil));

		/* skip fill bytes in between tracks (if any) */

		offset += image_g64_read_fill(img_g64, track_offsets[t] - offset);

		/* read in data */

		file_read_strict(&img_g64->fil, buffer, 2);
		s = import_u16_le(buffer);
		if (s > max_track_size) error_message("track %d in file '%s' too large", t, file_get_path(&img_g64->fil));
		img_g64->trk[t] = image_g64_allocate_track(s, speed_offsets[t], 0);
		file_read_strict(&img_g64->fil, img_g64->trk[t].data, s);
		offset += s + 2;
		track_offsets[t] = 0;
		verbose_message(GENERIC, 2, "got %d bytes", s);

		/* skip fill bytes */

		offset += image_g64_read_fill(img_g64, max_track_size - s);
		}
	if (img_g64->flags & FLAG_IGNORE_SIZE) return;
	if (file_read(&img_g64->fil, buffer, 1) == 0) return;
	error_warning("file '%s' has trailing junk", file_get_path(&img_g64->fil));
	}



/****************************************************************************
 * image_g64_write_data
 ****************************************************************************/
static void
image_g64_write_data(
	struct image_g64		*img_g64,
	int				max_track_size,
	int				offset)

	{
	unsigned char			buffer[4];
	int				s, t;

	/* write track offsets */

	offset += 8 * IMAGE_G64_MAX_TRACK;
	for (t = 0; t < IMAGE_G64_MAX_TRACK; t++)
		{
		if (img_g64->trk[t].data == NULL) s = 0;
		else s = offset, offset += max_track_size + 2;
		export_u32_le(buffer, s);
		verbose_message(GENERIC, 2, "writing G64 track offset %d for track %d to '%s'", s, t, file_get_path(&img_g64->fil));
		file_write(&img_g64->fil, buffer, 4);
		}

	/* write speed offsets */

	for (t = 0; t < IMAGE_G64_MAX_TRACK; t++)
		{
		export_u32_le(buffer, img_g64->trk[t].speed);
		verbose_message(GENERIC, 2, "writing G64 speed offset %d for track %d to '%s'", img_g64->trk[t].speed, t, file_get_path(&img_g64->fil));
		file_write(&img_g64->fil, buffer, 4);
		}

	/* write track data */

	for (t = 0; t < IMAGE_G64_MAX_TRACK; t++)
		{
		if (img_g64->trk[t].data == NULL) continue;
		s = img_g64->trk[t].size;
		export_u16_le(buffer, s);
		verbose_message(GENERIC, 2, "writing G64 data for track %d with %d bytes to '%s'", t, s, file_get_path(&img_g64->fil));
		file_write(&img_g64->fil, buffer, 2);
		file_write(&img_g64->fil, img_g64->trk[t].data, s);

		/*
		 * write fill bytes (if needed). use 0xaa as fill value in
		 * case the last two bits were already zero
		 */

		image_g64_write_fill(img_g64, 0xaa, max_track_size - s);
		}
	}




/****************************************************************************
 *
 * interface functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_g64_open
 ****************************************************************************/
static int
image_g64_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	struct g64_header		g64_hdr;
	static const char		magic[MAGIC_SIZE] = { 'G', 'C', 'R', '-', '1', '5', '4', '1' };
	char				buffer[MAGIC_SIZE];

	image_open(img, &img->g64.fil, path, mode);
	if (flags & IMAGE_FLAG_IGNORE_SIZE) img->g64.flags = FLAG_IGNORE_SIZE;
	if (file_is_readable(&img->g64.fil))
		{
		int			track_size, i;

		file_read_strict(&img->g64.fil, buffer, sizeof (buffer));
		file_read_strict(&img->g64.fil, &g64_hdr, sizeof (g64_hdr));
		for (i = 0; i < sizeof (magic); i++) if (magic[i] != buffer[i]) error_message("file '%s' has wrong magic", file_get_path(&img->g64.fil));
		if (g64_hdr.version != 0) error_message("file '%s' has wrong version", file_get_path(&img->g64.fil));
		if (g64_hdr.tracks > IMAGE_G64_MAX_TRACK) error_message("file '%s' has too many tracks", file_get_path(&img->g64.fil));
		track_size = import_u16_le(g64_hdr.track_size);
		image_g64_read_data(&img->g64, g64_hdr.tracks, track_size, sizeof (magic) + sizeof (g64_hdr));
		}
	else file_write(&img->g64.fil, magic, sizeof (magic));
	return (1);
	}



/****************************************************************************
 * image_g64_close
 ****************************************************************************/
static int
image_g64_close(
	union image			*img)

	{
	struct g64_header		g64_hdr;
	int				s, t;

	if (file_is_writable(&img->g64.fil))
		{

		/*
		 * currently no track will be larger than TRACK_SIZE,
		 * because larger tracks are truncated in image_g64_write()
		 * so the warning will never occur
		 */

		for (s = TRACK_SIZE, t = 0; t < IMAGE_G64_MAX_TRACK; t++) if (img->g64.trk[t].size > s) s = img->g64.trk[t].size;
		if (s != TRACK_SIZE) error_warning("file '%s' contains large tracks, file may be unusable with other software", file_get_path(&img->g64.fil));
		g64_hdr = (struct g64_header) { .tracks = IMAGE_G64_MAX_TRACK };
		export_u16_le(g64_hdr.track_size, s);
		file_write(&img->g64.fil, &g64_hdr, sizeof (g64_hdr));
		image_g64_write_data(&img->g64, s, MAGIC_SIZE + sizeof (g64_hdr));
		}
	else
		{
		if (! (img->g64.flags & FLAG_IGNORE_SIZE)) for (t = 0; t < IMAGE_G64_MAX_TRACK; t++)
			{
			if ((img->g64.trk[t].data == NULL) || (img->g64.trk[t].used)) continue;
			error_warning("track %d from file '%s' was not used", t, file_get_path(&img->g64.fil));
			}
		}
	for (t = 0; t < IMAGE_G64_MAX_TRACK; t++) if (img->g64.trk[t].data != NULL) free(img->g64.trk[t].data);
	return (image_close(img, &img->g64.fil));
	}



/****************************************************************************
 * image_g64_offset
 ****************************************************************************/
static int
image_g64_offset(
	union image			*img)

	{
	return (0);
	}



/****************************************************************************
 * image_g64_read
 ****************************************************************************/
static int
image_g64_read(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size;

	debug_error_condition(! file_is_readable(&img->g64.fil));
	if (track >= IMAGE_G64_MAX_TRACK) error_message("track value %d out of range for G64 image format", track); 
	if (img->g64.trk[track].data != NULL)
		{
		img->g64.trk[track].used = 1;
		size = img->g64.trk[track].size;
		debug_error_condition(fifo_get_limit(ffo) < size);
		verbose_message(GENERIC, 1, "reading G64 track %d with %d bytes from memory", track, size);
		fifo_write_block(ffo, img->g64.trk[track].data, size);
		fifo_set_speed(ffo, img->g64.trk[track].speed);
		}
	else
		{
		if (! (img->g64.flags & FLAG_IGNORE_SIZE)) error_warning("track %d in file '%s' contains no data", track, file_get_path(&img->g64.fil));
		memset(fifo_get_data(ffo), 0x55, TRACK_SIZE);
		fifo_set_wr_ofs(ffo, TRACK_SIZE);
		}
	return (1);
	}



/****************************************************************************
 * image_g64_write
 ****************************************************************************/
static int
image_g64_write(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);

	debug_error_condition(! file_is_writable(&img->g64.fil));
	if (track >= IMAGE_G64_MAX_TRACK) error_message("track value %d out of range for G64 image format", track);

	/*
	 * changing this limit to a larger value than TRACK_SIZE may cause
	 * image_g64_close() to print out warnings
	 */

	if (size > TRACK_SIZE)
		{
		error_warning("too much data on track %d for G64 image format, will truncate it", track);
		size = TRACK_SIZE;
		}
	debug_error_condition(img->g64.trk[track].data != NULL);
	verbose_message(GENERIC, 1, "writing G64 track %d with %d bytes to memory", track, size);
	img->g64.trk[track] = image_g64_allocate_track(size, fifo_get_speed(ffo), 1);
	fifo_read_block(ffo, img->g64.trk[track].data, size);
	return (1);
	}



/****************************************************************************
 * image_g64_done
 ****************************************************************************/
static int
image_g64_done(
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
 * image_g64_desc
 ****************************************************************************/
struct image_desc			image_g64_desc =
	{
	.name        = "g64",
	.level       = 1,
	.flags       = IMAGE_FLAG_84_TRACKS,
	.open        = image_g64_open,
	.close       = image_g64_close,
	.offset      = image_g64_offset,
	.track_read  = image_g64_read,
	.track_write = image_g64_write,
	.track_done  = image_g64_done
	};
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * image/raw.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raw.h"
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
#include "../parse.h"
#include "../string.h"



#define TEXT_BUFFER_SIZE		0x20000

#define MAGIC_SIZE			32

#define TYPE_DEVICE			1
#define TYPE_PIPE			2
#define TYPE_REGULAR			3

#define SUBTYPE_NONE			0
#define SUBTYPE_DATA			1
#define SUBTYPE_TEXT			2

#define FLAG_SEARCH_HINTS		(1 << 0)

#define TRACK_MAGIC			0xca
#define TRACK_FLAG_DONE			(1 << 0)
#define TRACK_FLAG_FOUND		(1 << 1)

#define HEADER_FLAG_WRITABLE		(1 << 0)
#define HEADER_FLAG_INDEX_STORED	(1 << 1)
#define HEADER_FLAG_INDEX_ALIGNED	(1 << 2)
#define HEADER_FLAG_NO_CORRECTION	(1 << 3)

struct track_header
	{
	unsigned char			magic;
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	unsigned char			size[4];
	};




/****************************************************************************
 *
 * low level functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_track_translate
 ****************************************************************************/
static int
image_raw_track_translate(
	struct image_track		*img_trk,
	int				track)

	{
	int				xlated_track = track;
	int				t, s;

	debug_error_condition((track < 0) || (track >= GLOBAL_NR_TRACKS));
	if (img_trk->side_offset != 0)
		{
		t = track;
		s = (t >= img_trk->side_offset) ? 1 : 0;
		if (s > 0) t -= img_trk->side_offset;
		xlated_track = 2 * t + s;
		}
	if (img_trk->flags & IMAGE_TRACK_FLAG_FLIP_SIDE) xlated_track ^= 1;
	debug_error_condition((xlated_track < 0) || (xlated_track >= GLOBAL_NR_TRACKS));
	if (track != xlated_track) verbose_message(GENERIC, 1, "translating track from %d to %d", track, xlated_track);
	return (xlated_track);
	}



/****************************************************************************
 * image_raw_ioctl
 ****************************************************************************/
static int
image_raw_ioctl(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	int				timeout,
	int				track,
	int				cmd,
	int				mode,
	unsigned char			*data,
	int				size)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				result = -1;

	tri.clock   = img_trk->clock;
	tri.timeout = timeout;
	tri.track   = track / 2;
	tri.side    = track & 1;
	tri.mode    = mode;
	tri.data    = data;
	tri.size    = size;

	/*
	 * i had to choose between two ways to implement support for drives
	 * with 40 tracks (like those old 5.25" 360K drives):
	 * 1. i keep the track numbering scheme and the driver signals
	 *    inaccessible tracks
	 * 2. the driver stepping routines stay unchanged. the driver just
	 *    tells to userspace, that this drive has half the tracks a
	 *    normal drive would have, so one step effectively is a doulbe
	 *    step. userspace is then responsible to act accordingly
	 *
	 * i decided to take the second approach, not because it is better
	 * than the first, only because it seemed easier to implement
	 */

	if (img_raw->fli.flags & CW_FLOPPYINFO_FLAG_DOUBLE_STEP)
		{
		if ((tri.track & 1) != 0)
			{
			if (! (img_trk->flags & IMAGE_TRACK_FLAG_OPTIONAL)) error_warning("error while accessing track %d, only double steps are supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
			goto done;
			}
		tri.track /= 2;
		debug_message(GENERIC, 2, "drive only supports double steps, so track is halved");
		}

	/* for now do no special handling for a "preposition track" */

	tri.track_seek = tri.track;
	verbose_message(GENERIC, 1, "accessing hardware track %d side %d with timeout %d ms on '%s'", tri.track, tri.side, tri.timeout, file_get_path(&img_raw->fil[0]));
	if (tri.clock >= img_raw->fli.nr_clocks) error_message("error while accessing track %d, clock is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.track >= img_raw->fli.nr_tracks) error_message("error while accessing track %d, track is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.side  >= img_raw->fli.nr_sides)  error_message("error while accessing track %d, side is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.mode  >= img_raw->fli.nr_modes)  error_message("error while accessing track %d, mode is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	result = file_ioctl(&img_raw->fil[0], cmd, &tri, FILE_FLAG_NONE);
done:
	return (result);
	}



/****************************************************************************
 * image_raw_seekable
 ****************************************************************************/
static cw_bool_t
image_raw_seekable(
	struct image_raw		*img_raw)

	{
	if ((file_seek(&img_raw->fil[0], 1, FILE_FLAG_RETURN) == 1) &&
		(file_seek(&img_raw->fil[0], 0, FILE_FLAG_RETURN) == 0)) return (CW_BOOL_TRUE);
	file_open(&img_raw->fil[1], NULL, FILE_MODE_TMP, FILE_FLAG_NONE);
	return (CW_BOOL_FALSE);
	}



/****************************************************************************
 * image_raw_read_correction
 ****************************************************************************/
static cw_void_t
image_raw_read_correction(
	cw_raw_t			*data,
	cw_size_t			size,
	cw_count_t			track)

	{
	cw_index_t			i;

	verbose_message(GENERIC, 1, "doing '0x80 -> 0x7f' correction on raw track %d", track);
	for (i = 0; i < size; i++) if (data[i] > 0) data[i]--;
	}



/****************************************************************************
 * image_raw_clock_adjust_double
 ****************************************************************************/
static cw_void_t
image_raw_clock_adjust_double(
	cw_raw_t			*data,
	cw_size_t			size,
	cw_count_t			track)

	{
	cw_count_t			histogram[GLOBAL_NR_PULSE_LENGTHS] = { };
	cw_count_t			d1, d2;
	cw_index_t			i;

	/*
	 * double values without creating gaps in the histogram. this is
	 * done by using 2 * value and 2 * value + 1 alternately for each
	 * value
	 */

	verbose_message(GENERIC, 1, "doing clock adjustment (doubling values) on raw track %d", track);
	for (i = 0; i < size; i++)
		{
		d1 = data[i];
		d2 = 2 * (d1 & GLOBAL_PULSE_LENGTH_MASK);
		if (d2 > GLOBAL_MAX_PULSE_LENGTH) d2 = GLOBAL_MAX_PULSE_LENGTH;
		if (histogram[d2] > histogram[d2 + 1]) d2++;
		histogram[d2]++;
		if ((d1 & GLOBAL_PULSE_INDEX_MASK) != 0) d2 |= GLOBAL_PULSE_INDEX_MASK;
		data[i] = d2;
		}
	}



/****************************************************************************
 * image_raw_clock_adjust_half
 ****************************************************************************/
static cw_void_t
image_raw_clock_adjust_half(
	cw_raw_t			*data,
	cw_size_t			size,
	cw_count_t			track)

	{
	cw_count_t			d1, d2;
	cw_index_t			i;

	/*
	 * double values without creating gaps in the histogram. this is
	 * done by using 2 * value and 2 * value + 1 alternately for each
	 * value
	 */

	verbose_message(GENERIC, 1, "doing clock adjustment (halving values) on raw track %d", track);
	for (i = 0; i < size; i++)
		{
		d1 = data[i];
		d2 = (d1 & GLOBAL_PULSE_LENGTH_MASK) / 2;
		if ((d1 & GLOBAL_PULSE_INDEX_MASK) != 0) d2 |= GLOBAL_PULSE_INDEX_MASK;
		data[i] = d2;
		}
	}



/****************************************************************************
 * image_raw_read_track_data
 ****************************************************************************/
static cw_size_t
image_raw_read_track_data(
	struct image_raw		*img_raw,
	struct file			*fil,
	struct track_header		*trk_hdr,
	struct fifo			*ffo)

	{
	cw_size_t			size = sizeof (struct track_header);

	if (file_read(fil, trk_hdr, size) == 0) return (0);
	if (trk_hdr->magic != TRACK_MAGIC) error_message("wrong header magic in file '%s'", file_get_path(fil));
	size = import_u32_le(trk_hdr->size);
	if (size > fifo_get_limit(ffo)) error_message("track %d too large in file '%s'", trk_hdr->track, file_get_path(fil));
	return (file_read_strict(fil, fifo_get_data(ffo), size));
	}



/****************************************************************************
 * image_raw_read_track_text
 ****************************************************************************/
static cw_size_t
image_raw_read_track_text(
	struct image_raw		*img_raw,
	struct file			*fil,
	struct track_header		*trk_hdr,
	struct fifo			*ffo)

	{
	cw_char_t			token[GLOBAL_MAX_NAME_SIZE];
	cw_raw8_t			*data = fifo_get_data(ffo);
	cw_count_t			limit = fifo_get_limit(ffo);
	cw_bool_t			hex_only;
	cw_size_t			size;

	/* only img_raw->fil[0] could be in text format */

	error_condition(&img_raw->fil[0] != fil);

	/*
	 * read one track, if it does not contain any data look for next
	 * track (this allows empty tracks for raw text format)
	 */

	do
		{
		if (parse_token(&img_raw->prs, token, GLOBAL_MAX_NAME_SIZE) == 0) return (0);
		if (string_equal(token, "track_data_hex")) hex_only = CW_BOOL_TRUE;
		else hex_only = CW_BOOL_FALSE;
		if (! string_equal2(token, "track_data", "track_data_hex")) parse_error_invalid(&img_raw->prs, token);
		*trk_hdr = (struct track_header)
			{
			.magic = TRACK_MAGIC,
			.track = parse_number(&img_raw->prs, NULL, 0),
			.clock = parse_number(&img_raw->prs, NULL, 0),
			.flags = parse_number(&img_raw->prs, NULL, 0)
			};
		parse_token(&img_raw->prs, token, GLOBAL_MAX_NAME_SIZE);
		if (! string_equal(token, "{")) parse_error(&img_raw->prs, "{ expected");
		size = 0;
		parse_hex_only(&img_raw->prs, hex_only);
		while (1)
			{
			if (parse_token(&img_raw->prs, token, GLOBAL_MAX_NAME_SIZE) == 0) parse_error(&img_raw->prs, "} expected");
			if (string_equal(token, "}")) break;
			if (size > limit) parse_error(&img_raw->prs, "track %d too large");
			data[size++] = parse_number(&img_raw->prs, token, GLOBAL_MAX_NAME_SIZE);
			}
		parse_hex_only(&img_raw->prs, CW_BOOL_FALSE);
		}
	while (size == 0);
	export_u32_le(trk_hdr->size, size);

	/*
	 * if this is a regular file, it is necessary to seek back to the
	 * position the last character was read, otherwise the beginning
	 * of the next track would be missed. if this is a pipe no seeking
	 * is done, just iterate over data.
	 *
	 * only img_raw->fil[0] could be in text format. if the temporary
	 * file img_raw->fil[1] is used, data is always in binary format
	 */

	if (img_raw->type == TYPE_REGULAR) parse_flush_text_buffer(&img_raw->prs);

	return (size);
	}



/****************************************************************************
 * image_raw_read_track2
 ****************************************************************************/
static cw_size_t
image_raw_read_track2(
	struct image_raw		*img_raw,
	struct file			*fil,
	struct image_track		*img_trk,
	struct track_header		*trk_hdr,
	struct fifo			*ffo,
	cw_type_t			subtype)

	{
	cw_size_t			size;
	cw_bool_t			do_correction = CW_BOOL_TRUE;

	/* get data */

	fifo_reset(ffo);
	if (subtype == SUBTYPE_DATA) size = image_raw_read_track_data(img_raw, fil, trk_hdr, ffo);
	else size = image_raw_read_track_text(img_raw, fil, trk_hdr, ffo);
	if (size == 0) return (0);

	if (trk_hdr->track >= GLOBAL_NR_TRACKS) error_message("invalid track in file '%s'", file_get_path(fil));
	if (trk_hdr->clock >= CW_NR_CLOCKS) error_message("invalid clock in file '%s'", file_get_path(fil));
	if (trk_hdr->flags & HEADER_FLAG_WRITABLE)      fifo_set_flags(ffo, FIFO_FLAG_WRITABLE);
	if (trk_hdr->flags & HEADER_FLAG_INDEX_STORED)  fifo_set_flags(ffo, FIFO_FLAG_INDEX_STORED);
	if (trk_hdr->flags & HEADER_FLAG_INDEX_ALIGNED) fifo_set_flags(ffo, FIFO_FLAG_INDEX_ALIGNED);
	if (trk_hdr->flags & HEADER_FLAG_NO_CORRECTION) do_correction = CW_BOOL_FALSE;

	/*
	 * strictly check flags. tracks generated for normal write cannot
	 * be written index aligned, because they contain prolog data
	 */

	if (img_trk != NULL)
		{
		int			flag1 = (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) ? 1 : 0;
		int			flag2 = (trk_hdr->flags & HEADER_FLAG_INDEX_ALIGNED) ? 1 : 0;

		if (flag1 != flag2) fifo_clear_flags(ffo, FIFO_FLAG_WRITABLE);
		}

	/*
	 * corrections are only needed for writable images created with older
	 * versions of cwtool
	 */

	if ((trk_hdr->flags & HEADER_FLAG_WRITABLE) && (do_correction)) image_raw_read_correction(fifo_get_data(ffo), size, trk_hdr->track);

	/*
	 * if we have 14(28) MHz data but need 28(56) MHz just double values
	 * if such adjusting is wanted (or half values if the other direction
	 * occurs)
	 *
	 * UGLY: img_trk is only NULL if called from image_raw_close(), so
	 *       it ok for the other callers, but this implicit assumption
	 *       is still ugly
	 */

	if ((options_get_clock_adjust()) && (img_trk != NULL))
		{
		if ((trk_hdr->clock == 0) && (img_trk->clock == 1)) image_raw_clock_adjust_double(fifo_get_data(ffo), size, trk_hdr->track);
		if ((trk_hdr->clock == 1) && (img_trk->clock == 2)) image_raw_clock_adjust_double(fifo_get_data(ffo), size, trk_hdr->track);
		if ((trk_hdr->clock == 1) && (img_trk->clock == 0)) image_raw_clock_adjust_half(fifo_get_data(ffo), size, trk_hdr->track);
		if ((trk_hdr->clock == 2) && (img_trk->clock == 1)) image_raw_clock_adjust_half(fifo_get_data(ffo), size, trk_hdr->track);
		}
	return (size);
	}



/****************************************************************************
 * image_raw_found
 ****************************************************************************/
static int
image_raw_found(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct track_header		*trk_hdr,
	int				track)

	{
	int				flag1 = (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) ? 1 : 0;
	int				flag2 = (trk_hdr->flags & HEADER_FLAG_INDEX_ALIGNED) ? 1 : 0;
	cw_bool_t			clock_ok = CW_BOOL_FALSE;

	/*
	 * if we have 14(28) MHz data but need 28(56) MHz just double values
	 * if such adjusting is wanted (or half values if the other direction
	 * occurs)
	 */

	if (trk_hdr->clock == img_trk->clock) clock_ok = CW_BOOL_TRUE;
	if (options_get_clock_adjust())
		{
		if ((trk_hdr->clock == 0) && (img_trk->clock == 1)) clock_ok = CW_BOOL_TRUE;
		if ((trk_hdr->clock == 1) && (img_trk->clock == 2)) clock_ok = CW_BOOL_TRUE;
		if ((trk_hdr->clock == 1) && (img_trk->clock == 0)) clock_ok = CW_BOOL_TRUE;
		if ((trk_hdr->clock == 2) && (img_trk->clock == 1)) clock_ok = CW_BOOL_TRUE;
		}

	/*
	 * it is ok to read from index aligned track data, if this format
	 * does not want it. but if this raw track should be written back
	 * strict flag checking is needed as done in image_raw_read_track2()
	 */

	if ((trk_hdr->track != track) || (! clock_ok) || (flag1 > flag2)) return (0);
	img_raw->track_flags[track] |= TRACK_FLAG_FOUND;
	return (1);
	}



/****************************************************************************
 * image_raw_hint_store
 ****************************************************************************/
static void
image_raw_hint_store(
	struct image_raw		*img_raw,
	struct track_header		*trk_hdr,
	struct fifo			*ffo,
	int				size,
	int				offset)

	{
	int				file = 1;

	if (img_raw->track_flags[trk_hdr->track] & TRACK_FLAG_DONE) return;
	if (img_raw->hints >= IMAGE_RAW_NR_HINTS) error_message("file '%s' has too many tracks", file_get_path(&img_raw->fil[0]));
	if (img_raw->type == TYPE_PIPE)
		{
		verbose_message(GENERIC, 1, "appending track to '%s'", file_get_path(&img_raw->fil[1]));
		file   = 2;
		offset = file_seek(&img_raw->fil[1], -1, FILE_FLAG_NONE);
		file_write(&img_raw->fil[1], trk_hdr, sizeof (struct track_header));
		file_write(&img_raw->fil[1], fifo_get_data(ffo), size);
		}
	debug_message(GENERIC, 2, "appending hint, hints = %d file = %d, track = %d, offset = %d", img_raw->hints, file, trk_hdr->track, offset);
	img_raw->hnt[img_raw->hints++] = (struct image_raw_hint)
		{
		.file   = file,
		.track  = trk_hdr->track,
		.clock  = trk_hdr->clock,
		.flags  = trk_hdr->flags,
		.offset = offset
		};
	}



/****************************************************************************
 * image_raw_hint_search
 ****************************************************************************/
static int
image_raw_hint_search(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	int				track)

	{
	struct track_header		trk_hdr;
	int				file, h;
	cw_type_t			subtype;

	for (h = 0; h < img_raw->hints; h++)
		{

		/*
		 * UGLY: not very clean, better give needed parameters
		 *       directly to image_raw_found instead using
		 *       struct track_header
		 */

		trk_hdr = (struct track_header)
			{
			.track = img_raw->hnt[h].track,
			.clock = img_raw->hnt[h].clock,
			.flags = img_raw->hnt[h].flags
			};

		/* continue if track not found or hint already invalidated */

		if ((! image_raw_found(img_raw, img_trk, &trk_hdr, track)) ||
			(img_raw->hnt[h].file == 0)) continue;

		/*
		 * file == 0 original file, may be in data or text format
		 * file == 1 temporary file, data format only
		 */

		file = img_raw->hnt[h].file - 1;
		if (file == 0) subtype = img_raw->subtype;
		else subtype = SUBTYPE_DATA;
		img_raw->hnt[h].file = 0;
		debug_message(GENERIC, 2, "found hint, h = %d file = %d, track = %d, offset = %d", h, file, track, img_raw->hnt[h].offset);
		file_seek(&img_raw->fil[file], img_raw->hnt[h].offset, FILE_FLAG_NONE);
		verbose_message(GENERIC, 1, "reading raw track %d from '%s'", track, file_get_path(&img_raw->fil[file]));
		return (image_raw_read_track2(img_raw, &img_raw->fil[file], img_trk, &trk_hdr, ffo, subtype));
		}

	/*
	 * check if we are looking for a track, which is not optional and we
	 * haven't seen yet
	 */

	if ((! (img_trk->flags & IMAGE_TRACK_FLAG_OPTIONAL)) && (! (img_raw->track_flags[track] & TRACK_FLAG_FOUND)))
		error_warning("track %d not found in file '%s'", track, file_get_path(&img_raw->fil[0]));
	return (-1);
	}



/****************************************************************************
 * image_raw_hint_invalidate
 ****************************************************************************/
static void
image_raw_hint_invalidate(
	struct image_raw		*img_raw,
	int				track)

	{
	int				h;

	img_raw->track_flags[track] |= TRACK_FLAG_DONE;
	for (h = 0; h < img_raw->hints; h++)
		{
		if ((img_raw->hnt[h].file == 0) || (img_raw->hnt[h].track != track)) continue;
		debug_message(GENERIC, 2, "invalidating hint, h = %d", h);
		img_raw->hnt[h].file = 0;
		}
	}



/****************************************************************************
 * image_raw_read_track
 ****************************************************************************/
static int
image_raw_read_track(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	int				track)

	{
	struct track_header		trk_hdr;
	int				size, offset = 0;

	while (1)
		{

		/*
		 * search in previously stored tracks, if the image was
		 * completely read
		 */

		if (img_raw->flags & FLAG_SEARCH_HINTS) return (image_raw_hint_search(img_raw, img_trk, ffo, track));

		/* read next track */

		verbose_message(GENERIC, 1, "trying to read raw track %d from '%s'", track, file_get_path(&img_raw->fil[0]));
		if (img_raw->type == TYPE_REGULAR) offset = file_seek(&img_raw->fil[0], -1, FILE_FLAG_NONE);
		size = image_raw_read_track2(img_raw, &img_raw->fil[0], img_trk, &trk_hdr, ffo, img_raw->subtype);

		/* end of file reached, now search in stored tracks */

		if (size == 0)
			{
			img_raw->flags |= FLAG_SEARCH_HINTS;
			continue;
			}

		/* return if this is the track we are looking for */

		verbose_message(GENERIC, 1, "got raw track %d with %d bytes from '%s'", trk_hdr.track, size, file_get_path(&img_raw->fil[0]));
		if (image_raw_found(img_raw, img_trk, &trk_hdr, track)) return (size);

		/*
		 * we got a currently unwanted track, store it for later
		 * usage
		 */

		image_raw_hint_store(img_raw, &trk_hdr, ffo, size, offset);

		/*
		 * if we would search in previously stored tracks also here,
		 * we would change file offset, this would influence later
		 * reads, so we will only search if all tracks were stored
		 */

		}
	}



/****************************************************************************
 * image_raw_write_prepare
 ****************************************************************************/
static void
image_raw_write_prepare(
	struct fifo			*ffo,
	int				track)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				size  = fifo_get_wr_ofs(ffo);
	int				mask  = GLOBAL_PULSE_INDEX_MASK | GLOBAL_PULSE_LENGTH_MASK;
	int				d, i, j;

	if (! (fifo_get_flags(ffo) & FIFO_FLAG_WRITABLE)) error_warning("data written on track %d may be corrupt", track);
	if (fifo_get_flags(ffo) & FIFO_FLAG_INDEX_STORED) mask = GLOBAL_PULSE_LENGTH_MASK;
	for (i = j = 0; i < size; i++)
		{
		d = data[i] &= mask;
		if ((d >= GLOBAL_MIN_PULSE_LENGTH) && (d <= GLOBAL_MAX_PULSE_LENGTH)) continue;
		if (d < GLOBAL_MIN_PULSE_LENGTH) d = GLOBAL_MIN_PULSE_LENGTH;
		if (d > GLOBAL_MAX_PULSE_LENGTH) d = GLOBAL_MAX_PULSE_LENGTH;
		data[i] = d;
		j++;
		}
	if (j > 0) error_warning("had to modify catweasel counters %d times on track %d", j, track);
	}



/****************************************************************************
 * image_raw_write_flags
 ****************************************************************************/
static int
image_raw_write_flags(
	struct image_track		*img_trk,
	struct fifo			*ffo)

	{
	int				flags_in1 = fifo_get_flags(ffo);
	int				flags_in2 = img_trk->flags;
	int				flags_out = HEADER_FLAG_NO_CORRECTION;

	/*
	 * if input data was also raw, we get all needed flags with
	 * fifo_get_flags(), otherwise we also have to check if the data
	 * would be written index aligned on a real catweasel device
	 */

	if (flags_in1 & FIFO_FLAG_WRITABLE)             flags_out |= HEADER_FLAG_WRITABLE;
	if (flags_in1 & FIFO_FLAG_INDEX_STORED)         flags_out |= HEADER_FLAG_INDEX_STORED;
	if (flags_in1 & FIFO_FLAG_INDEX_ALIGNED)        flags_out |= HEADER_FLAG_INDEX_ALIGNED;
	if (flags_in2 & IMAGE_TRACK_FLAG_INDEXED_WRITE) flags_out |= HEADER_FLAG_INDEX_ALIGNED;
	return (flags_out);
	}




/****************************************************************************
 *
 * interface functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_open
 ****************************************************************************/
static int
image_raw_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	static const char		magic_data[MAGIC_SIZE]  = "cwtool raw data";
	static const char		magic_data2[MAGIC_SIZE] = "cwtool raw data 2";
	static const char		magic_data3[MAGIC_SIZE] = "cwtool raw data 3";
	static const char		magic_text3[MAGIC_SIZE] = "# cwtool raw text 3\n";
	char				buffer[MAGIC_SIZE], *type_name, *subtype_name;
	int				i;

	image_open(img, &img->raw.fil[0], path, mode);

	/* check if we have a catweasel device */

	type_name        = "device";
	subtype_name     = "";
	img->raw.type    = TYPE_DEVICE;
	img->raw.subtype = SUBTYPE_NONE;
	img->raw.fli     = CW_FLOPPYINFO_INIT;
	if (file_ioctl(&img->raw.fil[0], CW_IOC_GFLPARM, &img->raw.fli, FILE_FLAG_RETURN) == 0) goto done;

	/*
	 * check if we have a pipe or a regular file, write magic bytes if
	 * file was opened for writing, or check magic bytes if it was opened
	 * for reading (if it is a text file allocate memory for text buffer)
	 */

	type_name        = "pipe or a file";
	subtype_name     = " (data)";
	img->raw.type    = TYPE_PIPE;
	img->raw.subtype = SUBTYPE_DATA;
	if (file_is_readable(&img->raw.fil[0]))
		{
		if (! image_raw_seekable(&img->raw)) type_name = "pipe";
		else type_name = "file", img->raw.type = TYPE_REGULAR;
		file_read_strict(&img->raw.fil[0], buffer, MAGIC_SIZE);
		for (i = 0; i < MAGIC_SIZE; i++)
			{
			if (buffer[i] == magic_data[i]) continue;
			if (buffer[i] == magic_data2[i]) continue;
			if (buffer[i] == magic_data3[i]) continue;
			if (buffer[i] == magic_text3[i]) continue;
			if (magic_text3[i] == '\0')
				{
				img->raw.subtype = SUBTYPE_TEXT;
				break;
				}
			error_message("file '%s' has wrong magic", file_get_path(&img->raw.fil[0]));
			}
		if (img->raw.subtype == SUBTYPE_TEXT)
			{
			subtype_name = " (text)";
			parse_init_file(
				&img->raw.prs,
				parse_valid_chars_raw_text(),
				&img->raw.fil[0]);
			parse_max_number(&img->raw.prs, 255);

			/*
			 * seek back to file start if it is a regular file.
			 * needed for proper offset calculation in
			 * image_raw_read_track_text()
			 */

			if (img->raw.type == TYPE_REGULAR) file_seek(&img->raw.fil[0], 0, FILE_FLAG_NONE);
			else parse_fill_text_buffer(&img->raw.prs, buffer, MAGIC_SIZE);
			}
		}
	else file_write(&img->raw.fil[0], magic_data3, MAGIC_SIZE);
done:
	verbose_message(GENERIC, 1, "assuming '%s' is a %s%s", file_get_path(&img->raw.fil[0]), type_name, subtype_name);
	return (1);
	}



/****************************************************************************
 * image_raw_close
 ****************************************************************************/
static int
image_raw_close(
	union image			*img)

	{
	struct track_header		trk_hdr;
	unsigned char			data[GLOBAL_MAX_TRACK_SIZE];
	struct fifo			ffo = FIFO_INIT(data, sizeof (data));

	/* read remaining data if we have a pipe to prevent "broken pipe" */

	if ((file_is_readable(&img->raw.fil[0])) && (img->raw.type == TYPE_PIPE))
		{
		while (image_raw_read_track2(&img->raw, &img->raw.fil[0], NULL, &trk_hdr, &ffo, img->raw.subtype) > 0) ;
		file_close(&img->raw.fil[1]);
		}
	return (image_close(img, &img->raw.fil[0]));
	}



/****************************************************************************
 * image_raw_offset
 ****************************************************************************/
static int
image_raw_offset(
	union image			*img)

	{

	/*
	 * image "raw" may be also used as destination while reading, so we
	 * have to allow calls to this function
	 */

	return (0);
	}



/*
 * when reading or writing a raw image to a file, things like side_offset or
 * flip_side have no effect. because they are applied twice, once to read the
 * track and once to write it. on read we translate the given track to the
 * "real" track number (depending on side_offset and flip_side, and if we
 * access a catweasel device also double_step), on write we do the same thing
 * again and write to the "real" track. So the only thing that is influenced
 * by side_offset and flip_side in this case is the order in which tracks are
 * written to the raw image
 */



/****************************************************************************
 * image_raw_read
 ****************************************************************************/
static int
image_raw_read(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_limit(ffo);
	int				mode = CW_TRACKINFO_MODE_INDEX_STORE;
	int				flag = FIFO_FLAG_INDEX_STORED;

	track = image_raw_track_translate(img_trk, track);
	debug_error_condition(! file_is_readable(&img->raw.fil[0]));
	debug_error_condition((img->raw.type != TYPE_DEVICE) && (img->raw.type != TYPE_PIPE) && (img->raw.type != TYPE_REGULAR));
	if (img->raw.type == TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) mode = CW_TRACKINFO_MODE_INDEX_WAIT, flag = FIFO_FLAG_INDEX_ALIGNED;
		fifo_set_flags(ffo, flag);
		size = image_raw_ioctl(&img->raw, img_trk, img_trk->timeout_read, track,
			CW_IOC_READ, mode, fifo_get_data(ffo), size);
		}
	else size = image_raw_read_track(&img->raw, img_trk, ffo, track);
	if (size == -1) return (0);
	if (size < GLOBAL_MIN_TRACK_SIZE) error_warning("got only %d bytes while reading track %d", size, track);
	if (size > options_get_track_size_limit())
		{
		size = options_get_track_size_limit();
		verbose_message(GENERIC, 1, "truncating track according to track_size_limit to %d bytes", size);
		}
	fifo_set_wr_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_write
 ****************************************************************************/
static int
image_raw_write(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);
	int				mode = CW_TRACKINFO_MODE_NORMAL;

	track = image_raw_track_translate(img_trk, track);
	debug_error_condition(! file_is_writable(&img->raw.fil[0]));
	debug_error_condition((img->raw.type != TYPE_DEVICE) && (img->raw.type != TYPE_PIPE) && (img->raw.type != TYPE_REGULAR));
	if (size < GLOBAL_MIN_TRACK_SIZE)
		{
		error_warning("got only %d bytes for writing track %d, will skip it", size, track);
		goto done;
		}
	if (img->raw.type == TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_WRITE) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		image_raw_write_prepare(ffo, track);
		size = image_raw_ioctl(&img->raw, img_trk, img_trk->timeout_write, track,
			CW_IOC_WRITE, mode, fifo_get_data(ffo), size);
		}
	else 
		{
		struct track_header	trk_hdr =
			{
			.magic = TRACK_MAGIC,
			.track = track,
			.clock = img_trk->clock,
			.flags = image_raw_write_flags(img_trk, ffo)
			};

		export_u32_le(trk_hdr.size, size);
		verbose_message(GENERIC, 1, "writing raw track %d with %d bytes to '%s'", track, size, file_get_path(&img->raw.fil[0]));
		file_write(&img->raw.fil[0], &trk_hdr, sizeof (trk_hdr));
		file_write(&img->raw.fil[0], fifo_get_data(ffo), size);
		}
	if (size == -1) return (0);
	if (size < fifo_get_wr_ofs(ffo)) error_warning("could not write full track %d, write timed out", track);
done:
	fifo_set_rd_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_done
 ****************************************************************************/
static int
image_raw_done(
	union image			*img,
	struct image_track		*img_trk,
	int				track)

	{
	track = image_raw_track_translate(img_trk, track);
	if (img->raw.type != TYPE_DEVICE) image_raw_hint_invalidate(&img->raw, track);
	return (1);
	}




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_desc
 ****************************************************************************/
struct image_desc			image_raw_desc =
	{
	.name        = "raw",
	.level       = 0,
	.flags       = IMAGE_FLAG_SAME_TRACK,
	.open        = image_raw_open,
	.close       = image_raw_close,
	.offset      = image_raw_offset,
	.track_read  = image_raw_read,
	.track_write = image_raw_write,
	.track_done  = image_raw_done
	};
/******************************************************** Karsten Scheibler */

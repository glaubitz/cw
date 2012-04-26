/****************************************************************************
 ****************************************************************************
 *
 * image/raw.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_RAW_H
#define CWTOOL_IMAGE_RAW_H

#include "types.h"
#include "../global.h"
#include "../file.h"
#include "../parse.h"
#include "desc.h"

/* number of retries + first read == GLOBAL_NR_RETRIES + 1 */

#define IMAGE_RAW_NR_HINTS		((GLOBAL_NR_RETRIES + 1) * GLOBAL_NR_TRACKS)

struct image_raw_hint
	{
	unsigned char			file;	/* index for struct file in struct image_raw */
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	int				offset;
	};

struct image_raw_text
	{
	cw_char_t			*text;
	cw_count_t			line;
	cw_count_t			line_ofs;
	cw_count_t			ofs;
	cw_count_t			limit;
	cw_size_t			size;
	};

struct image_raw
	{
	struct file			fil[2];
	struct cw_floppyinfo		fli;
	struct image_raw_hint		hnt[IMAGE_RAW_NR_HINTS];
	int				hints;
	int				type;
	int				subtype;
	int				flags;
	int				track_flags[GLOBAL_NR_TRACKS];
	struct image_raw_text		txt;
	struct parse			prs;
	};

extern struct image_desc		image_raw_desc;



#endif /* !CWTOOL_IMAGE_RAW_H */
/******************************************************** Karsten Scheibler */

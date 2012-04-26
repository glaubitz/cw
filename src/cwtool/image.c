/****************************************************************************
 ****************************************************************************
 *
 * image.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "image.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "file.h"
#include "string.h"



static struct image_desc		*img_dsc[] =
	{
	&image_raw_desc,
	&image_g64_desc,
	&image_d64_desc,
	&image_d64_noerror_desc,
	&image_plain_desc,
	NULL
	};



/****************************************************************************
 * image_search_desc
 ****************************************************************************/
struct image_desc *
image_search_desc(
	const char			*name)

	{
	int				i;

	for (i = 0; img_dsc[i] != NULL; i++) if (string_equal(name, img_dsc[i]->name)) break;
	return (img_dsc[i]);
	}



/****************************************************************************
 * image_open
 ****************************************************************************/
int
image_open(
	union image			*img,
	struct file			*fil,
	char				*path,
	int				mode)

	{
	debug_error_condition((mode != IMAGE_MODE_READ) && (mode != IMAGE_MODE_WRITE));
	mode = (mode == IMAGE_MODE_READ) ? FILE_MODE_READ : FILE_MODE_CREATE;

	/*
	 * clearing img also means clearing fil, because fil is part of img
	 * (as part of the structs contained in union image). so order is
	 * important
	 */

	*img = (union image) { };
	file_open(fil, path, mode, FILE_FLAG_NONE);
	return (1);
	}



/****************************************************************************
 * image_close
 ****************************************************************************/
int
image_close(
	union image			*img,
	struct file			*fil)

	{

	/*
	 * clearing img also means clearing fil, because fil is part of img
	 * (as part of the structs contained in union image). so order is
	 * important
	 */

	file_close(fil);
	*img = (union image) { };
	return (1);
	}
/******************************************************** Karsten Scheibler */

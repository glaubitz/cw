/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_apple_test.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_GCR_APPLE_TEST_H
#define CWTOOL_FORMAT_GCR_APPLE_TEST_H

#include "types.h"
#include "bounds.h"
#include "desc.h"

struct gcr_apple_test
	{
	struct
		{
		unsigned char		flags;
		unsigned char		dump_sector;
		unsigned char		reserved[2];
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned short		fill_value1;
		unsigned short		fill_value2;
		unsigned char		fill_length1;
		unsigned char		fill_length2;
		short			precomp[9];
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		volume_id;
		unsigned char		mode;
		unsigned char		track_step;
		unsigned int		sync_value1;
		unsigned int		sync_value2;
		struct bounds		bnd[3];
		}			rw;
	};

extern struct format_desc		gcr_apple_test_format_desc;



#endif /* !CWTOOL_FORMAT_GCR_APPLE_TEST_H */
/******************************************************** Karsten Scheibler */

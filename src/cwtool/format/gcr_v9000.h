/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_v9000.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_GCR_V9000_H
#define CWTOOL_FORMAT_GCR_V9000_H

#include "types.h"
#include "bounds.h"
#include "desc.h"

struct gcr_v9000
	{
	struct
		{
		unsigned short		sync_length1;
		unsigned short		sync_length2;
		short			postcomp_simple_adjust[2];
		unsigned char		flags;
		unsigned char		reserved[3];
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned char		prolog_value;
		unsigned char		epilog_value;
		unsigned short		sync_length1;
		unsigned short		sync_length2;
		unsigned char		fill_length;
		unsigned char		fill_value;
		short			precomp[9];
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		header_id;
		unsigned char		data_id;
		unsigned char		flags;
		unsigned char		side_offset;
		unsigned char		reserved[3];
		struct bounds		bnd[3];
		}			rw;
	};

extern struct format_desc		gcr_v9000_format_desc;



#endif /* !CWTOOL_FORMAT_GCR_V9000_H */
/******************************************************** Karsten Scheibler */

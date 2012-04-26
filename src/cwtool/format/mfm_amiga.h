/****************************************************************************
 ****************************************************************************
 *
 * format/mfm_amiga.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_MFM_AMIGA_H
#define CWTOOL_FORMAT_MFM_AMIGA_H

#include "types.h"
#include "bounds.h"
#include "desc.h"

struct mfm_amiga
	{
	struct
		{
		unsigned char		flags;
		unsigned char		reserved[3];
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned char		prolog_value;
		unsigned char		epilog_value;
		unsigned char		fill_length;
		unsigned char		fill_value;
		short			precomp[9];
		unsigned short		reserved;
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		sync_length;
		unsigned short		sync_value;
		unsigned char		format_byte;
		unsigned char		reserved[3];
		struct bounds		bnd[3];
		}			rw;
	};

extern struct format_desc		mfm_amiga_format_desc;



#endif /* !CWTOOL_FORMAT_MFM_AMIGA_H */
/******************************************************** Karsten Scheibler */

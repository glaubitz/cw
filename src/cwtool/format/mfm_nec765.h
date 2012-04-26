/****************************************************************************
 ****************************************************************************
 *
 * format/mfm_nec765.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_MFM_NEC765_H
#define CWTOOL_FORMAT_MFM_NEC765_H

#include "types.h"
#include "../global.h"
#include "bounds.h"
#include "desc.h"

struct mfm_nec765
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
		unsigned char		fill_length1;
		unsigned char		fill_value1;
		unsigned char		fill_length2;
		unsigned char		fill_value2;
		unsigned char		fill_length3;
		unsigned char		fill_value3;
		unsigned char		fill_length4;
		unsigned char		fill_value4;
		unsigned char		fill_length5;
		unsigned char		fill_value5;
		unsigned char		fill_length6;
		unsigned char		fill_value6;
		unsigned char		fill_length7;
		unsigned char		fill_value7;
		short			precomp[9];
		unsigned short		reserved;
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		sync_length;
		unsigned short		sync_value;
		unsigned short		crc16_init_value;
		unsigned char		reserved;
		unsigned char		flags;
		unsigned char		id_address_mark;
		unsigned char		data_address_mark1;
		unsigned char		data_address_mark2;
		unsigned char		track_step;
		unsigned char		pshift[(GLOBAL_NR_SECTORS + 1) / 2];
		struct bounds		bnd[3];
		}			rw;
	};

extern struct format_desc		mfm_nec765_format_desc;



#endif /* !CWTOOL_FORMAT_MFM_NEC765_H */
/******************************************************** Karsten Scheibler */

/****************************************************************************
 ****************************************************************************
 *
 * format/fill.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_FILL_H
#define CWTOOL_FORMAT_FILL_H

#include "types.h"
#include "desc.h"

struct fill
	{
	struct
		{
		unsigned char		mode;
		unsigned char		fill_value;
		unsigned char		reserved[2];
		unsigned int		fill_length;
		}			wr;
	};

extern struct format_desc		fill_format_desc;



#endif /* !CWTOOL_FORMAT_FILL_H */
/******************************************************** Karsten Scheibler */

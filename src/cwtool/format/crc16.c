/****************************************************************************
 ****************************************************************************
 *
 * format/crc16.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "crc16.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../global.h"
#include "../options.h"



/****************************************************************************
 * format_crc16
 ****************************************************************************/
int
format_crc16(
	int				initval,
	const unsigned char		*data,
	int				size)

	{
	int				byte, table;
	static const int		lookup[16] =
		{
		0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
		0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF
		};

	/* x^16 + x^12 + x^5 + x^0 = 0x1021 (x^16 is left out) */

	while (size-- > 0)
		{
		byte    = *data++;
		table   = lookup[((byte >> 4) ^ (initval >> 12)) & 0x0f];
		initval = (initval << 4) ^ table;
		table   = lookup[(byte ^ (initval >> 12)) & 0x0f];
		initval = (initval << 4) ^ table;
		}
	return (initval & 0xffff);
	}
/******************************************************** Karsten Scheibler */

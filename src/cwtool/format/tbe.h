/****************************************************************************
 ****************************************************************************
 *
 * format/tbe.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_TBE_H
#define CWTOOL_FORMAT_TBE_H

#include "../import.h"
#include "../export.h"
#include "crc16.h"

#define tbe_read_u16_be(data)		import_u16_be(data)
#define tbe_write_u16_be(data, val)	export_u16_be(data, val)
#define tbe_crc16(init, data, size)	format_crc16(init, data, size)



#endif /* !CWTOOL_FORMAT_TBE_H */
/******************************************************** Karsten Scheibler */

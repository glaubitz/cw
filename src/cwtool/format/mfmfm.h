/****************************************************************************
 ****************************************************************************
 *
 * format/mfmfm.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_MFMFM_H
#define CWTOOL_FORMAT_MFMFM_H

#include "../import.h"
#include "../export.h"
#include "crc16.h"

struct fifo;
struct range;
struct disk_error;

extern const int			mfmfm_decode_table[];
extern const int			mfmfm_encode_table[];
extern int				mfmfm_read_sync(struct fifo *, struct range *, int, int);
extern int				mfmfm_read_sync2(struct fifo *, struct range *, int, int);
extern int				mfmfm_write_sync(struct fifo *, int, int);
extern int				mfmfm_write_fill(struct fifo *, int, int, int (*)(struct fifo *, int));
extern int				mfmfm_read_bytes(struct fifo *, struct disk_error *, unsigned char *, int, int (*)(struct fifo *, struct disk_error *, int));
extern int				mfmfm_write_bytes(struct fifo *, unsigned char *, int, int (*)(struct fifo *, int));
extern int				mfmfm_get_sector_shift(unsigned char *, int, int);
extern int				mfmfm_set_sector_size(unsigned char *, int, int, int);
extern int				mfmfm_fill_sector_shift(unsigned char *, int, int, int);

#define mfmfm_read_u16_be(data)		import_u16_be(data)
#define mfmfm_write_u16_be(data, val)	export_u16_be(data, val)
#define mfmfm_read_u32_le(data)		import_u32_le(data)
#define mfmfm_write_u32_le(data, val)	export_u32_le(data, val)
#define mfmfm_crc16(init, data, size)	format_crc16(init, data, size)



#endif /* !CWTOOL_FORMAT_MFMFM_H */
/******************************************************** Karsten Scheibler */

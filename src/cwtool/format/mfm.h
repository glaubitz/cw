/****************************************************************************
 ****************************************************************************
 *
 * format/mfm.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_MFM_H
#define CWTOOL_FORMAT_MFM_H

#include "mfmfm.h"

struct fifo;
struct disk_error;

extern int				mfm_read_8data_bits(struct fifo *, struct disk_error *, int);
extern int				mfm_write_8data_bits(struct fifo *, int);

#define mfm_decode_table					mfmfm_decode_table
#define mfm_encode_table					mfmfm_encode_table
#define mfm_read_u16_be(data)					mfmfm_read_u16_be(data)
#define mfm_write_u16_be(data, val)				mfmfm_write_u16_be(data, val)
#define mfm_read_u32_le(data)					mfmfm_read_u32_le(data)
#define mfm_write_u32_le(data, val)				mfmfm_write_u32_le(data, val)
#define mfm_read_sync(ffo, range, val, size)			mfmfm_read_sync(ffo, range, val, size)
#define mfm_write_sync(ffo, val, size)				mfmfm_write_sync(ffo, val, size)
#define mfm_write_fill(ffo, val, size)				mfmfm_write_fill(ffo, val, size, mfm_write_8data_bits)
#define mfm_read_bytes(ffo, err, data, size)			mfmfm_read_bytes(ffo, err, data, size, mfm_read_8data_bits)
#define mfm_write_bytes(ffo, data, size)			mfmfm_write_bytes(ffo, data, size, mfm_write_8data_bits)
#define mfm_crc16(init, data, size)				mfmfm_crc16(init, data, size)
#define mfm_get_sector_shift(pshift, sector, sectors)		mfmfm_get_sector_shift(pshift, sector, sectors)
#define mfm_set_sector_size(pshift, sector, sectors, size)	mfmfm_set_sector_size(pshift, sector, sectors, size)
#define mfm_fill_sector_shift(pshift, sector, sectors, shift)	mfmfm_fill_sector_shift(pshift, sector, sectors, shift)



#endif /* !CWTOOL_FORMAT_MFM_H */
/******************************************************** Karsten Scheibler */

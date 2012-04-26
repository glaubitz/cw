/****************************************************************************
 ****************************************************************************
 *
 * format/fm.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_FM_H
#define CWTOOL_FORMAT_FM_H

#include "mfmfm.h"

struct fifo;
struct disk_error;

extern int				fm_read_8data_bits(struct fifo *, struct disk_error *, int);
extern int				fm_write_8data_bits(struct fifo *, int);

#define fm_decode_table						mfmfm_decode_table
#define fm_encode_table						mfmfm_encode_table
#define fm_read_u16_be(data)					mfmfm_read_u16_be(data)
#define fm_write_u16_be(data, val)				mfmfm_write_u16_be(data, val)
#define fm_read_sync(ffo, range, val1, val2)			mfmfm_read_sync2(ffo, range, val1, val2)
#define fm_write_sync(ffo, val, size)				mfmfm_write_sync(ffo, val, size)
#define fm_write_fill(ffo, val, size)				mfmfm_write_fill(ffo, val, size, fm_write_8data_bits)
#define fm_read_bytes(ffo, err, data, size)			mfmfm_read_bytes(ffo, err, data, size, fm_read_8data_bits)
#define fm_write_bytes(ffo, data, size)				mfmfm_write_bytes(ffo, data, size, fm_write_8data_bits)
#define fm_crc16(init, data, size)				mfmfm_crc16(init, data, size)
#define fm_get_sector_shift(pshift, sector, sectors)		mfmfm_get_sector_shift(pshift, sector, sectors)
#define fm_set_sector_size(pshift, sector, sectors, size)	mfmfm_set_sector_size(pshift, sector, sectors, size)
#define fm_fill_sector_shift(pshift, sector, sectors, shift)	mfmfm_fill_sector_shift(pshift, sector, sectors, shift)



#endif /* !CWTOOL_FORMAT_FM_H */
/******************************************************** Karsten Scheibler */

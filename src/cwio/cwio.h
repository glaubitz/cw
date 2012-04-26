/****************************************************************************
 ****************************************************************************
 *
 * cwio.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWIO_H
#define CWIO_H




/****************************************************************************
 *
 * data structures and defines
 *
 ****************************************************************************/




#define CWIO_MAX_PATH_LEN		4096
#define CWIO_BUFFER_SIZE		0x20000

#define CWIO_DEVICE_TYPE_ANY		1
#define CWIO_DEVICE_TYPE_DEVICE		2
#define CWIO_DEVICE_TYPE_FILE		3

#define CWIO_DATA_CLOCK_14MHZ		1
#define CWIO_DATA_CLOCK_28MHZ		2
#define CWIO_DATA_CLOCK_56MHZ		3

#define CWIO_DATA_MODE_NORMAL		1	/* read or write from current head position */
#define CWIO_DATA_MODE_INDEX_WAIT	2	/* wait for index before reading or writing */
#define CWIO_DATA_MODE_INDEX_STORE	3	/* only while reading: store index signal in msb of data */

#define CWIO_MODE_READ			1
#define CWIO_MODE_WRITE			2

/*
 * those structs are defined in cwio.c, so there is no way to know the inner
 * members for callers who use cwio.h. use the functions
 * cwio_device_struct_size() and cwio_data_struct_size() to determine how
 * many bytes you need to allocate for the structs
 */

struct cwio_device;
struct cwio_data;




/****************************************************************************
 *
 * global functions
 *
 ****************************************************************************/




extern int
cwio_device_struct_size(
	void);

extern int
cwio_device_set_params(
	struct cwio_device		*cwio_dev,
	const char			*path,
	int				type);

extern int
cwio_device_has_double_step(
	struct cwio_device		*cwio_dev);

extern int
cwio_data_struct_size(
	void);

extern int
cwio_data_set_params(
	struct cwio_data		*cwio_data,
	int				track,
	int				side,
	int				clock,
	int				mode,
	int				timeout,
	void				*data,
	int				size);

extern int
cwio_open(
	struct cwio_device		*cwio_dev,
	int				mode);

extern int
cwio_close(
	struct cwio_device		*cwio_dev);

extern int
cwio_read(
	struct cwio_device		*cwio_dev,
	struct cwio_data		*cwio_data);

extern int
cwio_write(
	struct cwio_device		*cwio_dev,
	struct cwio_data		*cwio_data);



#endif /* !CWIO_H */
/******************************************************** Karsten Scheibler */

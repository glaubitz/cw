/****************************************************************************
 ****************************************************************************
 *
 * write.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>

#include "cwio.h"



/****************************************************************************
 * main
 ****************************************************************************/
int
main(
	void)

	{
	struct cwio_device		*cwio_dev;
	struct cwio_data		*cwio_data;
	unsigned char			buffer[CWIO_BUFFER_SIZE];
	int				track = 10;
	int				len;

	/* allocate memory for cwio_dev and cwio_data */

	cwio_dev  = alloca(cwio_device_struct_size());
	cwio_data = alloca(cwio_data_struct_size());
	if ((cwio_dev == NULL) || (cwio_data == NULL))
		{
		fprintf(stderr, "out of memory\n");
		exit(1);
		}

	/* set device parameters and open device for writing */

	cwio_device_set_params(
		cwio_dev,
		"/dev/cw0raw0",
		CWIO_DEVICE_TYPE_ANY);
	cwio_open(cwio_dev, CWIO_MODE_WRITE);

	/*
	 * determine if driver knows that drive has only 40 tracks (one step
	 * with a 40 track drive is equivalent to two steps with a 80 track
	 * drive). this can be set with cwtool -I
	 */

	if (cwio_device_has_double_step(cwio_dev)) track /= 2;
	fprintf(stderr, "writing track %d\n", track);

	/* get data from stdin */

	len = fread(buffer, 1, sizeof (buffer), stdin);

	/* set data parameters and write data to device */

	cwio_data_set_params(
		cwio_data,
		track,			/* track: 0 - 85 (0 - 42 with double step) */
		0,			/* side:  0 - 1 */
		CWIO_DATA_CLOCK_14MHZ,
		CWIO_DATA_MODE_NORMAL,
		400,			/* timeout for track operation in milliseconds */
		buffer,			/* buffer to store raw data */
		len);			/* size of buffer */
	cwio_write(cwio_dev, cwio_data);

	/* write more tracks here if needed ... */

	/* done */

	cwio_close(cwio_dev);
	return (0);
	}
/******************************************************** Karsten Scheibler */

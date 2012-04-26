/****************************************************************************
 ****************************************************************************
 *
 * cwtool.c
 *
 ****************************************************************************
 *
 * - struct disk is the center of all things, this struct contains the
 *   layout of all supported disks.
 * - struct disk has an entry for each track (each track may have a
 *   different format)
 * - L0: catweasel raw counter values
 *   L1: raw bits (FM, MFM, GCR, ..., converted from/to L0 with bounds)
 *   L2: decoded FM, MFM, GCR, ... (with checksums and sector headers)
 *   L3: sector data bytes only
 * - if a disk should be read, disk_read() is called, it opens the given
 *   source and destination file, source file has to be L0 (either
 *   catweasel device or file containing raw counter values), destination
 *   file may be L0 - L3 depending on the selected format
 * - disk_write() is more or less inverse, source file may be L0 - L3,
 *   depending on selected format, destination file has to be L0
 * - plain image formats as ADF or D64 are L3, G64 is L1
 * - config.c translates a textual config file to entries in struct disk
 *   beside the built in directives it also understands format specific
 *   directives passed via struct format_desc
 * - struct fifo is the container for the data and some side information,
 *   its functions support bit and byte operations suitable for decoding
 *   and encoding
 *
 ****************************************************************************
 ****************************************************************************/





#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cwtool.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "global.h"
#include "options.h"
#include "cmdline.h"
#include "config.h"
#include "disk.h"
#include "drive.h"
#include "file.h"
#include "string.h"



static int				exit_code = 0;



/****************************************************************************
 * cwtool_get_line
 ****************************************************************************/
static char *
cwtool_get_line(
	char				*line,
	int				size,
	const char			*name,
	const char			*info)

	{
	const static char		space[] = "                ";
	int				i = 0;

	while ((name[i] != '\0') && (space[i] != '\0')) i++;
	if ((string_length(info) == 0) || (verbose_get_level(VERBOSE_CLASS_CWTOOL_ILRW) == VERBOSE_LEVEL_NONE)) string_snprintf(line, size, "%s", name);
	else string_snprintf(line, size, "%s%s %s", name, &space[i], info);
	return (line);
	}



/****************************************************************************
 * cwtool_get_disk
 ****************************************************************************/
static struct disk *
cwtool_get_disk(
	void)

	{
	struct disk			*dsk = disk_search(cmdline_get_disk_name());

	if (dsk == NULL) error_message("unknown disk name '%s'", cmdline_get_disk_name());
	return (dsk);
	}



/****************************************************************************
 * cwtool_info_error_sector
 ****************************************************************************/
static int
cwtool_info_error_sector(
	char				*line,
	int				max,
	struct disk_sector_info		*dsk_sct_nfo,
	int				sector)

	{
	char				*reason = "mx";

	/*
	 * UGLY: access struct dsk_sct_nfo not here, better do this
	 *       do this in disk.c ?
	 */

	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_NOT_FOUND) reason = "nf";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_ENCODING)  reason = "en";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_ID)        reason = "id";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_NUMBERING) reason = "nu";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_SIZE)      reason = "si";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_CHECKSUM)  reason = "cs";
	return (string_snprintf(line, max, " %02d=%s@0x%06x", sector, reason, dsk_sct_nfo->offset));
	}



/****************************************************************************
 * cwtool_info_error_details
 ****************************************************************************/
static void
cwtool_info_error_details(
	struct disk_info		*dsk_nfo)

	{
	char				line[1024];
	int				i, l, s, t;

	verbose_message(CWTOOL_ILRW, 1, "details for bad sectors:");
	for (t = 0; t < GLOBAL_NR_TRACKS; t++)
		{
		for (i = s = 0; s < GLOBAL_NR_SECTORS; s++) if (dsk_nfo->sct_nfo[t][s].flags) i++;
		if (i == 0) continue;
		for (s = 0; s < GLOBAL_NR_SECTORS; )
			{
			l = string_snprintf(line, sizeof (line), "track %3d:", t);
			for (i = 0; (i < 4) && (s < GLOBAL_NR_SECTORS); s++)
				{
				if (! dsk_nfo->sct_nfo[t][s].flags) continue;
				l += cwtool_info_error_sector(&line[l], sizeof (line) - l, &dsk_nfo->sct_nfo[t][s], s);
				i++;
				}
			if (i > 0) verbose_message(CWTOOL_ILRW, 1, "%s", line);
			}
		}
	}



/****************************************************************************
 * cwtool_info_print
 ****************************************************************************/
static void
cwtool_info_print(
	struct disk_info		*dsk_nfo,
	int				summary)

	{
	char				line[1024], path[16];
	int				selector = 0;

	/* if there are bad sectors change exit code of cwtool to non zero */

	if (dsk_nfo->sum.sectors_bad > 0) exit_code = 1;

	/* return if verbosity is not high enough */

	if (verbose_get_level(VERBOSE_CLASS_CWTOOL_ILRW) == VERBOSE_LEVEL_NONE) return;

	/* construct line to be printed out */

	if (cmdline_get_mode() == CMDLINE_MODE_WRITE) selector += 4;
	if (summary)
		{
		selector += 2;
		if (dsk_nfo->sum.sectors_good + dsk_nfo->sum.sectors_weak + dsk_nfo->sum.sectors_bad > 0) selector++;
		}
	else if (dsk_nfo->sectors_good + dsk_nfo->sectors_weak + dsk_nfo->sectors_bad > 0) selector++;

	if (selector == 0) string_snprintf(line, sizeof (line), "reading track %3d try %2d (sectors: none) (%s)",
		dsk_nfo->track, dsk_nfo->try, string_dot(path, sizeof (path), dsk_nfo->path));
	if (selector == 1) string_snprintf(line, sizeof (line), "reading track %3d try %2d (sectors: good %2d weak %2d bad %2d) (%s)",
		dsk_nfo->track, dsk_nfo->try, dsk_nfo->sectors_good,
		dsk_nfo->sectors_weak, dsk_nfo->sectors_bad,
		string_dot(path, sizeof (path), dsk_nfo->path));
	if (selector == 2) string_snprintf(line, sizeof (line), "%3d tracks read",
		dsk_nfo->sum.tracks);
	if (selector == 3) string_snprintf(line, sizeof (line), "%3d tracks read (sectors: good %4d weak %4d bad %4d)",
		dsk_nfo->sum.tracks, dsk_nfo->sum.sectors_good,
		dsk_nfo->sum.sectors_weak, dsk_nfo->sum.sectors_bad);
	if (selector == 4) string_snprintf(line, sizeof (line), "writing track %3d (sectors: none)",
		dsk_nfo->track);
	if (selector == 5) string_snprintf(line, sizeof (line), "writing track %3d (sectors: %2d)",
		dsk_nfo->track, dsk_nfo->sectors_good);
	if (selector == 6) string_snprintf(line, sizeof (line), "%3d tracks written",
		dsk_nfo->sum.tracks);
	if (selector == 7) string_snprintf(line, sizeof (line), "%3d tracks written (sectors: %4d)",
		dsk_nfo->sum.tracks, dsk_nfo->sum.sectors_good);

	verbose_message(CWTOOL_ILRW, 1, "%s", line);
	if ((summary) && (dsk_nfo->sum.sectors_bad > 0)) cwtool_info_error_details(dsk_nfo);
	}



/****************************************************************************
 * cwtool_version
 ****************************************************************************/
static void
cwtool_version(
	void)

	{
	printf(GLOBAL_VERSION_STRING "\n");
	}



/****************************************************************************
 * cwtool_dump
 ****************************************************************************/
static void
cwtool_dump(
	void)

	{
	printf("# builtin default config (%s)\n%s", global_version_string(), config_default(1));
	}



/****************************************************************************
 * cwtool_initialize
 ****************************************************************************/
static void
cwtool_initialize(
	void)

	{
	cmdline_read_config();
	drive_init_all_devices();
	}



/****************************************************************************
 * cwtool_list
 ****************************************************************************/
static void
cwtool_list(
	void)

	{
	struct disk			*dsk;
	struct drive			*drv;
	char				line[3 * GLOBAL_MAX_PATH_SIZE];
	int				i;

	cmdline_read_config();
	if (disk_get(0) != NULL) printf("Available disk names are:\n");
	for (i = 0; (dsk = disk_get(i)) != NULL; i++) printf("%s\n", cwtool_get_line(line, sizeof (line), disk_get_name(dsk), disk_get_info(dsk)));
	if (drive_get(0) != NULL) printf("Configured device paths are:\n");
	for (i = 0; (drv = drive_get(i)) != NULL; i++) printf("%s\n", cwtool_get_line(line, sizeof (line), drive_get_path(drv), drive_get_info(drv)));
	}



/****************************************************************************
 * cwtool_statistics
 ****************************************************************************/
static void
cwtool_statistics(
	void)

	{
	struct disk			*dsk;

	cmdline_read_config();
	dsk = cwtool_get_disk();
	setlinebuf(stdout);
	disk_statistics(dsk, cmdline_get_file(0));
	}



/****************************************************************************
 * cwtool_read
 ****************************************************************************/
static void
cwtool_read(
	void)

	{
	struct disk			*dsk;
	struct disk_option		dsk_opt = DISK_OPTION_INIT(cwtool_info_print, cmdline_get_retry(), DISK_OPTION_FLAG_NONE);
	cw_count_t			files = cmdline_get_files();

	cmdline_read_config();
	if (options_get_always_initialize()) drive_init_all_devices();
	dsk = cwtool_get_disk();
	disk_read(dsk, &dsk_opt, cmdline_get_all_files(), files - 1, cmdline_get_file(files - 1), cmdline_get_output());
	}



/****************************************************************************
 * cwtool_write
 ****************************************************************************/
static void
cwtool_write(
	void)

	{
	struct disk			*dsk;
	cw_flag_t			flags = (cmdline_get_flag(CMDLINE_FLAG_IGNORE_SIZE)) ? DISK_OPTION_FLAG_IGNORE_SIZE : DISK_OPTION_FLAG_NONE;
	struct disk_option		dsk_opt = DISK_OPTION_INIT(cwtool_info_print, 0, flags);

	cmdline_read_config();
	if (options_get_always_initialize()) drive_init_all_devices();
	dsk = cwtool_get_disk();
	disk_write(dsk, &dsk_opt, cmdline_get_file(0), cmdline_get_file(1));
	}



/****************************************************************************
 * main
 ****************************************************************************/
int
main(
	int				argc,
	char				**argv)

	{
	cw_mode_t			mode;

	/* parse cmdline */

	setlinebuf(stderr);
	cmdline_parse(argv);
	mode = cmdline_get_mode();

	/* decide what to do */

	if      (mode == CMDLINE_MODE_VERSION)    cwtool_version();
	else if (mode == CMDLINE_MODE_DUMP)       cwtool_dump();
	else if (mode == CMDLINE_MODE_INITIALIZE) cwtool_initialize();
	else if (mode == CMDLINE_MODE_LIST)       cwtool_list();
	else if (mode == CMDLINE_MODE_STATISTICS) cwtool_statistics();
	else if (mode == CMDLINE_MODE_READ)       cwtool_read();
	else if (mode == CMDLINE_MODE_WRITE)      cwtool_write();
	else debug_error();

	/* done */

	return (exit_code);
	}
/******************************************************** Karsten Scheibler */

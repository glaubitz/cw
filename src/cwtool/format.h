/****************************************************************************
 ****************************************************************************
 *
 * format.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_H
#define CWTOOL_FORMAT_H

#include "types.h"
#include "format/raw.h"
#include "format/gcr_g64.h"
#include "format/gcr_cbm.h"
#include "format/gcr_apple.h"
#include "format/gcr_apple_test.h"
#include "format/gcr_v9000.h"
#include "format/fm_nec765.h"
#include "format/mfm_nec765.h"
#include "format/mfm_amiga.h"
#include "format/tbe_cw.h"
#include "format/fill.h"
#include "format/desc.h"

union format
	{
	struct gcr_g64			gcr_g64;
	struct gcr_cbm			gcr_cbm;
	struct gcr_apple		gcr_apl;
	struct gcr_apple_test		gcr_apl_tst;
	struct gcr_v9000		gcr_v9;
	struct fm_nec765		fm_nec;
	struct mfm_nec765		mfm_nec;
	struct mfm_amiga		mfm_amg;
	struct tbe_cw			tbe_cw;
	struct fill			fll;
	};

#define FORMAT_FLAG_NONE		0
#define FORMAT_FLAG_GREEDY		(1 << 0)
#define FORMAT_FLAG_OUTPUT		(1 << 1)

#define FORMAT_OPTION_TYPE_NONE			0
#define FORMAT_OPTION_TYPE_BOOLEAN		1
#define FORMAT_OPTION_TYPE_INTEGER		2
#define FORMAT_OPTION_FLAG_NONE			0
#define FORMAT_OPTION_FLAG_OBSOLETE		(1 << 0)
#define FORMAT_OPTION_BOOLEAN(n, m, p)		{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_BOOLEAN, .flags = FORMAT_OPTION_FLAG_NONE }
#define FORMAT_OPTION_INTEGER(n, m, p)		{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_INTEGER, .flags = FORMAT_OPTION_FLAG_NONE }
#define FORMAT_OPTION_BOOLEAN_COMPAT(n, m, p)	FORMAT_OPTION_BOOLEAN(n, m, p)
#define FORMAT_OPTION_INTEGER_COMPAT(n, m, p)	FORMAT_OPTION_INTEGER(n, m, p)
#define FORMAT_OPTION_BOOLEAN_OBSOLETE(n, m, p)	{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_BOOLEAN, .flags = FORMAT_OPTION_FLAG_OBSOLETE }
#define FORMAT_OPTION_INTEGER_OBSOLETE(n, m, p)	{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_INTEGER, .flags = FORMAT_OPTION_FLAG_OBSOLETE }
#define FORMAT_OPTION_END			{ .name = NULL, .magic = 0, .params = 0, .type = FORMAT_OPTION_TYPE_NONE, .flags = FORMAT_OPTION_FLAG_NONE }

struct format_option
	{
	char				*name;
	int				magic;
	int				params;
	int				type;
	cw_flag_t			flags;
	};

extern struct format_desc		*format_search_desc(const char *);
extern struct format_option		*format_search_option(struct format_option *, const char *);
extern cw_bool_t			format_option_is_obsolete(struct format_option *);
extern int				format_compare2(const char *, unsigned long, unsigned long);
extern int				format_compare3(const char *, unsigned long, unsigned long, unsigned long);



#endif /* !CWTOOL_FORMAT_H */
/******************************************************** Karsten Scheibler */

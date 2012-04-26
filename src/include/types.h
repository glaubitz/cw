/****************************************************************************
 ****************************************************************************
 *
 * types.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_TYPES_H
#define CW_TYPES_H

/*
 * stdlib.h is not really needed here, but some glibc-versions cause compile
 * time warnings like "conflicting types for 'dev_t'"
 */

#ifndef __KERNEL__
#include <stdlib.h>
#endif /* !__KERNEL__ */

#include <linux/types.h>

/* basic types */

#ifdef __KERNEL__
typedef s8				cw_s8_t;
typedef u8				cw_u8_t;
typedef s16				cw_s16_t;
typedef u16				cw_u16_t;
typedef s32				cw_s32_t;
typedef u32				cw_u32_t;
typedef s64				cw_s64_t;
typedef u64				cw_u64_t;
#else /* __KERNEL__ */
#ifndef CW_STANDALONE	/* currently not used */
typedef __s8				cw_s8_t;
typedef __u8				cw_u8_t;
typedef __s16				cw_s16_t;
typedef __u16				cw_u16_t;
typedef __s32				cw_s32_t;
typedef __u32				cw_u32_t;
typedef __s64				cw_s64_t;
typedef __u64				cw_u64_t;
#else /* !CW_STANDALONE */
typedef signed char			cw_s8_t;
typedef unsigned char			cw_u8_t;
typedef signed short			cw_s16_t;
typedef unsigned short			cw_u16_t;
typedef signed int			cw_s32_t;
typedef unsigned int			cw_u32_t;
typedef signed long long		cw_s64_t;
typedef unsigned long long		cw_u64_t;
#endif /* !CW_STANDALONE */
#endif /* __KERNEL__ */

typedef int				cw_int_t;
typedef unsigned long			cw_ptr_t;
typedef char				cw_char_t;
typedef void				cw_void_t;

/* pulse length reported by catweasel controller */

typedef cw_u8_t				cw_raw_t;
typedef cw_u8_t				cw_raw8_t;
typedef cw_u16_t			cw_raw16_t;

/* a small number */

typedef cw_u8_t				cw_snum_t;

/*
 * index of an array, size of an array, count how many items in an array
 * have a special property
 */

#define CW_INDEX_NONE			-1
#define CW_INDEX_INVALID		CW_INDEX_NONE
#define CW_COUNT_NONE			CW_INDEX_NONE
#define CW_COUNT_INVALID		CW_COUNT_NONE
#define CW_SIZE_NONE			CW_INDEX_NONE
#define CW_SIZE_INVALID			CW_SIZE_NONE

typedef cw_s32_t			cw_index_t;
typedef cw_index_t			cw_size_t;
typedef cw_index_t			cw_count_t;
typedef cw_s64_t			cw_index64_t;
typedef cw_index64_t			cw_count64_t;
typedef cw_index64_t			cw_size64_t;

/*
 * just signal to the caller true or false, no additional values are needed.
 * this allows calls like: if (! function(...)) ...
 */

#define CW_BOOL_FALSE			0
#define CW_BOOL_FAIL			CW_BOOL_FALSE
#define CW_BOOL_TRUE			1
#define CW_BOOL_OK			CW_BOOL_TRUE

typedef cw_s32_t			cw_bool_t;

/* to count pico and milli seconds */

typedef cw_s32_t			cw_psecs_t;
typedef cw_s32_t			cw_msecs_t;

/* to select a type or a mode or for a bit mask of flags */

#define CW_TYPE_NONE			0
#define CW_TYPE_FREE			CW_TYPE_NONE
#define CW_MODE_NONE			CW_TYPE_NONE
#define CW_MODE_FREE			CW_MODE_NONE
#define CW_FLAG_NONE			CW_TYPE_NONE
#define CW_FLAG_FREE			CW_FLAG_NONE

typedef cw_u32_t			cw_type_t;
typedef cw_type_t			cw_mode_t;
typedef cw_type_t			cw_flag_t;



#endif /* !CW_TYPES_H */
/******************************************************** Karsten Scheibler */

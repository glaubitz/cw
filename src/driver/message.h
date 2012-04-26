/****************************************************************************
 ****************************************************************************
 *
 * message.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CW_MESSAGE_H
#define CW_MESSAGE_H

#define CW_NAME				"cw"

#ifdef CW_DEBUG
#define cw_notice(args...)							\
	do									\
		{								\
		printk(KERN_NOTICE CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)

#define cw_error(args...)							\
	do									\
		{								\
		printk(KERN_ERR CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)

#define cw_debug(level, args...)						\
	do									\
		{								\
		if (cw_debug_level < level) break;					\
		printk(KERN_DEBUG CW_NAME ":%s:%d: ", __FILE__, __LINE__);	\
		printk(args);							\
		printk("\n");							\
		}								\
	while (0)
#else /* CW_DEBUG */
#define cw_notice(args...)					\
	do							\
		{						\
		printk(KERN_NOTICE CW_NAME ": " args);		\
		printk("\n");					\
		}						\
	while (0)

#define cw_error(args...)					\
	do							\
		{						\
		printk(KERN_ERR CW_NAME ": " args);		\
		printk("\n");					\
		}						\
	while (0)

#define cw_debug(args...)
#endif /* CW_DEBUG */



#endif /* !CW_MESSAGE_H */
/******************************************************** Karsten Scheibler */

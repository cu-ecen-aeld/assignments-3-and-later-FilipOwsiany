/*
 * common.h
 *
 *  Created on: Sep 18, 2025
 *      Author: Filip Owsiany
 */

#ifndef COMMON_H_
#define COMMON_H_

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#endif

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

void* my_memcpy(void* dest, const void* src, size_t n);
void* my_malloc(size_t size);
void my_free(void* ptr);
void* my_memset(void*s, int c, size_t n);

#endif /* COMMON_H_ */

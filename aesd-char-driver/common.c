#include "common.h"

#ifdef __KERNEL__
    #include <linux/string.h>
    #include <linux/stdarg.h>
    #include <linux/slab.h>
#else
    #include <stdlib.h>
    #include <string.h>
    #include <stdio.h>
    #include <stdarg.h>
#endif

void* my_memcpy(void* dest, const void* src, size_t n)
{
    return memcpy(dest, src, n);
}

void* my_malloc(size_t size)
{
#ifdef __KERNEL__
    return kmalloc(size, GFP_KERNEL);
#else
    return malloc(size);
#endif
}

void my_free(void* ptr)
{
#ifdef __KERNEL__
    kfree(ptr);
#else
    free(ptr);
#endif
}

void* my_memset(void*s, int c, size_t n)
{
    return memset(s, c, n);
}
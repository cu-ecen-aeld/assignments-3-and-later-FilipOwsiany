/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

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

int my_printf(const char *format, ...)
{
   va_list arg;
   int done;

   va_start (arg, format);
#ifdef __KERNEL__
   done = vprintk(format, arg); 
#else
   done = vfprintf (stdout, format, arg);
#endif
   va_end (arg);

   return done;
}

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

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    if (buffer == NULL || char_offset < 0) 
    {
        my_printf("Error: Invalid parameters passed to aesd_circular_buffer_find_entry_offset_for_fpos\n");
        return NULL; // Handle null pointers gracefully
    }
    my_printf("aesd_circular_buffer_find_entry_offset_for_fpos called with char_offset: %zu\n", char_offset);
    
    size_t current_offset = 0;
    size_t buffer_index = buffer->out_offs;

    size_t i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) 
    {   
        my_printf("entry %zu\n", buffer_index);
        my_printf("entry %zu: buffptr: %p\n", 
                    buffer_index, buffer->entry[buffer_index].buffptr);
        if (buffer->entry[buffer_index].buffptr != NULL) 
        {
            // my_printf("Checking entry %zu: buffptr: %s, size: %zu\n", 
            //         buffer_index, buffer->entry[buffer_index].buffptr, buffer->entry[buffer_index].size);
            if(current_offset <= char_offset && 
                char_offset < current_offset + buffer->entry[buffer_index].size) 
            {
                if (entry_offset_byte_rtn != NULL) 
                {
                    *entry_offset_byte_rtn = char_offset - current_offset;
                }
                my_printf("Found entry at index %zu with offset %zu\n", buffer_index, *entry_offset_byte_rtn);
                return &buffer->entry[buffer_index];
            }
            current_offset += buffer->entry[buffer_index].size;
        } 
        else
        {
            return NULL;
        }
        buffer_index = (buffer_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    my_printf("aesd_circular_buffer_add_entry called\n");

    if (!buffer || !add_entry) 
    {
        my_printf("Error: add_entry or buffer is NULL\n");
        return;
    }

    // Jeśli bufor jest pełny, zwalniamy najstarszy wpis
    if (buffer->full) 
    {
        my_printf("Buffer full. Overwriting entry at out_offs=%d\n", buffer->out_offs);
        my_free((void *)buffer->entry[buffer->out_offs].buffptr);
        buffer->entry[buffer->out_offs].buffptr = NULL;
        buffer->entry[buffer->out_offs].size = 0;

        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Skopiuj dane
    buffer->entry[buffer->in_offs].buffptr = my_malloc(add_entry->size);
    if (!buffer->entry[buffer->in_offs].buffptr) 
    {
        my_printf("Error: Memory allocation failed\n");
        return;
    }
    my_memcpy((void *)buffer->entry[buffer->in_offs].buffptr, add_entry->buffptr, add_entry->size);
    buffer->entry[buffer->in_offs].size = add_entry->size;

    my_printf("Added entry at in_offs=%d, size=%zu\n", buffer->in_offs, add_entry->size);

    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    buffer->full = (buffer->in_offs == buffer->out_offs);
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    if (buffer == NULL) 
    {
        my_printf("Error: buffer is NULL\n");
        return; // Handle null pointer gracefully
    }
    my_printf("aesd_circular_buffer_init called\n");
    my_memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

/**
* Cleanup the circular buffer described by @param buffer
*/
void aesd_circular_buffer_cleanup(struct aesd_circular_buffer *buffer)
{
    size_t i = 0;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) 
    {
        if(buffer->entry[i].buffptr != NULL)
        {
            my_free(buffer->entry[i].buffptr);
        }
    }   
}


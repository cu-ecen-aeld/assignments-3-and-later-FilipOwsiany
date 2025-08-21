/**
 * @file aesd_circular_buffer.c
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

#include "aesd_circular_buffer.h"
#include "common.h"

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
        PDEBUG("Error: Invalid parameters passed to aesd_circular_buffer_find_entry_offset_for_fpos\n");
        return NULL; // Handle null pointers gracefully
    }
    PDEBUG("aesd_circular_buffer_find_entry_offset_for_fpos called with char_offset: %zu\n", char_offset);
    
    size_t current_offset = 0;
    size_t buffer_index = buffer->out_offs;

    size_t i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) 
    {   
        PDEBUG("entry %zu\n", buffer_index);
        PDEBUG("entry %zu: buffptr: %p\n", 
                    buffer_index, buffer->entry[buffer_index].buffptr);
        if (buffer->entry[buffer_index].buffptr != NULL) 
        {
            if(current_offset <= char_offset && 
                char_offset < current_offset + buffer->entry[buffer_index].size) 
            {
                if (entry_offset_byte_rtn != NULL) 
                {
                    *entry_offset_byte_rtn = char_offset - current_offset;
                }
                PDEBUG("Found entry at index %zu with offset %zu\n", buffer_index, *entry_offset_byte_rtn);
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

extern struct aesd_buffer_entry *aesd_circular_buffer_find_entry_for_ioctl(struct aesd_circular_buffer *buffer,
            size_t write_cmd, size_t write_cmd_offset, size_t *entry_offset_byte_rtn)
{
    if (buffer == NULL || write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED || write_cmd_offset < 0) 
    {
        PDEBUG("Error: Invalid parameters passed to aesd_circular_buffer_find_entry_for_ioctl\n");
        return NULL; // Handle null pointers gracefully
    }
    PDEBUG("aesd_circular_buffer_find_entry_for_ioctl called with write_cmd: %zu, write_cmd_offset: %zu\n", write_cmd, write_cmd_offset);

    size_t current_offset = 0;
    size_t buffer_index = buffer->out_offs;

    size_t i;
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) 
    {   
        PDEBUG("entry %zu\n", buffer_index);
        PDEBUG("entry %zu: buffptr: %p\n", 
                    buffer_index, buffer->entry[buffer_index].buffptr);
        if (buffer->entry[buffer_index].buffptr != NULL) 
        {
            if (i == write_cmd)
            {
                if (write_cmd_offset >= buffer->entry[buffer_index].size) 
                {
                    PDEBUG("Error: write_cmd_offset %zu exceeds size %zu of entry %zu\n", 
                            write_cmd_offset, buffer->entry[buffer_index].size, buffer_index);

                    *entry_offset_byte_rtn = 0;
                    return NULL; // Invalid offset
                }
                PDEBUG("Found entry for ioctl at index %zu\n", buffer_index);
                current_offset += write_cmd_offset;
                if (entry_offset_byte_rtn != NULL) 
                {
                    *entry_offset_byte_rtn = current_offset;
                }
                return &buffer->entry[buffer_index];
            }
            current_offset += buffer->entry[buffer_index].size;
        } 
        else
        {
            *entry_offset_byte_rtn = 0;
            return NULL;
        }
        buffer_index = (buffer_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    *entry_offset_byte_rtn = 0;
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
    PDEBUG("aesd_circular_buffer_add_entry called\n");

    if (!buffer || !add_entry) 
    {
        PDEBUG("Error: add_entry or buffer is NULL\n");
        return;
    }

    // Jeśli bufor jest pełny, zwalniamy najstarszy wpis
    if (buffer->full) 
    {
        PDEBUG("Buffer full. Overwriting entry at out_offs=%d\n", buffer->out_offs);
        my_free((void *)buffer->entry[buffer->out_offs].buffptr);
        buffer->size -= buffer->entry[buffer->out_offs].size;
        buffer->entry[buffer->out_offs].buffptr = NULL;
        buffer->entry[buffer->out_offs].size = 0;

        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // Skopiuj dane
    buffer->entry[buffer->in_offs].buffptr = my_malloc(add_entry->size);
    if (!buffer->entry[buffer->in_offs].buffptr) 
    {
        PDEBUG("Error: Memory allocation failed\n");
        return;
    }
    my_memcpy((void *)buffer->entry[buffer->in_offs].buffptr, add_entry->buffptr, add_entry->size);
    buffer->size += add_entry->size;
    PDEBUG("Buffer size after adding entry: %zu\n", buffer->size);
    buffer->entry[buffer->in_offs].size = add_entry->size;
    PDEBUG("Added entry at in_offs=%d, size=%zu\n", buffer->in_offs, add_entry->size);
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
        PDEBUG("Error: buffer is NULL\n");
        return; // Handle null pointer gracefully
    }
    PDEBUG("aesd_circular_buffer_init called\n");
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


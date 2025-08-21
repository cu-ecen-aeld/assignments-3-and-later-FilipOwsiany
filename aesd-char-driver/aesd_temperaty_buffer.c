
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

#include "aesd_temperaty_buffer.h"
#include "common.h"

void aesd_temperary_buffer_init(struct aesd_temperary_buffer *bufferTemperary)
{
    bufferTemperary->buffptr = NULL;
    bufferTemperary->size = 0;
}

void aesd_temperary_buffer_clean(struct aesd_temperary_buffer *bufferTemperary)
{
    if (NULL != bufferTemperary->buffptr)
    {
        PDEBUG("Cleaning temporary buffer\n");
        // Free the memory allocated for the buffer
        kfree((void *)bufferTemperary->buffptr);
        bufferTemperary->buffptr = NULL;
        bufferTemperary->size = 0;
    }
    else
    {
        PDEBUG("Temporary buffer is already clean\n");
    }
}

bool aesd_temperary_buffer_is_empty(struct aesd_temperary_buffer *bufferTemperary)
{
    return (bufferTemperary->buffptr == NULL || bufferTemperary->size == 0);
}

bool aesd_temperary_buffer_add(struct aesd_temperary_buffer *bufferTemperary, const char *data, size_t size)
{

    char* buffptrLast = bufferTemperary->buffptr;

    if (buffptrLast == NULL)
    {
        // If the buffer is empty, allocate new memory
        buffptrLast = kmalloc(size, GFP_KERNEL);
        if (buffptrLast == NULL)
        {
            return false;
        }
        memcpy(buffptrLast, data, size);
        bufferTemperary->buffptr = buffptrLast;
        bufferTemperary->size = size;
        return true;
    }    

    char* buffptrNew = kmalloc(bufferTemperary->size + size, GFP_KERNEL);
    if (NULL == buffptrNew)
    {
        return false;
    }
    
    memcpy(buffptrNew, buffptrLast, bufferTemperary->size);
    memcpy(buffptrNew + bufferTemperary->size, data, size);
    kfree(buffptrLast);
    bufferTemperary->buffptr = buffptrNew;
    bufferTemperary->size += size;

    return true;
}

void aesd_temperary_buffer_delete(struct aesd_temperary_buffer *bufferTemperary)
{
    if (NULL != bufferTemperary->buffptr)
    {
        kfree(bufferTemperary->buffptr);
        bufferTemperary->buffptr = NULL;
        bufferTemperary->size = 0;
    }
}
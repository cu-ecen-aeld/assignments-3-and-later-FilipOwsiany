
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "aesd_temperaty_buffer.h"

void aesd_temperary_buffer_init(struct aesd_temperary_buffer *bufferTemperary)
{
    bufferTemperary->buffptr = NULL;
    bufferTemperary->size = 0;
}

void aesd_temperary_buffer_clean(struct aesd_temperary_buffer *bufferTemperary)
{
    if (NULL != bufferTemperary->buffptr)
    {
        printf("Cleaning temporary buffer\n");
        // Free the memory allocated for the buffer
        free((void *)bufferTemperary->buffptr);
        bufferTemperary->buffptr = NULL;
        bufferTemperary->size = 0;
    }
    else
    {
        printf("Temporary buffer is already clean\n");
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
        buffptrLast = malloc(size);
        if (buffptrLast == NULL)
        {
            return false;
        }
        memcpy(buffptrLast, data, size);
        bufferTemperary->buffptr = buffptrLast;
        bufferTemperary->size = size;
        return true;
    }    

    char* buffptrNew = malloc(bufferTemperary->size + size);
    if (NULL == buffptrNew)
    {
        return false;
    }
    
    memcpy(buffptrNew, buffptrLast, bufferTemperary->size);
    memcpy(buffptrNew + bufferTemperary->size, data, size);
    free(buffptrLast);
    bufferTemperary->buffptr = buffptrNew;
    bufferTemperary->size += size;

    return true;
}

void aesd_temperary_buffer_delete(struct aesd_temperary_buffer *bufferTemperary)
{
    if (NULL != bufferTemperary->buffptr)
    {
        free(bufferTemperary->buffptr);
        bufferTemperary->buffptr = NULL;
        bufferTemperary->size = 0;
    }
}
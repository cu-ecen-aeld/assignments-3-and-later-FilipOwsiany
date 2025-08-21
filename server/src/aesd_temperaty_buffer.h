/*
 * aesd_temperaty_buffer.h
 *
 *  Created on: September 18th, 2025
 *      Author: Dan Walkes
 */

#ifndef AESD_TEMPERARY_BUFFER_H
#define AESD_TEMPERARY_BUFFER_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#endif

struct aesd_temperary_buffer
{
    char *buffptr;
    size_t size;
};

void aesd_temperary_buffer_init(struct aesd_temperary_buffer *bufferTemperary);
void aesd_temperary_buffer_clean(struct aesd_temperary_buffer *bufferTemperary);

bool aesd_temperary_buffer_is_empty(struct aesd_temperary_buffer *bufferTemperary);


bool aesd_temperary_buffer_add(struct aesd_temperary_buffer *bufferTemperary, const char *data, size_t size);
void aesd_temperary_buffer_delete(struct aesd_temperary_buffer *bufferTemperary);

#endif /* AESD_TEMPERARY_BUFFER_H */

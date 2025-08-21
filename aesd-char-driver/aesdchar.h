/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include "aesd_circular_buffer.h"
#include "aesd_temperaty_buffer.h"

#include <linux/cdev.h>

struct aesd_dev
{
    struct     aesd_temperary_buffer *bufferTemperary; /* Temperary buffer pointer    */
    struct     aesd_circular_buffer *bufferCircular;   /* Circular buffer pointer     */
    struct     cdev cdev;                              /* Char device structure       */
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */

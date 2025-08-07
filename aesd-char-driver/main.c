/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Your Name Here"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open\n");
    /**
     * TODO: handle open
     */
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release\n");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld\n",count,*f_pos);
    size_t entry_offset = 0;
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_circular_buffer *buffer = dev->buffer;

    struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(buffer, *f_pos, &entry_offset);

    if (!entry)
    {
        return 0; // No data to read
    }

    size_t available = entry->size - entry_offset;
    size_t to_copy = min(count, available);

    if (copy_to_user(buf, entry->buffptr + entry_offset, to_copy))
    {
        return -EFAULT;
    }

    *f_pos += to_copy;
    return to_copy;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("write %zu bytes with offset %lld\n",count,*f_pos);

    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_circular_buffer *buffer = dev->buffer;

    char *kbuf = kmalloc(count, GFP_KERNEL);
    
    if (!kbuf)
    {
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buf, count)) 
    {
        kfree(kbuf);
        return -EFAULT;
    }

    bool newline = false;
    size_t i = 0;
    for (i = 0; i < count; i++) {
        if (kbuf[i] == '\n') {
            newline = true;
            break;
        }
    }

    if (newline == true && dev->current_command == NULL)
    {
        PDEBUG("newline == true && dev->current_command == NULL\n");
        struct aesd_buffer_entry entry = {
            .buffptr = kbuf,
            .size = count
        };

        aesd_circular_buffer_add_entry(buffer, &entry);

        dev->current_command = NULL;
        dev->current_command_size = 0;

    }
    else if (newline == true && dev->current_command != NULL)
    {
        PDEBUG("newline == true && dev->current_command != NULL\n");
        char* current_command_part_last = dev->current_command;
        char* current_command_done = kmalloc(dev->current_command_size + count, GFP_KERNEL);
        if (!current_command_done)
        {
            return -ENOMEM;
        }

        memcpy(current_command_done, current_command_part_last, dev->current_command_size);
        memcpy(current_command_done + dev->current_command_size, kbuf, count);
        kfree(current_command_part_last);

        size_t i = 0;
        for (i = 0; i < dev->current_command_size + count; i++)
        {
            PDEBUG("byte: %d\n", current_command_done[i]);
        }

        struct aesd_buffer_entry entry = {
            .buffptr = current_command_done,
            .size = dev->current_command_size + count,
        };

        aesd_circular_buffer_add_entry(buffer, &entry);    
        
        kfree(current_command_done);
        dev->current_command = NULL;
        dev->current_command_size = 0;
    }
    else
    {
        PDEBUG("else\n");
        char* current_command_part_last = dev->current_command;
        char* current_command_part = kmalloc(dev->current_command_size + count, GFP_KERNEL);
        if (!current_command_part)
        {
            return -ENOMEM;
        }
        if (current_command_part_last)
        {
            memcpy(current_command_part, current_command_part_last, dev->current_command_size);
            memcpy(current_command_part + dev->current_command_size, kbuf, count);
            kfree(current_command_part_last);
        }
        else
        {
            memcpy(current_command_part, kbuf, count);
        }
        
        dev->current_command = current_command_part;
        dev->current_command_size = dev->current_command_size + count;

        size_t i = 0;
        for (i = 0; i < dev->current_command_size; i++)
        {
            PDEBUG("byte: %d\n", dev->current_command[i]);
        }
        
    }

    kfree(kbuf);
    return count;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev\n", err);
    }
    return err;
}



int aesd_init_module(void)
{
    PDEBUG("init\n");
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);

    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device,0,sizeof(struct aesd_dev));
    aesd_device.buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    aesd_device.current_command = NULL;
    aesd_device.current_command_size = 0;
    
    if (aesd_device.buffer == NULL)
    {
        return -ENOMEM;
    }
    
    aesd_circular_buffer_init(aesd_device.buffer);

    /**
     * TODO: initialize the AESD specific portion of the device
     */

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    PDEBUG("major=%d minor=%d\n", aesd_major, aesd_minor);
    return result;

}

void aesd_cleanup_module(void)
{
    PDEBUG("cleanup\n");
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    aesd_circular_buffer_cleanup(aesd_device.buffer);

    if (aesd_device.buffer != NULL)
    {
        kfree(aesd_device.buffer);
    }

    if (aesd_device.current_command != NULL)
    {
        kfree(aesd_device.current_command);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

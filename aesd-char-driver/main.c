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

#include "aesd_ioctl.h"
#include "aesdchar.h"
#include "common.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Filip Owsiany :)");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open\n");
    filp->private_data = container_of(inode->i_cdev, struct aesd_dev, cdev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release\n");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld\n",count,*f_pos);
    size_t entry_offset = 0;
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_circular_buffer *bufferCircular = dev->bufferCircular;

    struct aesd_buffer_entry *entry = aesd_circular_buffer_find_entry_offset_for_fpos(bufferCircular, *f_pos, &entry_offset);

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
    struct aesd_temperary_buffer *bufferTemperary = dev->bufferTemperary;
    struct aesd_circular_buffer *bufferCircular = dev->bufferCircular;

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

    if (newline == true && aesd_temperary_buffer_is_empty(bufferTemperary))
    {
        struct aesd_buffer_entry entry = {
            .buffptr = kbuf,
            .size = count
        };
        aesd_circular_buffer_add_entry(bufferCircular, &entry);
    }
    else if (newline == true && !aesd_temperary_buffer_is_empty(bufferTemperary))
    {
        if(aesd_temperary_buffer_add(bufferTemperary, kbuf, count)!= true)
        {
            kfree(kbuf);
            return -ENOMEM; // Memory allocation failed
        }
 
        struct aesd_buffer_entry entry = {
            .buffptr = bufferTemperary->buffptr,
            .size = bufferTemperary->size,
        };

        aesd_circular_buffer_add_entry(bufferCircular, &entry);

        aesd_temperary_buffer_delete(bufferTemperary);
    }
    else
    {
        if(aesd_temperary_buffer_add(bufferTemperary, kbuf, count)!= true)
        {
            kfree(kbuf);
            return -ENOMEM; // Memory allocation failed
        }
    }

    kfree(kbuf);
    return count;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_circular_buffer *bufferCircular = dev->bufferCircular;

    PDEBUG("llseek %lld %d\n", offset, whence);

    switch (whence)
    {
    case SEEK_SET:
        if (offset < 0)
        {
            return -EINVAL;
        }
        filp->f_pos = offset;
        break;
    case SEEK_CUR:
        if (filp->f_pos + offset < 0)
        {
            return -EINVAL;
        }
        filp->f_pos += offset;
        break;
    case SEEK_END:
        filp->f_pos = bufferCircular->size + offset;
        break;
    
    default:
        break;
    }

    return filp->f_pos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    PDEBUG("ioctl\n");

    struct aesd_dev *dev = filp->private_data;
    struct aesd_circular_buffer *bufferCircular = dev->bufferCircular;

    struct aesd_seekto seekto;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
    {
        return -EINVAL;
    }
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
    {
        return -EINVAL;
    } 

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO:
            if (copy_from_user(&seekto, (void __user *)arg, sizeof(seekto)))
            {
                return -EFAULT;
            }
            PDEBUG("ioctl seekto: write_cmd=%u, write_cmd_offset=%u\n", seekto.write_cmd, seekto.write_cmd_offset);

            if (seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED)
            {
                return -EINVAL; // Invalid write command
            }

            if (seekto.write_cmd_offset >= bufferCircular->entry[seekto.write_cmd].size)
            {
                return -EINVAL; // Invalid offset within the write command
            }

            size_t offset = 0;

            if(aesd_circular_buffer_find_entry_for_ioctl(bufferCircular,
                seekto.write_cmd, seekto.write_cmd_offset, &offset) == NULL)
            {
                PDEBUG("No entry found for ioctl\n");
            }
            else
            {
                PDEBUG("Found entry for ioctl at offset %zu\n", offset);
            }

            filp->f_pos = offset;
            PDEBUG("New file position: %lld\n", filp->f_pos);

            return 0;

        default:
            return -EINVAL;
        }
}

struct file_operations aesd_fops = {
    .owner =             THIS_MODULE,
    .read  =             aesd_read,
    .write =             aesd_write,
    .unlocked_ioctl =    aesd_ioctl,
    .llseek =            aesd_llseek,
    .open =              aesd_open,
    .release =           aesd_release,
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
    aesd_device.bufferCircular = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);    
    if (aesd_device.bufferCircular == NULL)
    {
        return -ENOMEM;
    }
    
    aesd_circular_buffer_init(aesd_device.bufferCircular);

    aesd_device.bufferTemperary = kmalloc(sizeof(struct aesd_temperary_buffer), GFP_KERNEL);
    if (aesd_device.bufferTemperary == NULL)
    {
        return -ENOMEM;
    }

    aesd_temperary_buffer_init(aesd_device.bufferTemperary);    

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

    aesd_circular_buffer_cleanup(aesd_device.bufferCircular);

    if (aesd_device.bufferCircular != NULL)
    {
        kfree(aesd_device.bufferCircular);
    }

    aesd_temperary_buffer_clean(aesd_device.bufferTemperary);

    if (aesd_device.bufferTemperary != NULL)
    {
        kfree(aesd_device.bufferTemperary);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

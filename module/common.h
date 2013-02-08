#ifndef COMMON_H
#define COMMON_H

#include <linux/version.h>
#include <linux/semaphore.h>

#if (LINUX_VERSION_CODE < 0x020630)
#define OLD_LUNARIS_KERNEL
#endif

#define DRIVER_NAME "physicalmemory"
#define PRINTK_PREFIX DRIVER_NAME ": "

// Resources
extern struct resource* region;

// Plumbing
extern struct semaphore data_lock;

struct file_data
{
  struct list_head allocated;
};


#endif

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/device.h>

#include "common.h"
#include "physicalmemory_ioctl.h"
#include "allocate.h"

// Metadata
MODULE_AUTHOR("Kees-Jan Dijkzeul");
MODULE_LICENSE("GPL");
#define DRIVER_NAME "physicalmemory"
#define PRINTK_PREFIX DRIVER_NAME ": "

// Parameters
static unsigned long start = 0;
static unsigned long size = 0;
static unsigned long end = 0;
module_param(start, ulong, S_IRUGO);
module_param(size, ulong, S_IRUGO);
module_param(end, ulong, S_IRUGO);

// Resources
static int physicalmemory_major = 0;
struct resource* region = NULL;
static u64 mappedMemory = 0;

// Plumbing
#ifdef OLD_LUNARIS_KERNEL
DECLARE_MUTEX(data_lock);
#else
DEFINE_SEMAPHORE(data_lock);
#endif

#ifdef OLD_LUNARIS_KERNEL
static void __wbinvd(void *dummy)
{
	wbinvd();
}

static int wbinvd_on_all_cpus(void)
{
  int result = on_each_cpu(__wbinvd, NULL, 1);
  return result;
}
#endif

static int check_parameters(void)
{
  if(start && size && end)
  {
    if(end != start + size)
    {
      printk(KERN_WARNING PRINTK_PREFIX "ERROR: start: 0x%010lX, end: 0x%010lX, size: 0x%010lX: "
             "size and end don't match\n",
             start, end, size);

      return -EINVAL;
    }
  }
  if(!start)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: start address not given\n");
    return -EINVAL;
  }
  if(!end && !size)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: neither end nor size given\n");
    return -EINVAL;
  }
  if(start & ~PAGE_MASK)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: start is not page aligned\n");
    return -EINVAL;
  }
  if(size & ~PAGE_MASK)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: size is not page aligned\n");
    return -EINVAL;
  }
  if(end & ~PAGE_MASK)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: end is not page aligned\n");
    return -EINVAL;
  }

  if(!end)
    end = start+size;
  if(!size)
    size = end-start;

  return 0;
}

static int __init obtain_memory(void)
{
  region = request_mem_region(start, size, DRIVER_NAME);
  if(!region)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: request_mem_region failed\n");
    return -ENOMEM;
  }

  mappedMemory = (u64)ioremap_cache(start, size);
  if(!mappedMemory)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: ioremap failed\n");
    return -ENOMEM;
  }

  printk(KERN_NOTICE PRINTK_PREFIX "Mapped physical memory to address 0x%010llX\n", mappedMemory);

  return 0;
}

static void release_memory(void)
{
  if(mappedMemory)
  {
    iounmap((u64*)mappedMemory);
    mappedMemory = 0;
  }

  if(region)
  {
    release_mem_region(start, size);
    region = NULL;
  }
}

static int physicalmemory_open (struct inode *inode, struct file *filp)
{
  struct file_data* private = NULL;
  // printk(KERN_NOTICE PRINTK_PREFIX "Open\n");

  private = kzalloc(sizeof(*private), GFP_KERNEL);
  if(!private)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Failed to allocate memory for bookkeeping\n");
    return -ENOMEM;
  }

  INIT_LIST_HEAD(&private->allocated);
  filp->private_data = private;
  return 0;
}

static int physicalmemory_release(struct inode *inode, struct file *filp)
{
  int result = 0;
  // printk(KERN_NOTICE PRINTK_PREFIX "Release\n");
  
  result = physicalmemory_free_all(filp);
  
  BUG_ON(!filp->private_data);
  kfree(filp->private_data);
  filp->private_data = NULL;

  return result;
}

static long physicalmemory_ioctl(struct file *f, unsigned cmd, unsigned long arg)
{
  const unsigned int type = _IOC_TYPE(cmd);
  const unsigned int nr = _IOC_NR(cmd);

  // printk(KERN_NOTICE PRINTK_PREFIX "ioctl\n");

  if(type != PHYSICALMEMORY_IOCTL_TYPE)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: ioctl type %u unknown\n", type);
    return -ENOTTY;
  }

  if(nr>=PHYSICALMEMORY_IOCTL_COUNT)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: physicalmemory_ioctl %u unknown\n", nr);
    return -ENOTTY;
  }

  switch(cmd)
  {
  case PHYSICALMEMORY_IOCTL_FLUSH_CACHE:
    printk(KERN_NOTICE PRINTK_PREFIX "Flushing caches\n");
    wbinvd_on_all_cpus();
    mb();
    break;
  case PHYSICALMEMORY_IOCTL_ALLOCATE:
    printk(KERN_NOTICE PRINTK_PREFIX "Allocate buffer\n");
    return physicalmemory_allocate(f, (struct MemoryBlock*)arg);
    break;
  case PHYSICALMEMORY_IOCTL_FREE:
    printk(KERN_NOTICE PRINTK_PREFIX "Free buffer\n");
    return physicalmemory_free(f, (struct MemoryBlock*)arg);
    break;
  default:
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: ioctl %x not handled\n", cmd);
    break;
  }
  
  return 0;
}


/*
 * Set up the cdev structure for a device.
 */
static void physicalmemory_setup_cdev(struct cdev *dev, int minor,
                                      struct file_operations *fops)
{
  int err, devno = MKDEV(physicalmemory_major, minor);

  cdev_init(dev, fops);
  dev->owner = THIS_MODULE;
  dev->ops = fops;
  err = cdev_add (dev, devno, 1);
  /* Fail gracefully if need be */
  if (err)
    printk (KERN_NOTICE "Error %d adding physicalmemory%d", err, minor);
}


/*
 * Our various sub-devices.
 */
/* Device 0 uses remap_pfn_range */
static struct file_operations physicalmemory_remap_ops = {
  .owner          = THIS_MODULE,
  .open           = physicalmemory_open,
  .release        = physicalmemory_release,
  .unlocked_ioctl = physicalmemory_ioctl,
  .mmap           = physicalmemory_remap_mmap,
};

#define MAX_PHYSICALMEMORY_DEV 1

/*
 * We export two physicalmemory devices.  There's no need for us to maintain any
 * special housekeeping info, so we just deal with raw cdevs.
 */
static struct cdev PhysicalmemoryDevs[MAX_PHYSICALMEMORY_DEV];

static void physicalmemory_cleanup(void)
{
  printk(KERN_NOTICE PRINTK_PREFIX "Cleanup\n");
  if(physicalmemory_major)
  {
    printk(KERN_NOTICE PRINTK_PREFIX "Unregistering devices\n");
    cdev_del(PhysicalmemoryDevs);
    unregister_chrdev_region(MKDEV(physicalmemory_major, 0), 2);
  }

  release_memory();
}

static int __init physicalmemory_init(void)
{
  int result;
  dev_t dev = MKDEV(physicalmemory_major, 0);

  printk(KERN_NOTICE PRINTK_PREFIX "Init\n");
  printk(KERN_NOTICE "REQUESTED start: 0x%010lX, size: 0x%010lX, end: 0x%010lX\n", start, size, end );
  result = check_parameters();
  if(result < 0)
    goto fail;

  result = obtain_memory();
  if(result < 0)
    goto fail;

  printk(KERN_NOTICE "OBTAINED start: 0x%010lX, size: 0x%010lX, end: 0x%010lX\n", start, size, end );

  /* Figure out our device number. */
  if (physicalmemory_major)
    result = register_chrdev_region(dev, 2, DRIVER_NAME);
  else {
    result = alloc_chrdev_region(&dev, 0, 2, DRIVER_NAME);
    physicalmemory_major = MAJOR(dev);
  }
  if (result < 0) {
    printk(KERN_WARNING "physicalmemory: unable to get major %d\n", physicalmemory_major);
    goto fail;
  }
  if (physicalmemory_major == 0)
  {
    physicalmemory_major = result;
    result = 0;
  }

  /* Now set up two cdevs. */
  physicalmemory_setup_cdev(PhysicalmemoryDevs, 0, &physicalmemory_remap_ops);
  return 0;

 fail:
  physicalmemory_cleanup();
  return result;
}


module_init(physicalmemory_init);
module_exit(physicalmemory_cleanup);

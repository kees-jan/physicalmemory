// #include <linux/config.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <asm/page.h>
#include <linux/cdev.h>

#include <linux/device.h>

// Metadata
MODULE_AUTHOR("Kees-Jan Dijkzeul");
MODULE_LICENSE("GPL");
#define DRIVER_NAME "physicalmemory"

// Parameters
static unsigned long start = 0;
static unsigned long size = 0;
static unsigned long end = 0;
module_param(start, ulong, S_IRUGO);
module_param(size, ulong, S_IRUGO);
module_param(end, ulong, S_IRUGO);

// Resources
static int physicalmemory_major = 0;
static struct resource* region = NULL;
static u64 mappedMemory = 0;

static int check_parameters(void)
{
  if(start && size && end)
  {
    if(end != start + size)
    {
      printk(KERN_WARNING "PhysicalMemory: ERROR: start: 0x010%lX, end: 0x010%lX, size: 0x010%lX: size and end don't match\n",
             start, end, size);

      return -EINVAL;
    }
  }
  if(!start)
  {
    printk(KERN_WARNING "PhysicalMemory: ERROR: start address not given\n");
    return -EINVAL;
  }
  if(!end && !size)
  {
    printk(KERN_WARNING "PhysicalMemory: ERROR: end and or size not given\n");
    return -EINVAL;
  }
  if(start & ~PAGE_MASK)
  {
    printk(KERN_WARNING "PhysicalMemory: ERROR: start is not page aligned\n");
    return -EINVAL;
  }
  if(size & ~PAGE_MASK)
  {
    printk(KERN_WARNING "PhysicalMemory: ERROR: size is not page aligned\n");
    return -EINVAL;
  }
  if(end & ~PAGE_MASK)
  {
    printk(KERN_WARNING "PhysicalMemory: ERROR: end is not page aligned\n");
    return -EINVAL;
  }
  
  if(!end)
    end = start+size;
  if(!size)
    size = end-start;

  return 0;
}

static int obtain_memory(void)
{
  region = request_mem_region(start, size, DRIVER_NAME);
  if(!region)
    return -ENOMEM;

  mappedMemory = (u64)ioremap(start, size);
  if(!mappedMemory)
    return -ENOMEM;
  
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

/*
 * Open the device; in fact, there's nothing to do here.
 */
static int physicalmemory_open (struct inode *inode, struct file *filp)
{
  return 0;
}


/*
 * Closing is just as simple.
 */
static int physicalmemory_release(struct inode *inode, struct file *filp)
{
  return 0;
}



/*
 * Common VMA ops.
 */

void physicalmemory_vma_open(struct vm_area_struct *vma)
{
  printk(KERN_NOTICE "Physicalmemory VMA open, virt %lx, phys %lx\n",
         vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

void physicalmemory_vma_close(struct vm_area_struct *vma)
{
  printk(KERN_NOTICE "Physicalmemory VMA close.\n");
}


/*
 * The remap_pfn_range version of mmap.  This one is heavily borrowed
 * from drivers/char/mem.c.
 */

static struct vm_operations_struct physicalmemory_remap_vm_ops = {
  .open =  physicalmemory_vma_open,
  .close = physicalmemory_vma_close,
};

static int physicalmemory_remap_mmap(struct file *filp, struct vm_area_struct *vma)
{
  if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                      vma->vm_end - vma->vm_start,
                      vma->vm_page_prot))
    return -EAGAIN;

  vma->vm_ops = &physicalmemory_remap_vm_ops;
  physicalmemory_vma_open(vma);
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
  .owner   = THIS_MODULE,
  .open    = physicalmemory_open,
  .release = physicalmemory_release,
  .mmap    = physicalmemory_remap_mmap,
};

#define MAX_PHYSICALMEMORY_DEV 1

#if 0
static struct file_operations *physicalmemory_fops[MAX_PHYSICALMEMORY_DEV] = {
  &physicalmemory_remap_ops,
};
#endif

/*
 * We export two physicalmemory devices.  There's no need for us to maintain any
 * special housekeeping info, so we just deal with raw cdevs.
 */
static struct cdev PhysicalmemoryDevs[MAX_PHYSICALMEMORY_DEV];

static void physicalmemory_cleanup(void)
{
  printk(KERN_NOTICE "PhysicalMemory: Cleanup\n");
  if(physicalmemory_major)
  {
    printk(KERN_NOTICE "PhysicalMemory: Unregistering devices\n");
    cdev_del(PhysicalmemoryDevs);
    unregister_chrdev_region(MKDEV(physicalmemory_major, 0), 2);
  }

  release_memory();
}

/*
 * Module housekeeping.
 */
static int __init physicalmemory_init(void)
{
  int result;
  dev_t dev = MKDEV(physicalmemory_major, 0);

  printk(KERN_NOTICE "PhysicalMemory: Init\n");
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

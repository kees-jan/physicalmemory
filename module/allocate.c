#include "allocate.h"

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include "common.h"

#define CACHED "cached"

struct block
{
  struct list_head list;
  struct resource* res;
  int refcount;
};

static inline void init_block(struct block* b)
{
  INIT_LIST_HEAD(&b->list);
  b->res = NULL;
  b->refcount = 0;
}
  
static int list_count(struct list_head* list)
{
  int count = 0;
  struct list_head* pos;

  list_for_each(pos, list)
    count++;

  return count;
}

static struct block* find(struct list_head* list, unsigned long physical_address, unsigned long size)
{
  struct block* pos;

  list_for_each_entry(pos, list, list)
  {
    BUG_ON(!pos->res);
    if(pos->res->start == physical_address &&
       (pos->res->end - pos->res->start + 1) == size)
    {
      return pos;
    }
  }

  return NULL;
}

int physicalmemory_allocate(struct file* f, struct MemoryBlock* arg)
{
  unsigned long size = 0;
  unsigned long align = PAGE_SIZE;
  struct resource *res = NULL;
  struct block *block = NULL;
  unsigned long physical_address = 0;
  int result = 0;
  struct file_data* fileData = f->private_data;

  BUG_ON(!fileData);
  BUG_ON(!region);
  if(copy_from_user(&size, &arg->size, sizeof(unsigned long)))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Can't read size\n");
    return -EPERM;
  }
  if(copy_from_user(&align, &arg->alignment, sizeof(unsigned long)))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Can't read alignment\n");
    return -EPERM;
  }
     
  printk(KERN_NOTICE PRINTK_PREFIX "Allocating buffer of size 0x%lX, aligned at 0x%lX\n", size, align);

  res = kzalloc(sizeof(*res), GFP_KERNEL);
  block = kzalloc(sizeof(*block), GFP_KERNEL);
  if(!res || !block)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Failed to allocate memory for bookkeeping\n");
    result = -ENOMEM;
    goto fail;
  }
  res->start = 0;
  res->end = 0;
  res->name = CACHED;

  result = allocate_resource(region, res, size, 0, -1, align, NULL, NULL);
  if(result < 0 )
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Failed to allocate memory\n");
    result = -ENOMEM;
    goto fail;
  }

  physical_address = res->start;
  copy_to_user(&arg->physicalAddress, &physical_address, sizeof(unsigned long));

  init_block(block);
  block->res = res;

  if(down_interruptible(&data_lock))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Interrupted\n");
    result = -EAGAIN;
    goto fail;
  }

  list_add(&block->list, &fileData->allocated);
  up(&data_lock);
  
  return 0;

 fail:
  if(res)
  {
    if(res->start)
      release_resource(res);
    kfree(res);
  }
  if(block) kfree(block);
  return result;
}

static void physicalmemory_free_block(struct block* p)
{
  BUG_ON(!p);
  BUG_ON(!p->res);
  
  printk(KERN_NOTICE PRINTK_PREFIX "Freeing buffer of size 0x%lX at 0x%010lX\n",
         (unsigned long)(p->res->end - p->res->start + 1), (unsigned long)(p->res->start));

  BUG_ON(release_resource(p->res));
  list_del_init(&p->list);

  kfree(p->res);
  kfree(p);
}

int physicalmemory_free(struct file* f, struct MemoryBlock* arg)
{
  unsigned long size = 0;
  unsigned long physical_address = 0;
  int result = -EINVAL;
  struct file_data* fileData = f->private_data;

  BUG_ON(!fileData);
  BUG_ON(!region);
  if(copy_from_user(&size, &arg->size, sizeof(unsigned long)))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Can't read size\n");
    return -EPERM;
  }
  if(copy_from_user(&physical_address, &arg->physicalAddress, sizeof(unsigned long)))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Can't read physicalAddress\n");
    return -EPERM;
  }
    
  printk(KERN_NOTICE PRINTK_PREFIX "Request free of buffer of size 0x%lX at 0x%010lX\n",
         size, physical_address);

  if(!down_interruptible(&data_lock))
  {
    struct block* pos = find(&fileData->allocated, physical_address, size);
    if(pos && !pos->refcount)
    {
      physicalmemory_free_block(pos);
      pos = NULL;
      result = 0;
    }
    up(&data_lock);

    if(pos)
    {
      printk(KERN_WARNING PRINTK_PREFIX "ERROR: Attempt to free memory that is still in use\n");
      result = -EINVAL;
    }
  }
  else
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Interrupted\n");
    result = -EAGAIN;
  }
  
  return result;
}

int physicalmemory_free_all(struct file* f)
{
  struct block* pos = NULL;
  struct block* temp = NULL;
  struct file_data* fileData = f->private_data;
  bool listIsEmtpy = true;
  
  BUG_ON(!fileData);
  
  down(&data_lock);
  printk(KERN_NOTICE PRINTK_PREFIX "Deallocate all %d buffers for process\n", list_count(&fileData->allocated));
  
  list_for_each_entry_safe(pos, temp, &fileData->allocated, list)
  {
    if(!pos->refcount)
      physicalmemory_free_block(pos);
  }
  listIsEmtpy = list_empty(&fileData->allocated);
  up(&data_lock);

  BUG_ON(!listIsEmtpy);
  
  return 0;
}

static struct vm_operations_struct physicalmemory_remap_vm_ops = {
  .open =  physicalmemory_vma_open,
  .close = physicalmemory_vma_close,
};

int physicalmemory_remap_mmap(struct file *f, struct vm_area_struct *vma)
{
  int result = 0;
  struct block* mem = NULL;
  struct file_data* fileData = f->private_data;
  BUG_ON(!fileData);

  printk(KERN_NOTICE PRINTK_PREFIX "VMA map, virt 0x%010lX, phys 0x%010lX, size 0x%010lX\n",
         vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
         vma->vm_end - vma->vm_start);
  
  if(down_interruptible(&data_lock))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Interrupted\n");
    return -EAGAIN;
  }

  mem = find(&fileData->allocated, vma->vm_pgoff << PAGE_SHIFT, vma->vm_end - vma->vm_start);
  if(!mem)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Region not allocated\n");
    result = -EINVAL;
    goto out;
  }
  
  if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
                      vma->vm_end - vma->vm_start,
                      vma->vm_page_prot))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: remap_pfn_range failed\n");
    result = -EAGAIN;
    goto out;
  }

  vma->vm_ops = &physicalmemory_remap_vm_ops;
  mem->refcount++;
  vma->vm_private_data = mem;

 out:
  up(&data_lock);
  return result;
}

void physicalmemory_vma_open(struct vm_area_struct *vma)
{
  struct block* mem = NULL;
  
  printk(KERN_NOTICE PRINTK_PREFIX "VMA open, virt 0x%010lX, phys 0x%010lX, size 0x%010lX\n",
         vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
         vma->vm_end - vma->vm_start);

  BUG_ON(!vma->vm_private_data);
  
  down(&data_lock);
  mem = (struct block*)vma->vm_private_data;
  mem->refcount++;
  up(&data_lock);
}

void physicalmemory_vma_close(struct vm_area_struct *vma)
{
  struct block* mem = NULL;

  printk(KERN_NOTICE PRINTK_PREFIX "VMA close.\n");
  BUG_ON(!vma->vm_private_data);
  
  down(&data_lock);
  mem = (struct block*)vma->vm_private_data;
  mem->refcount--;
  up(&data_lock);
}



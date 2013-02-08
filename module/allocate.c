#include "allocate.h"

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <asm/uaccess.h>

#include "common.h"

#define CACHED "cached"

struct block
{
  struct list_head list;
  struct resource* res;
};

static int list_count(struct list_head* list)
{
  int count = 0;
  struct list_head* pos;

  list_for_each(pos, list)
    count++;

  return count;
}

int physicalmemory_allocate(struct file* f, struct MemoryBlock* arg)
{
  unsigned long size = 0;
  struct resource *res = NULL;
  struct block *block = NULL;
  unsigned long physical_address = 0;
  int result = 0;
  struct file_data* fileData = f->private_data;

  BUG_ON(!fileData);
  if(!region)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: No memory to allocate from\n");
    return -ENOMEM;
  }
  if(copy_from_user(&size, &arg->size, sizeof(unsigned long)))
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Can't read size\n");
    return -EPERM;
  }
     
  printk(KERN_NOTICE PRINTK_PREFIX "Allocating buffer of size %lu\n", size);

  res = kzalloc(sizeof(*res), GFP_KERNEL);
  block = kzalloc(sizeof(*block), GFP_KERNEL);
  if(!res || !block)
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Failed to allocate memory for bookkeeping\n");
    result = -ENOMEM;
    goto fail;
  }
  res->name = CACHED;

  result = allocate_resource(region, res, size, 0, -1, 1, NULL, NULL);
  if(result < 0 )
  {
    printk(KERN_WARNING PRINTK_PREFIX "ERROR: Failed to allocate memory\n");
    result = -ENOMEM;
    goto fail;
  }

  physical_address = res->start;
  copy_to_user(&arg->physicalAddress, &physical_address, sizeof(unsigned long));

  INIT_LIST_HEAD(&block->list);
  block->res = res;

  write_lock(&data_lock);
  list_add(&block->list, &fileData->allocated);
  write_unlock(&data_lock);
  
  return 0;

 fail:
  if(res) kfree(res);
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
  return -EINVAL;
}

int physicalmemory_free_all(struct file* f)
{
  struct block* pos = NULL;
  struct block* temp = NULL;
  struct file_data* fileData = f->private_data;
  BUG_ON(!fileData);
  
  write_lock(&data_lock);
  printk(KERN_NOTICE PRINTK_PREFIX "Deallocate all %d buffers for process\n", list_count(&fileData->allocated));
  
  list_for_each_entry_safe(pos, temp, &fileData->allocated, list)
  {
    physicalmemory_free_block(pos);
  }
  BUG_ON(!list_empty(&fileData->allocated));
  write_unlock(&data_lock);
  
  return 0;
}

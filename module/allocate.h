#ifndef ALLOCATE_H
#define ALLOCATE_H

#include <linux/fs.h>

#include "physicalmemory_ioctl.h"

int physicalmemory_allocate(struct file* f, struct MemoryBlock* arg);
int physicalmemory_free(struct file* f, struct MemoryBlock* arg);
int physicalmemory_free_all(struct file* f);

int physicalmemory_remap_mmap(struct file *filp, struct vm_area_struct *vma);
void physicalmemory_vma_open(struct vm_area_struct *vma);
void physicalmemory_vma_close(struct vm_area_struct *vma);

#endif

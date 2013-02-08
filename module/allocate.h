#ifndef ALLOCATE_H
#define ALLOCATE_H

#include <linux/fs.h>

#include "physicalmemory_ioctl.h"

int physicalmemory_allocate(struct file* f, struct MemoryBlock* arg);
int physicalmemory_free(struct file* f, struct MemoryBlock* arg);
int physicalmemory_free_all(struct file* f);

#endif

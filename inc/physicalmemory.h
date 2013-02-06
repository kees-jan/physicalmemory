#ifndef PHYSICALMEMORY_H
#define PHYSICALMEMORY_H

#include <sys/mman.h>

void phys_init();
void *phys_mmap(void *addr, size_t length, int prot, int flags,
                  off_t offset);
int  phys_munmap(void *addr, size_t length);
void phys_flushCaches();


#endif

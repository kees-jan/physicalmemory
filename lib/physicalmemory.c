#include <physicalmemory.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#include <physicalmemory_ioctl.h>

static int fd=-1;

int phys_init()
{
  if(fd==-1)
  {
    fd=open("/dev/physicalmemory", O_RDWR);
    if(fd==-1)
    {
      perror("Open device failed");
      return -1;
    }
  }
  return 0;
}

static void init_block(struct physicalmemory_block* block)
{
  block->buffer = NULL;
  block->physicalAddress = 0;
  block->size = 0;
}

int phys_allocate(struct physicalmemory_block* block, size_t size)
{
  init_block(block);

  struct MemoryBlock b = MEMORYBLOCK_INITIALIZER;
  b.size = size;

  int result = ioctl(fd, PHYSICALMEMORY_IOCTL_ALLOCATE, &b);
  if(result>=0)
  {
    void* address=mmap(0, b.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, b.physicalAddress);
    if(address!=MAP_FAILED && address!=NULL)
    {
      // succes!
      block->buffer = address;
      block->physicalAddress = b.physicalAddress;
      block->size = b.size;
      return 0;
    }

    printf("physicalmemory: BUG: alloc succeeded, but mmap did not.\n");
    result = -1;
  }

  return result;
}

int phys_allocate_aligned(struct physicalmemory_block* block, size_t size, size_t alignment)
{
  init_block(block);

  struct MemoryBlock b = MEMORYBLOCK_INITIALIZER;
  b.size = size;
  b.alignment = alignment;

  int result = ioctl(fd, PHYSICALMEMORY_IOCTL_ALLOCATE, &b);
  if(result>=0)
  {
    void* address=mmap(0, b.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, b.physicalAddress);
    if(address!=MAP_FAILED && address!=NULL)
    {
      // succes!
      block->buffer = address;
      block->physicalAddress = b.physicalAddress;
      block->size = b.size;
      return 0;
    }

    printf("physicalmemory: BUG: alloc succeeded, but mmap did not\n");
    result = -1;
  }

  return result;
}

int phys_free(struct physicalmemory_block* block)
{
  int result=munmap(block->buffer, block->size);
  if(result>=0)
  {
    struct MemoryBlock b = MEMORYBLOCK_INITIALIZER;
    b.size = block->size;
    b.physicalAddress = block->physicalAddress;
    
    result = ioctl(fd, PHYSICALMEMORY_IOCTL_FREE, &b);

    // Free might fail if the process forked, and the child still has
    // the memory mapped. If this is the case, the memory will remain
    // allocated until both processes exit.
  }

  return result;
}

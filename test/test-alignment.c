#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#include <physicalmemory_ioctl.h>

const int PAGESIZE = 4096;

int main()
{
    int fd=-1;
    unsigned long len = PAGESIZE;

    fd=open("/dev/physicalmemory", O_RDWR);
    if(fd==-1)
    {
      perror("Open device failed");
    }

    struct MemoryBlock block1 = MEMORYBLOCK_INITIALIZER;
    block1.size = len;
    block1.physicalAddress = 0;

    int result = ioctl(fd, PHYSICALMEMORY_IOCTL_ALLOCATE, &block1);
    if(result<0)
    {
      perror("allocate failed");
      return -1;
    }
    printf("Got memory at physical address 0x%010lX\n", block1.physicalAddress);

    struct MemoryBlock block2 = MEMORYBLOCK_INITIALIZER;
    block2.size = len;
    block2.physicalAddress = 0;
    block2.alignment = 2*PAGESIZE;

    result = ioctl(fd, PHYSICALMEMORY_IOCTL_ALLOCATE, &block2);
    if(result<0)
    {
      perror("allocate failed");
      return -1;
    }
    printf("Got memory at physical address 0x%010lX\n", block2.physicalAddress);

    if(block2.physicalAddress & (block2.alignment-1))
    {
      printf("ERROR: Alignment not correct\n");
      return -1;
    }
    
    return 0;
}
        

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#include <physicalmemory_ioctl.h>

int main(int argc, char **argv)
{
    int fd=-1;
    unsigned long len;

    if (argc != 2
       || sscanf(argv[1],"%li", &len) != 1) {
        fprintf(stderr, "%s: Usage \"%s <len>\"\n", argv[0],
                argv[0]);
        return 1;
    }

    fd=open("/dev/physicalmemory", O_RDWR);
    if(fd==-1)
    {
      perror("Open device failed");
    }

    struct MemoryBlock block = MEMORYBLOCK_INITIALIZER;
    block.size = len;
    block.physicalAddress = 0;

    int result = ioctl(fd, PHYSICALMEMORY_IOCTL_ALLOCATE, &block);
    if(result<0)
    {
      perror("ioctl failed");
      return -1;
    }
    printf("Got memory at physical address 0x%010lX\n", block.physicalAddress);
    return 0;
}
        

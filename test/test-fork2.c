#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
      perror("allocate failed");
      return -1;
    }
    printf("Got memory at physical address 0x%010lX\n", block.physicalAddress);

    void* address=mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, block.physicalAddress);
    if(address==MAP_FAILED || address==NULL)
    {
      perror("mmap failed");
      return -1;
    }

    pid_t p = fork();
    if(p==-1)
    {
      perror("fork failed");
      return -1;
    }
    if(p==0)
    {
      // child
      sleep(1);
      exit(0);
    }
    
    result = munmap(address, len);
    if(result<0)
    {
      perror("munmap failed");
      return 1;
    }

    result = ioctl(fd, PHYSICALMEMORY_IOCTL_FREE, &block);
    if(result>=0)
    {
      printf("ERROR: Free succeeded even though child still has the mapping open\n");
      return -1;
    }

    int status=0;
    result = wait(&status);
    if(result==-1)
    {
      perror("wait failed");
    }

    result = ioctl(fd, PHYSICALMEMORY_IOCTL_FREE, &block);
    if(result<0)
    {
      perror("free failed");
      return -1;
    }
    printf("Freed memory at physical address 0x%010lX\n", block.physicalAddress); 

    return 0;
}
        

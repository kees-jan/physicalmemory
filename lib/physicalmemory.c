#include <physicalmemory.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

#include <physicalmemory_ioctl.h>

static int fd=-1;

void phys_init()
{
  if(fd==-1)
  {
    fd=open("/dev/physicalmemory", O_RDWR);
    if(fd==-1)
    {
      perror("Open device failed");
    }
  }
}

void *phys_mmap(void *addr, size_t length, int prot, int flags, off_t offset)
{
  if(fd==-1)
  {
    printf("ERROR: mmap: Device not open\n");
    return NULL;
  }

  return mmap(addr, length, prot, flags, fd, offset);
}

int  phys_munmap(void *addr, size_t length)
{
  return munmap(addr, length);
}

int phys_flushCaches()
{
  if(fd==-1)
  {
    printf("ERROR: flushCaches: Device not open\n");
    return -1;
  }
  else
  {
    int result = ioctl(fd, PHYSICALMEMORY_IOCTL_FLUSH_CACHE);
    if(result<0)
    {
      perror("ioctl failed");
      return -1;
    }
    return 0;
  }
}


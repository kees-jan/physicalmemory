#include <physicalmemory.h>

#include <stdio.h>

const int PAGESIZE = 4096;

int main()
{
  unsigned long len = PAGESIZE;

  int result = phys_init();
  if(result < 0)
  {
    perror("Init failed");
  }

  struct physicalmemory_block b1;
  result = phys_allocate(&b1, len);
  if(result<0)
  {
    perror("allocate failed");
    return -1;
  }

  struct physicalmemory_block b2;
  result = phys_allocate_aligned(&b2, 2*len, 2*len);
  if(result<0)
  {
    perror("second allocate failed");
    return -1;
  }

  if(b2.physicalAddress & (2*len-1))
  {
    printf("ERROR: Alignment not correct\n");
    return -1;
  }
  
  result = phys_free(&b1);
  if(result<0)
  {
    perror("free failed");
    return -1;
  }
    
  result = phys_free(&b2);
  if(result<0)
  {
    perror("second free failed");
    return -1;
  }
  return 0;
}
        

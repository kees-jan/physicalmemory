#include <physicalmemory.h>

#include <stdio.h>

int main(int argc, char **argv)
{
  unsigned long len;

  if (argc != 2
      || sscanf(argv[1],"%li", &len) != 1) {
    fprintf(stderr, "%s: Usage \"%s <len>\"\n", argv[0],
            argv[0]);
    return 1;
  }

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
  result = phys_allocate(&b2, len);
  if(result<0)
  {
    perror("second allocate failed");
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
        

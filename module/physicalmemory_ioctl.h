#ifndef PHYSICALMEMORY_IOCTL_H
#define PHYSICALMEMORY_IOCTL_H

#include <asm/ioctl.h>

#define PHYSICALMEMORY_IOCTL_TYPE                   'P'
#define PHYSICALMEMORY_IOCTL_FLUSH_CACHE            _IO(PHYSICALMEMORY_IOCTL_TYPE, 0)
#define PHYSICALMEMORY_IOCTL_COUNT                  1

#endif
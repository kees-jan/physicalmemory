#ifndef PHYSICALMEMORY_H
#define PHYSICALMEMORY_H

#include <sys/mman.h>
#include <stdint.h>

struct physicalmemory_block
{
  uint8_t* buffer;
  uint64_t physicalAddress;
  size_t size;
};

/**
 * Initialize physical memory subsystem
 *
 * @retval 0 on success
 * @retval -1 on error (@c errno will be set)
 */
int phys_init();

/**
 * Allocate a block of the requested size
 *
 * @param[out] block Information about the allocated block (only valid when allocation is succesful)
 * @param[in] size Requested blocksize
 * @retval 0 on success
 * @retval -1 on error (@c errno will be set)
 */
int phys_allocate(struct physicalmemory_block* block, size_t size);

/**
 * Allocate a block of the requested size and alignment
 *
 * @param[out] block Information about the allocated block (only valid when allocation is succesful)
 * @param[in] size Requested blocksize
 * @param[in] alignment Requested alignment
 * @retval 0 on success
 * @retval -1 on error (@c errno will be set)
 *
 * @post If allocation is successful, @c block->physicalAddress will be a multiple of @c alignment
 */
int phys_allocate_aligned(struct physicalmemory_block* block, size_t size, size_t alignment);

/**
 * Free the given block
 *
 * @param[in] block The block to be freed
 * @retval 0 on success
 * @retval -1 on error (@c errno will be set)
 *
 * @pre @c block must be previously allocated with one of the allocation functions
 */
int phys_free(struct physicalmemory_block* block);



#endif

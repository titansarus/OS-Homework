/*
 * mm_alloc.c
 * My implementation uses a linked list for handling each metadata block. At the
 * first call of mm_malloc, the head of the linked list gets initialized. Each
 * time in mm_malloc, I iterate over the linked list; if one of the previous
 * blocks has enough space for allocation, we call split_block. In split block,
 * I check whether It has enough space to be separated into two blocks or not.
 * If it can be separated, We will use the first part for actual malloc, and the
 * second part will be empty. If it has no space for another s_block, we will
 * use the size we need, and a little bit of space from the block will be in "no
 * man's land." If we don't find an appropriate free block in malloc, we extend
 * the heap and add a new node to the linked list. In mm_realloc, we check for
 * edge cases. If no edge case happened, we will allocate a new block, copy the
 * data from the previous block and free that block. In mm_free, we check for
 * edge cases, then set is_free=1 in metadata headers, and then call for fusion.
 * In "fusion," we check whether the previous or next block of the recently
 * freed block is free or not. If they are free, we will merge them with the
 * recently released block. Note: I didn't use s_block:ptr or s_block:data[0],
 * but I kept them for compatibility reasons to have the same BLOCK_SIZE as
 * specified in the mm_alloc.h.
 */

#include "mm_alloc.h"

#include <memory.h>
#include <unistd.h>

/* Returns the minimum of two size_t variables. Used in realloc. */
size_t MIN(size_t a, size_t b) {
  if (a < b) {
    return a;
  }
  return b;
}

/* head of linked list */
s_block_ptr head_ = NULL;

/* Initialize and allocate memory to the head of the linked list at the first
 * call of malloc, because at first, the data structure is not yet initialized.
 */
void *malloc_head_initializer(size_t size) {
  head_ = sbrk(0);
  if (sbrk(size + BLOCK_SIZE) == (void *)-1) {
    head_ = NULL;
    return NULL;
  }
  head_->prev = NULL;
  head_->next = NULL;
  head_->size = size;
  head_->is_free = 0;
  memset((void *)head_ + BLOCK_SIZE, 0, head_->size);
  return (void *)head_ + BLOCK_SIZE;  // returning the memory block itself which
                                      // can be used by the end user.
}

/* Retrieve the s_block structure that is assigned to a memory block in heap. */
s_block_ptr get_block(void *ptr) {
  if (ptr == NULL) {
    return NULL;
  }
  if (ptr < ((void *)head_ + BLOCK_SIZE) || ptr > sbrk(0)) {
    return NULL;
  }
  s_block_ptr tail = NULL;
  tail = (s_block_ptr)(ptr - (void *)BLOCK_SIZE);
  return tail;
}

/*
 * Split the block by size if it is possible. i.e., we split it into two parts.
 * The first part has size: `size` and is occupied. The second part is free and
 * has the remaining size. The second part should have at least the size of
 * s_block; otherwise, the second part will be of size zero, and actually, we
 * will send only the first part, and no second half will be generated. In
 * either case, the function will return the first half's allocated memory
 * pointer.
 */
void *split_block(s_block_ptr block, size_t size) {
  block->is_free = 0;
  // If having enough space for at least an s_block metadata struct, break the
  // block into two parts.
  if (block->size - size >= BLOCK_SIZE) {
    s_block_ptr next_block = block;
    next_block = ((void *)block + BLOCK_SIZE) + size;
    next_block->next = block->next;
    if (block->next != NULL) block->next->prev = next_block;
    block->next = next_block;
    next_block->prev = block;
    next_block->size = block->size - size - BLOCK_SIZE;
    next_block->is_free = 1;
  }
  block->size = size;
  memset(((void *)block + BLOCK_SIZE), 0, block->size);
  return ((void *)block + BLOCK_SIZE);
}

void *mm_malloc(size_t size) {
#ifdef MM_USE_STUBS
  return calloc(1, size);
#else
  // Exception Case
  if (!size) {
    return NULL;
  }
  if (head_ == NULL) {
    return malloc_head_initializer(size);
  }

  s_block_ptr curr = head_;
  s_block_ptr prev = curr;
  // Iterate on linked list.
  while (curr) {
    /* If the block has more size than needed, we will call split_block. It will
     * either not change it if it doesn't have enough space for another block or
     * break it into two parts.
     */
    if (curr->is_free && curr->size >= size) {
      return split_block(curr, size);
    }
    prev = curr;
    curr = curr->next;
  }
  return extend_heap(prev, size);
#endif
}

void *extend_heap(s_block_ptr last, size_t size) {
  void *heap_head = sbrk(0);
  size_t no_mans_land = (heap_head - ((void *)last + BLOCK_SIZE + last->size));

  if (sbrk(size + BLOCK_SIZE - no_mans_land) == (void *)-1) {
    return NULL;
  }

  s_block_ptr curr = ((void *)last + BLOCK_SIZE) + last->size;
  last->next = curr;
  curr->prev = last;
  curr->next = NULL;
  curr->is_free = 0;
  curr->size = size;
  memset((void *)curr + BLOCK_SIZE, 0, curr->size);
  return (void *)curr + BLOCK_SIZE;
}

void *mm_realloc(void *ptr, size_t size) {
#ifdef MM_USE_STUBS
  return realloc(ptr, size);
#else
  // Exception Case
  if (ptr == NULL) {
    if (size == 0) {
      goto NULL_RETURN;
    } else {
      return mm_malloc(size);
    }
  }
  // Exception Case
  if (size == 0) {
    mm_free(ptr);
    goto NULL_RETURN;
  }
  s_block_ptr current = get_block(ptr);
  void *allocatedMemory = mm_malloc(size);
  if (current == NULL || allocatedMemory == NULL) {
    goto NULL_RETURN;
  }
  if (current->is_free) {
    mm_free(allocatedMemory);
    goto NULL_RETURN;
  }
  // The min function is for cases when we realloc with smaller size than
  // original size.
  memcpy(allocatedMemory, ptr, MIN(current->size, size));
  mm_free(ptr);
  return allocatedMemory;

NULL_RETURN:
  return NULL;
#endif
}

void mm_free(void *ptr) {
#ifdef MM_USE_STUBS
  free(ptr);
#else
  if (ptr == NULL) {
    return;
  }
  s_block_ptr block = get_block(ptr);
  if (block == NULL) return;
  block->is_free = 1;
  fusion(block);
#endif
}

/* Connecting adjacent free blocks */
void fusion(s_block_ptr block) {
  s_block_ptr previous_block = block->prev;
  s_block_ptr next_block = block->next;
  // We are checking if the previous block is free to coalesce these two parts
  // together.
  if (previous_block && previous_block->is_free) {
    previous_block->next = block->next;
    if (next_block != NULL) {
      next_block->prev = previous_block;
    }
    previous_block->size = ((void *)block + BLOCK_SIZE) -
                           ((void *)previous_block + BLOCK_SIZE) + block->size;
    // We will also try to coalesce the next block if it is free.
    if (next_block != NULL && next_block->is_free) {
      previous_block->next = next_block->next;
      if (next_block->next != NULL) {
        next_block->next->prev = previous_block;
      }
      previous_block->size = ((void *)next_block + BLOCK_SIZE) -
                             ((void *)previous_block + BLOCK_SIZE) +
                             next_block->size;
    }
    return;

    // We are checking if the next block is free to coalesce these two parts
    // together.
  } else if (next_block && next_block->is_free) {
    block->is_free = 1;
    block->size = ((void *)next_block + BLOCK_SIZE) -
                  ((void *)block + BLOCK_SIZE) + next_block->size;
    block->next = next_block->next;
    next_block = next_block->next;
    if (next_block) {
      next_block->prev = block;
    }
    return;
  }
}
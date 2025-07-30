/*
 * CSE 351 Lab 5 (Dynamic Storage Allocator)
 * Extra credit: Implementing mm_realloc
 *
 * Name(s):  
 * NetID(s): 
 *
 * NOTES:
 *  - This requires mm_malloc and mm_free to be working correctly, so don't
 *    start on this until you finish mm.c.
 *  - Only the traces that start with 'realloc' actually call this function,
 *    so make sure you actually test your code on these traces!
 *  - This file does not need to be submitted if you did not attempt it.
 */
#include "mm.c"


/*
 * EXTRA CREDIT:
 * Change the size of the memory block pointed to by ptr to size bytes while
 * preserving the existing data in range. If a new block is allocated during
 * this process, make sure to free the old block.
 *  - if ptr is NULL, equivalent to malloc(size)
 *  - if size is 0, equivalent to free(size)
 */
void* mm_realloc(void* ptr, size_t size) {
  // Handle edge cases
  if (ptr == NULL) {
    return mm_malloc(size);
  }
  if (size == 0) {
    mm_free(ptr);
    return NULL;
  }

  // Get the current block information
  block_info* current_block = (block_info*) UNSCALED_POINTER_SUB(ptr, WORD_SIZE);
  size_t current_size = SIZE(current_block->size_and_tags);
  size_t current_payload_size = current_size - WORD_SIZE;
  
  // Calculate required size (including header)
  size_t req_size = size + WORD_SIZE;
  if (req_size <= MIN_BLOCK_SIZE) {
    req_size = MIN_BLOCK_SIZE;
  } else {
    req_size = ALIGNMENT * ((req_size + ALIGNMENT - 1) / ALIGNMENT);
  }

  if (req_size <= current_size && (current_size - req_size) < MIN_BLOCK_SIZE) {
    return ptr;
  }
  
  // If exact match, return ptr
  if (req_size == current_size) {
    return ptr;
  }

  if (req_size > current_size && (req_size - current_size) <= 2 * WORD_SIZE) {
    block_info* following_block = (block_info*) UNSCALED_POINTER_ADD(current_block, current_size);
    if ((char*)following_block < (char*)mem_heap_hi() && 
        (following_block->size_and_tags & TAG_USED) == 0 &&
        SIZE(following_block->size_and_tags) >= (req_size - current_size)) {
      // Quick expansion possible - do minimal work
      remove_free_block(following_block);
      size_t total_size = current_size + SIZE(following_block->size_and_tags);
      size_t preceding_block_use_tag = current_block->size_and_tags & TAG_PRECEDING_USED;
      current_block->size_and_tags = total_size | preceding_block_use_tag | TAG_USED;
      
      // Update next block's preceding used tag
      block_info* next_block = (block_info*) UNSCALED_POINTER_ADD(current_block, total_size);
      if ((char*)next_block < (char*)mem_heap_hi()) {
        next_block->size_and_tags = next_block->size_and_tags | TAG_PRECEDING_USED;
      }
      return ptr;
    }
  }
  
  // If shrinking significantly, split the block
  if (req_size < current_size) {
    // Split the block
    size_t preceding_block_use_tag = current_block->size_and_tags & TAG_PRECEDING_USED;
    
    // Update current block size
    current_block->size_and_tags = req_size | preceding_block_use_tag | TAG_USED;
    
    // Create new free block from the remainder
    block_info* new_free_block = (block_info*) UNSCALED_POINTER_ADD(current_block, req_size);
    size_t remaining_size = current_size - req_size;
    
    // Set up the new free block header and footer
    new_free_block->size_and_tags = remaining_size | TAG_PRECEDING_USED;
    size_t* footer = (size_t*) UNSCALED_POINTER_ADD(new_free_block, remaining_size - WORD_SIZE);
    *footer = remaining_size | TAG_PRECEDING_USED;
    
    // Insert the new free block and coalesce
    insert_free_block(new_free_block);
    coalesce_free_block(new_free_block);
    
    return ptr;
  }
  
  // If shrinking but not enough to split, just return ptr
  if (req_size < current_size) {
    return ptr;
  }
  
  // If expanding, try multiple strategies for in-place expansion
  if (req_size > current_size) {
    size_t needed_extra = req_size - current_size;
    
    block_info* following_block = (block_info*) UNSCALED_POINTER_ADD(current_block, current_size);
    if ((char*)following_block < (char*)mem_heap_hi() && 
        (following_block->size_and_tags & TAG_USED) == 0) {
      size_t following_size = SIZE(following_block->size_and_tags);
      
      if (following_size >= needed_extra) {
        // Remove following block from free list
        remove_free_block(following_block);
        
        size_t total_available = current_size + following_size;
        
        // If there's leftover space after expansion, create a new free block
        if (total_available - req_size >= MIN_BLOCK_SIZE) {
          // Update current block size
          size_t preceding_block_use_tag = current_block->size_and_tags & TAG_PRECEDING_USED;
          current_block->size_and_tags = req_size | preceding_block_use_tag | TAG_USED;
          
          // Create new free block from remainder
          block_info* new_free_block = (block_info*) UNSCALED_POINTER_ADD(current_block, req_size);
          size_t remaining_size = total_available - req_size;
          
          new_free_block->size_and_tags = remaining_size | TAG_PRECEDING_USED;
          size_t* footer = (size_t*) UNSCALED_POINTER_ADD(new_free_block, remaining_size - WORD_SIZE);
          *footer = remaining_size | TAG_PRECEDING_USED;
          
          // Insert and coalesce the new free block
          insert_free_block(new_free_block);
          coalesce_free_block(new_free_block);
        } else {
          // Use entire combined block
          size_t preceding_block_use_tag = current_block->size_and_tags & TAG_PRECEDING_USED;
          current_block->size_and_tags = total_available | preceding_block_use_tag | TAG_USED;
          
          // Update following block's preceding used tag
          block_info* next_block = (block_info*) UNSCALED_POINTER_ADD(current_block, total_available);
          if ((char*)next_block < (char*)mem_heap_hi()) {
            next_block->size_and_tags = next_block->size_and_tags | TAG_PRECEDING_USED;
          }
        }
        
        return ptr;
      }
    }

    if ((current_block->size_and_tags & TAG_PRECEDING_USED) == 0) {
      // Get preceding block footer to find its size
      size_t* preceding_footer = (size_t*) UNSCALED_POINTER_SUB(current_block, WORD_SIZE);
      size_t preceding_size = SIZE(*preceding_footer);
      block_info* preceding_block = (block_info*) UNSCALED_POINTER_SUB(current_block, preceding_size);
      
      size_t total_available = preceding_size + current_size;
      
      if (total_available >= req_size) {
        // Remove preceding block from free list
        remove_free_block(preceding_block);
        
        // Calculate new pointer (will be at start of preceding block)
        void* new_ptr = UNSCALED_POINTER_ADD(preceding_block, WORD_SIZE);
        
        size_t words_to_copy = current_payload_size / WORD_SIZE;
        size_t* src_words = (size_t*)ptr;
        size_t* dest_words = (size_t*)new_ptr;
        
        size_t i = 0;
        for (; i + 3 < words_to_copy; i += 4) {
          dest_words[i] = src_words[i];
          dest_words[i+1] = src_words[i+1];
          dest_words[i+2] = src_words[i+2];
          dest_words[i+3] = src_words[i+3];
        }
        for (; i < words_to_copy; i++) {
          dest_words[i] = src_words[i];
        }
        
        // Copy remaining bytes if payload size not word-aligned
        size_t remaining_bytes = current_payload_size % WORD_SIZE;
        if (remaining_bytes > 0) {
          char* src_bytes = (char*)ptr + (words_to_copy * WORD_SIZE);
          char* dest_bytes = (char*)new_ptr + (words_to_copy * WORD_SIZE);
          for (size_t j = 0; j < remaining_bytes; j++) {
            dest_bytes[j] = src_bytes[j];
          }
        }
        
        // Set up the new block
        size_t preceding_block_use_tag = preceding_block->size_and_tags & TAG_PRECEDING_USED;
        
        if (total_available - req_size >= MIN_BLOCK_SIZE) {
          // Split: use req_size, create free block from remainder
          preceding_block->size_and_tags = req_size | preceding_block_use_tag | TAG_USED;
          
          block_info* new_free_block = (block_info*) UNSCALED_POINTER_ADD(preceding_block, req_size);
          size_t remaining_size = total_available - req_size;
          
          new_free_block->size_and_tags = remaining_size | TAG_PRECEDING_USED;
          size_t* footer = (size_t*) UNSCALED_POINTER_ADD(new_free_block, remaining_size - WORD_SIZE);
          *footer = remaining_size | TAG_PRECEDING_USED;
          
          insert_free_block(new_free_block);
          coalesce_free_block(new_free_block);
        } else {
          // Use entire combined block
          preceding_block->size_and_tags = total_available | preceding_block_use_tag | TAG_USED;
          
          // Update following block's preceding used tag
          block_info* next_block = (block_info*) UNSCALED_POINTER_ADD(preceding_block, total_available);
          if ((char*)next_block < (char*)mem_heap_hi()) {
            next_block->size_and_tags = next_block->size_and_tags | TAG_PRECEDING_USED;
          }
        }
        
        return new_ptr;
      }
    }
    
    if ((current_block->size_and_tags & TAG_PRECEDING_USED) == 0) {
      size_t* preceding_footer = (size_t*) UNSCALED_POINTER_SUB(current_block, WORD_SIZE);
      size_t preceding_size = SIZE(*preceding_footer);
      block_info* preceding_block = (block_info*) UNSCALED_POINTER_SUB(current_block, preceding_size);
      
      if ((following_block->size_and_tags & TAG_USED) == 0) {
        size_t following_size = SIZE(following_block->size_and_tags);
        size_t total_available = preceding_size + current_size + following_size;
        
        if (total_available >= req_size) {
          // Remove both blocks from free lists
          remove_free_block(preceding_block);
          remove_free_block(following_block);
          
          // Calculate new pointer
          void* new_ptr = UNSCALED_POINTER_ADD(preceding_block, WORD_SIZE);

          size_t words_to_copy = current_payload_size / WORD_SIZE;
          size_t* src_words = (size_t*)ptr;
          size_t* dest_words = (size_t*)new_ptr;
          
          size_t i = 0;
          for (; i + 3 < words_to_copy; i += 4) {
            dest_words[i] = src_words[i];
            dest_words[i+1] = src_words[i+1];
            dest_words[i+2] = src_words[i+2];
            dest_words[i+3] = src_words[i+3];
          }
          for (; i < words_to_copy; i++) {
            dest_words[i] = src_words[i];
          }
          
          // Copy remaining bytes
          size_t remaining_bytes = current_payload_size % WORD_SIZE;
          if (remaining_bytes > 0) {
            char* src_bytes = (char*)ptr + (words_to_copy * WORD_SIZE);
            char* dest_bytes = (char*)new_ptr + (words_to_copy * WORD_SIZE);
            for (size_t j = 0; j < remaining_bytes; j++) {
              dest_bytes[j] = src_bytes[j];
            }
          }
          
          // Set up the new block
          size_t preceding_block_use_tag = preceding_block->size_and_tags & TAG_PRECEDING_USED;
          
          if (total_available - req_size >= MIN_BLOCK_SIZE) {
            preceding_block->size_and_tags = req_size | preceding_block_use_tag | TAG_USED;
            
            block_info* new_free_block = (block_info*) UNSCALED_POINTER_ADD(preceding_block, req_size);
            size_t remaining_size = total_available - req_size;
            
            new_free_block->size_and_tags = remaining_size | TAG_PRECEDING_USED;
            size_t* footer = (size_t*) UNSCALED_POINTER_ADD(new_free_block, remaining_size - WORD_SIZE);
            *footer = remaining_size | TAG_PRECEDING_USED;
            
            insert_free_block(new_free_block);
            coalesce_free_block(new_free_block);
          } else {
            preceding_block->size_and_tags = total_available | preceding_block_use_tag | TAG_USED;
            
            block_info* next_block = (block_info*) UNSCALED_POINTER_ADD(preceding_block, total_available);
            if ((char*)next_block < (char*)mem_heap_hi()) {
              next_block->size_and_tags = next_block->size_and_tags | TAG_PRECEDING_USED;
            }
          }
          
          return new_ptr;
        }
      }
    }
  }
  
  // If we can't resize in place, allocate new block, copy data, and free old
  void* new_ptr = mm_malloc(size);
  if (new_ptr == NULL) {
    return NULL;
  }
  
  size_t copy_size = (current_payload_size < size) ? current_payload_size : size;
  

  size_t* src_words = (size_t*)ptr;
  size_t* dest_words = (size_t*)new_ptr;
  size_t words_to_copy = copy_size / WORD_SIZE;
  
  size_t i = 0;
  for (; i + 3 < words_to_copy; i += 4) {
    dest_words[i] = src_words[i];
    dest_words[i+1] = src_words[i+1];
    dest_words[i+2] = src_words[i+2];
    dest_words[i+3] = src_words[i+3];
  }
  
  // Handle remaining words
  for (; i < words_to_copy; i++) {
    dest_words[i] = src_words[i];
  }
  
  // Copy remaining bytes if not word-aligned
  size_t remaining_bytes = copy_size % WORD_SIZE;
  if (remaining_bytes > 0) {
    char* src_bytes = (char*)ptr + (words_to_copy * WORD_SIZE);
    char* dest_bytes = (char*)new_ptr + (words_to_copy * WORD_SIZE);
    for (size_t j = 0; j < remaining_bytes; j++) {
      dest_bytes[j] = src_bytes[j];
    }
  }
  
  // Free the old block
  mm_free(ptr);
  
  return new_ptr;
}

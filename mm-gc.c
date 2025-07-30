/*
 * CSE 351 Lab 5 (Dynamic Storage Allocator)
 * Extra credit: Implementing garbage collection
 *
 * Name(s):  
 * NetID(s): 
 *
 * NOTES:
 *  - This requires mm_malloc and mm_free to be working correctly, so don't
 *    start on this until you finish mm.c.
 *  - This file does not need to be submitted if you did not attempt it.
 */
#include "mm.c"


// The tag to indicate that a block is marked.
#define TAG_MARKED 4

// Forward function declarations
static void sweep(void);
static int is_pointer(void* ptr);
static void mark(void* ptr);


/* A modified version of examine_heap() to include TAG_MARKED. */
static void examine_heap_gc() {
  block_info* block;

  fprintf(stderr, "FREE_LIST_HEAD: %p\n", (void*) FREE_LIST_HEAD);

  for (block = (block_info*) UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE);  // first block on heap
       SIZE(block->size_and_tags) != 0 && (void*) block < mem_heap_hi();
       block = (block_info*) UNSCALED_POINTER_ADD(block, SIZE(block->size_and_tags))) {

    fprintf(stderr, "%p: %ld %ld %ld %ld\t",
            (void*) block,
            SIZE(block->size_and_tags),
            block->size_and_tags & TAG_MARKED,
            block->size_and_tags & TAG_PRECEDING_USED,
            block->size_and_tags & TAG_USED);

    // and allocated/free specific data
    if (block->size_and_tags & TAG_USED) {
      fprintf(stderr, "ALLOCATED\n");
    } else {
      fprintf(stderr, "FREE\tnext: %p, prev: %p\n",
              (void*) block->next,
              (void*) block->prev);
    }
  }
  fprintf(stderr, "END OF HEAP\n\n");
}


/*
 * This will determine if the given pointer points to the beginning
 * of the payload of a block which is allocated.
 */
static int is_pointer(void* ptr) {
  size_t* cur_block = (size_t*) UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE);
  size_t* heap_footer = (size_t*) UNSCALED_POINTER_SUB(mem_heap_hi(), WORD_SIZE - 1);

  while (cur_block < heap_footer) {
    size_t size_and_tags = *cur_block;
    void* payload_ptr = UNSCALED_POINTER_ADD(cur_block, WORD_SIZE);
    size_t allocated = size_and_tags & TAG_USED;

    if (payload_ptr == ptr && allocated) {
      return 1;
    }
    cur_block = (size_t*) UNSCALED_POINTER_ADD(cur_block, SIZE(size_and_tags));
  }
  return 0;
}


/*
 * Mark the block pointed to by ptr. The argument ptr will point to the
 * beginning of the payload of the block. Use the tag TAG_MARKED to signify
 * that a block is reachable. Next go and mark all other blocks that are
 * pointed to by pointers in this block.
 */
static void mark(void* ptr) {
  if (ptr == NULL) {
    return;
  }
  
  // Check if this is a valid pointer to the start of an allocated block
  if (!is_pointer(ptr)) {
    return;
  }
  
  size_t* block_header = (size_t*) UNSCALED_POINTER_SUB(ptr, WORD_SIZE);
  
  // If already marked, avoid infinite recursion
  if (*block_header & TAG_MARKED) {
    return;
  }
  
  // Mark this block
  *block_header |= TAG_MARKED;
  
  // Get the size of this block's payload
  size_t block_size = SIZE(*block_header);
  size_t payload_size = block_size - WORD_SIZE;
  
  // Scan through the payload looking for pointers
  // Pointers are word-aligned, so iterate by WORD_SIZE
  void** payload_start = (void**)ptr;
  size_t num_words = payload_size / WORD_SIZE;
  
  for (size_t i = 0; i < num_words; i++) {
    void* potential_ptr = payload_start[i];
    
    // Recursively mark any blocks pointed to by this block
    mark(potential_ptr);
  }
}


/*
 * Sweep through the all of the allocated blocks in the heap and free all
 * that are unreachable (i.e., TAG_MARKED is unset).
 */
static void sweep() {
  size_t* cur_block = (size_t*) UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE);
  size_t* heap_footer = (size_t*) UNSCALED_POINTER_SUB(mem_heap_hi(), WORD_SIZE - 1);

  while (cur_block < heap_footer) {
    size_t size_and_tags = *cur_block;
    size_t block_size = SIZE(size_and_tags);
    
    // If this is the end of heap marker, break
    if (block_size == 0) {
      break;
    }
    
    // Check if this block is allocated
    if (size_and_tags & TAG_USED) {
      // If allocated but not marked, it's garbage - free it
      if (!(size_and_tags & TAG_MARKED)) {
        void* payload = UNSCALED_POINTER_ADD(cur_block, WORD_SIZE);
        mm_free(payload);
      } else {
        // If marked, clear the mark bit for next collection
        *cur_block &= ~TAG_MARKED;
      }
    }
    
    // Move to next block
    cur_block = (size_t*) UNSCALED_POINTER_ADD(cur_block, block_size);
  }
}


/* Run the mark-and-sweep garbage collection algorithm. */
void mm_garbage_collect(void* rootPtrs[], int num_roots) {
  int i;
  for (i = 0; i < num_roots; i++) {
    void* root = rootPtrs[i];
    mark(root);
  }
  sweep();
}

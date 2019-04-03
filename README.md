# Thread-Safe-Malloc

- Thread safe version is in sub-repository

Hint: A common way to implement malloc() / free() and 
      manage the memory space is to keep a data structure that 
      represents list of free memory regions. 
      This collection of free memory ranges would change as malloc() and free() 
      are called to allocate and release regions of memory in the process data segment.
      
Implement 2 versions of malloc and free, 
each based on a different strategy for determining the memory region to allocate. 
The two strategies are:

1. First Fit: Examine the free space tracker (e.g. free list), 
   and allocate an address from the first free region with enough space to fit the requested allocation size.
2. Best Fit: Examine all of the free space information, 
   and allocate an address from the free region which has 
   the smallest number of bytes greater than or equal to the requested allocation size.
   
     //First Fit malloc/free
     void *ff_malloc(size_t size);
     void ff_free(void *ptr);
     //Best Fit malloc/free
     void *bf_malloc(size_t size);
     void bf_free(void *ptr);
     
Coalesce in this situation by merging the adjacent free regions into a single free region of memory.

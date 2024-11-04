#ifndef SIM_CACHE_H
#define SIM_CACHE_H

#include <inttypes.h>

// Nothing much to yap about here :(

// Cache
typedef 
struct {
   uint32_t BLOCKSIZE;
   uint32_t L1_SIZE;
   uint32_t L1_ASSOC;
   uint32_t L2_SIZE;
   uint32_t L2_ASSOC;
   uint32_t PREF_N;
   uint32_t PREF_M;
} cache_params_t;

// Cache Block
typedef struct {
   bool validBit;
   bool dirtyBit;
   uint32_t lru_counter;
   uint32_t tag;
   uint32_t block_data; // Data value of the block (kind of useless)
} Cache_Block;

// Cache Set
typedef struct {
   Cache_Block *blocks; // Pointer to accesss the array of blocks in the set
} Cache_Set;

// Prefetch Block
typedef struct {
   uint32_t buffer_block; // Options to expand prefetch in future
} Prefetch_Block;

// Prefetch
typedef struct {
   bool valid_prefetch;
   Prefetch_Block *prefetch_blocks; // Pointer to access the array of blocks in the buffer
   uint32_t buffer_lru;
} Prefetch_Buffer;

#endif

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include "sim.h"

#define WORDWIDTH 32    // Data width

using namespace std;

// Yapping time

// Generic Cache class 
class Cache {
private:
    // User-defined parameters
    uint32_t cacheSize;                     // Total bytes of data storage  
    uint32_t assoc;                         // Associativity of the cache   
    uint32_t blockSize;                     // Number of bytes in a block   
    uint32_t streamBuffers;                 // Number of stream buffers
    uint32_t streamMemoryBlocks;            // Stream memory size

    // Derived parameters
    uint32_t numBlocks;                     // Number of blocks in a set
    uint32_t numSets;                       // Number of sets
    uint32_t blockOffsetBits;               // Number of offset bits
    uint32_t indexBits;                     // Number of index bits
    uint32_t tagBits;                       // Number of tag bits

    // Access parameters
    char rw = '\0';                         // Flag to track if read or write
    uint32_t addr = 0;                      // Address

    // Sets
    Cache_Set* mySet = NULL;               // Pointer to dynamically allocate sets

    // Prefetch Buffers
    Prefetch_Buffer* mybuffer = NULL;      // Pointer to dynamically allocate buffers
    

public:
    // Cache hierarchy parameters
    Cache* nextCache;                       // Pointer to track next level cache (NULL is next level not present)

    // Prefetch Buffers
    bool buffer_active;                     // Flag to track if buffer is active or not

    // Performance metrics
    uint32_t reads;                         // Number of reads
    uint32_t read_misses;                   // Number of read misses
    uint32_t writes;                        // Number of writes
    uint32_t write_misses;                  // Number of write misses
    float    miss_rate;                     // Miss Rate         
    uint32_t writebacks;                    // Number of writebacks
    uint32_t prefetches;                    // Number of prefetches requests from this cache
    uint32_t read_prefetch;                 // Number of L2 reads that originated from L1 prefetches
    uint32_t read_prefetch_misses;          // Number of L2 reads that originated from L1 prefetches
    uint32_t mem_traffic;                   // Number of main mem accesses

    // Cache Methods
    Cache(uint32_t cacheSize, uint32_t assoc, uint32_t blockSize, uint32_t streamBuffers, uint32_t streamMemoryBlocks, Cache* nextCache);
    ~Cache();
    void init_cache();
    void cache_read(uint32_t addr);
    void cache_write(uint32_t addr);
    void get_bits(uint32_t bits[], uint32_t addr);
    void update_lru(uint32_t index, uint32_t accessed_block_index);
    uint32_t find_replacement_lru(uint32_t index);
    void miss_rate_calc();
    void print_block_contents();
    void print_perf_params();

    // Buffer Methods
    void new_prefetch(uint32_t addr);
    void sync_prefetch(uint32_t buffer_block, uint32_t buffer_index, uint32_t buffer_shift_index);
    void continue_prefetch(uint32_t buffer_block, uint32_t buffer_index, uint32_t addr);
    bool prefetch_request(uint32_t addr);
    void update_buffer_lru(uint32_t buffer_index);
    uint32_t find_replacement_buffer_lru();
    void print_buffer();
};

// Constructor (The Man, the Myth, the Legend)
Cache::Cache(uint32_t cacheSize, uint32_t assoc, uint32_t blockSize, uint32_t streamBuffers, uint32_t streamMemoryBlocks, Cache* nextCache)
    : cacheSize(cacheSize), assoc(assoc), blockSize(blockSize), streamBuffers(streamBuffers), streamMemoryBlocks(streamMemoryBlocks), nextCache(nextCache)
{
    init_cache();
}

// Initialization function to set context
void Cache::init_cache() {
    
    // If cache not initialized
    if(cacheSize == 0) {
        numBlocks       = 0;
        numSets         = 0;
        blockOffsetBits = 0;
        indexBits       = 0;
        tagBits         = 0;
        miss_rate       = 0;

        reads           = 0;
        read_misses     = 0;
        writes          = 0;
        write_misses    = 0;
        writebacks      = 0;
        miss_rate       = 0;

        buffer_active   = false;
        prefetches      = 0;
        read_prefetch   = 0;
        read_prefetch_misses = 0;
        mem_traffic     = 0;
    }
    // If initialized
    else {
        numBlocks = cacheSize / blockSize;
        numSets = cacheSize / (assoc * blockSize);
        blockOffsetBits = log2(blockSize);
        indexBits = log2(numSets);
        tagBits = WORDWIDTH - blockOffsetBits - indexBits;
        miss_rate = 0;

        reads           = 0;
        read_misses     = 0;
        writes          = 0;
        write_misses    = 0;
        writebacks      = 0;
        miss_rate       = 0;

        buffer_active   = false;
        prefetches      = 0;
        read_prefetch   = 0;
        read_prefetch_misses = 0;
        mem_traffic     = 0;

        // Cache ,emory allocation
        mySet = new Cache_Set[numSets];
        for (uint32_t i = 0; i < numSets; i++) {
            mySet[i].blocks = new Cache_Block[assoc];
            for (uint32_t j = 0; j < assoc; j++) {
                mySet[i].blocks[j] = { false, false, j, 0, 0 }; // Initialize block properties
            }
        }

        // If Prefetch is active and there is no next level cache (directly main memory)
        if ((streamBuffers > 0) && (nextCache == NULL)) {
            buffer_active = true;

            // Prefetch buffer memory allocation
            mybuffer = new Prefetch_Buffer[streamBuffers];
            for (uint32_t i = 0; i < streamBuffers; i++) {
                mybuffer[i].prefetch_blocks = new Prefetch_Block[streamMemoryBlocks];
                mybuffer[i].valid_prefetch = false;
                mybuffer[i].buffer_lru = i;
                for (uint32_t j = 0; j < streamMemoryBlocks; j++) {
                    mybuffer[i].prefetch_blocks[j].buffer_block = 0; // Initialize block
                }
            }
        }
    }
}

// Cache read function; handles reads (dumb comment lol)
void Cache::cache_read(uint32_t addr) {
    uint32_t bits[3];
    // bits[0] = block offset
    // bits[1] = set index
    // bits[2] = tag
    bool bufferHit = false;

    reads++;
    get_bits(bits, addr);

    // Search the buffer for block
    if (buffer_active) {
        bufferHit = prefetch_request(addr);
    }

    // Search the set for tag
    for (uint32_t i = 0; i < assoc; i++) {
        if (mySet[bits[1]].blocks[i].validBit && (bits[2] == mySet[bits[1]].blocks[i].tag)) {
            // Read Hit :)
            update_lru(bits[1], i);
            return;
        }
    }

    // Read Miss :(
    if (buffer_active) {
        if(!bufferHit) {
            read_misses++;
            new_prefetch(addr);
        }
    }
    else {
        read_misses++;
    }
    
    // Replement block index
    uint32_t victim_index = find_replacement_lru(bits[1]);

    // If LRU block is clean
    if(!mySet[bits[1]].blocks[victim_index].dirtyBit) {
        // If prefetch is not active
        if (!buffer_active) {
            // If next level exists
            if(nextCache != NULL) {
                nextCache->cache_read(addr);
            }
            // If only L1 exists
            else {
                mem_traffic++;
            }
        }
        // If prefetch is active
        else {
            // If prefetch miss
            if(!bufferHit) {
                mem_traffic++;
            }
        }

        // Update LRU
        update_lru(bits[1], victim_index);

        // Update replaced block
        mySet[bits[1]].blocks[victim_index].validBit = true;
        mySet[bits[1]].blocks[victim_index].tag = bits[2];
        mySet[bits[1]].blocks[victim_index].block_data++;

        return;
    }

    // If LRU block is dirty
    else {
        writebacks++;
        // If prefetch is not active
        if (!buffer_active) {
            // If next level exists
            if(nextCache != NULL) {
                uint32_t dirty_addr = (mySet[bits[1]].blocks[victim_index].tag << (indexBits + blockOffsetBits)) + (bits[1] << blockOffsetBits);
                nextCache->cache_write(dirty_addr);
                nextCache->cache_read(addr);
            }
            else{
                mem_traffic++;          // write to memory (writeback)
                mem_traffic++;          // read from memory (fetch new data)
            }
        }
        // If prefetch is active
        else {
            mem_traffic++;
            // If prefetch miss
            if(!bufferHit) {
                mem_traffic++;
            }
        }

        // Update LRU
        update_lru(bits[1], victim_index);

        // Update replaced block
        mySet[bits[1]].blocks[victim_index].validBit = true;
        mySet[bits[1]].blocks[victim_index].dirtyBit = false;
        mySet[bits[1]].blocks[victim_index].tag = bits[2];
        mySet[bits[1]].blocks[victim_index].block_data++;

        return;
    }    
}

// Cache write function; handles writes (another dumb comment lol)
void Cache::cache_write(uint32_t addr) {
    uint32_t bits[3];
    // bits[0] = block offset
    // bits[1] = set index
    // bits[2] = tag
    bool bufferHit = false;

    writes++;
    get_bits(bits, addr);

    // Search the buffer for block
    if (buffer_active) {
        bufferHit = prefetch_request(addr);
    }

    // Search the set for tag
    for (uint32_t i = 0; i < assoc; i++) {
        if (mySet[bits[1]].blocks[i].validBit && (bits[2] == mySet[bits[1]].blocks[i].tag)) {
            // Write Hit :)
            mySet[bits[1]].blocks[i].dirtyBit = true; // Set dirty bit on write
            mySet[bits[1]].blocks[i].block_data++;
            update_lru(bits[1], i);
            return;
        }
    }

    // Write Miss :(
    if (buffer_active) {
        if(!bufferHit) {
            write_misses++;
            new_prefetch(addr);
        }
    }
    else {
        write_misses++;
    }
    
    // Replement block index
    uint32_t victim_index = find_replacement_lru(bits[1]);

    // If LRU block is clean
    if(!mySet[bits[1]].blocks[victim_index].dirtyBit) {
        // If prefetch is not active
        if (!buffer_active) {
            // If next level exists
            if(nextCache != NULL) {
                nextCache->cache_read(addr);
            }
            // If only L1 exists
            else {
                mem_traffic++;
            }
        }
        // If prefetch is active
        else {
            // If prefetch miss
            if(!bufferHit) {
                mem_traffic++;
            }
        }

        // Update LRU
        update_lru(bits[1], victim_index);

        // Update replaced block
        mySet[bits[1]].blocks[victim_index].validBit = true;
        mySet[bits[1]].blocks[victim_index].dirtyBit = true;
        mySet[bits[1]].blocks[victim_index].tag = bits[2];
        mySet[bits[1]].blocks[victim_index].block_data++;
        return;
    }

    // If LRU block is dirty
    else {
        writebacks++;
        // If prefetch is not active
        if (!buffer_active) {
            if(nextCache != NULL) {
                uint32_t dirty_addr = (mySet[bits[1]].blocks[victim_index].tag << (indexBits + blockOffsetBits)) + (bits[1] << blockOffsetBits);
                nextCache->cache_write(dirty_addr);
                nextCache->cache_read(addr);
            }
            else{
                mem_traffic++;          // write to memory (writeback)
                mem_traffic++;          // read from memory (fetch new data)
            }
        }
        // If prefetch is active
        else {
            mem_traffic++;
            // If prefetch miss
            if(!bufferHit) {
                mem_traffic++;
            }
        }
        // Update LRU
        update_lru(bits[1], victim_index);

        // Update replaced block
        mySet[bits[1]].blocks[victim_index].validBit = true;
        mySet[bits[1]].blocks[victim_index].validBit = true;
        mySet[bits[1]].blocks[victim_index].tag = bits[2];
        mySet[bits[1]].blocks[victim_index].block_data++;
        return;
    }
}

// *bits[] overflow might cause a issue, to fix later if I get time* -> unfixed :( no time
// Applies masks and gets the values of Block Offset, Set index and Tag
// Takes an array, and sets it as follows
// bits[0] = Block Offset
// bits[1] = Set Index
// bits[2] = Tag
void Cache::get_bits(uint32_t bits[], uint32_t addr) {
    // Block Offset
    bits[0] = (addr & (numBlocks - 1));
    // Set Index
    bits[1] = ((addr >> blockOffsetBits) & (numSets - 1));
    // Tag
    bits[2] = (addr >> (blockOffsetBits + indexBits));
}

// Updates the LRU (another dumb comment)
// Increments the LRU counter of all blocks with current LRU counter less than accessed block
void Cache::update_lru(uint32_t index, uint32_t accessed_block_index) {
    for (uint32_t j = 0; j < assoc; j++) {
        if ((mySet[index].blocks[j].lru_counter < mySet[index].blocks[accessed_block_index].lru_counter)) {
            mySet[index].blocks[j].lru_counter++;
        }
    }
    mySet[index].blocks[accessed_block_index].lru_counter = 0; // Reset for the recently accessed block
}

// Returns index of block to be evicted
// First checks for invalid block, if all valid then returns MRU
uint32_t Cache::find_replacement_lru(uint32_t index) {
    uint32_t max_lru = 0;
    uint32_t replacementIndex = 0;

    for (uint32_t j = 0; j < assoc; j++) {
        if (!mySet[index].blocks[j].validBit) {
            replacementIndex = j;
        }
    }

    for (uint32_t j = 0; j < assoc; j++) {
        if (mySet[index].blocks[j].lru_counter > max_lru) {
            max_lru = mySet[index].blocks[j].lru_counter;
            replacementIndex = j;
        }
    }

    return replacementIndex;
}

// Function for a fresh prefetch
// Takes an address, extracts block info, and prefetches from block+1 to buffer size
void Cache::new_prefetch(uint32_t addr) {
    uint32_t buffer_index = find_replacement_buffer_lru();

    uint32_t block_to_replace = (addr >> blockOffsetBits) + 1; // Start prefetching from next block

    // Loop and fill next memory blocks
    for (uint32_t j = 0; j < streamMemoryBlocks; j++) {
        mybuffer[buffer_index].prefetch_blocks[j].buffer_block = block_to_replace;
        block_to_replace++;
    }

    mybuffer[buffer_index].valid_prefetch = true;
    update_buffer_lru(buffer_index);

    // Update parameters
    prefetches += streamMemoryBlocks;
    mem_traffic += streamMemoryBlocks;
}

// Simulation shortcut function (*wink wink*)
// Skips the shifting part, directly load fresh buffer and increment prefetch by only supposed "new fetches"
void Cache::sync_prefetch(uint32_t buffer_block, uint32_t buffer_index, uint32_t buffer_shift_index) {
    prefetches += buffer_shift_index;
    mem_traffic += buffer_shift_index;

    uint32_t shift_block = buffer_block;

    for (uint32_t i = 0; i < streamMemoryBlocks; i++) {
        mybuffer[buffer_index].prefetch_blocks[i].buffer_block = shift_block;
        shift_block++;
    }
}

// Proper simulation; shifts and increments
void Cache::continue_prefetch(uint32_t buffer_block, uint32_t buffer_index, uint32_t buffer_shift_index) {
    uint32_t shift_block = buffer_block;
    uint32_t new_index = 0;

    prefetches += buffer_shift_index;
    mem_traffic += buffer_shift_index;

    // Shift blocks after "hit" block
    for (uint32_t i = 0; i < (streamMemoryBlocks - buffer_shift_index); i++) {
        mybuffer[buffer_index].prefetch_blocks[i].buffer_block = mybuffer[buffer_index].prefetch_blocks[buffer_shift_index].buffer_block;
        buffer_shift_index++;
        shift_block++;
        new_index = i;
    }

    // Fetch new blocks to fill in shifted spots
    for (uint32_t i = new_index+1; i < streamMemoryBlocks; i++) {
        mybuffer[buffer_index].prefetch_blocks[i].buffer_block = shift_block;
        shift_block++;
    }
}

// Main prefetch request function
// Sorts the buffers from MRU -> LRU
// Starting from the MRU buffer, searches the given block
// If hit, update LRU and sync the buffer
// If miss... well it's a miss
bool Cache::prefetch_request(uint32_t addr) {
    uint32_t block_addr = addr >> blockOffsetBits;

    // Create a vector to pair LRU values with buffer indices
    vector<pair<uint32_t, uint32_t>> lru_list;
    for (uint32_t i = 0; i < streamBuffers; i++) {
        if (mybuffer[i].valid_prefetch) {
            lru_list.push_back({mybuffer[i].buffer_lru, i});
        }
    }

    // Sort the vector by LRU in ascending order (most-recently-used first)
    sort(lru_list.begin(), lru_list.end(), [](const auto &a, const auto &b) {
        return a.first < b.first; // Sort based on LRU value
    });

    // Now search in the sorted order
    for (const auto& buffer : lru_list) {
        uint32_t i = buffer.second; // Get the buffer index
        for (uint32_t j = 0; j < streamMemoryBlocks; j++) {
            if (mybuffer[i].prefetch_blocks[j].buffer_block == block_addr) {
                // Prefetch hit
                update_buffer_lru(i);
                sync_prefetch(block_addr + 1, i, j + 1);
                return true;
            }
        }
    }

    return false;
}

// Updates the LRU, but for buffers
void Cache::update_buffer_lru(uint32_t buffer_index) {
    for (uint32_t j = 0; j < streamBuffers; j++) {
        if (mybuffer[j].buffer_lru <  mybuffer[buffer_index].buffer_lru) {
            mybuffer[j].buffer_lru++;
        }
    }
    mybuffer[buffer_index].buffer_lru = 0;
}

// Returns index of block to be evicted, but for buffers
uint32_t Cache::find_replacement_buffer_lru() {
    uint32_t max_lru = 0;
    uint32_t replacementIndex = 0;

    for (uint32_t j = 0; j < streamBuffers; j++) {
        if (mybuffer[j].buffer_lru > max_lru) {
            max_lru = mybuffer[j].buffer_lru;
            replacementIndex = j;
        }
    }

    return replacementIndex;
}

// Prints buffer contents (wow)
void Cache::print_buffer() {
    cout << "===== Stream Buffer(s) contents =====" << endl;

    // Create a vector to hold the LRU and buffer index
    vector<pair<uint32_t, Prefetch_Buffer*>> buffer_list;

    // Collect the LRU and corresponding buffer
    for (uint32_t i = 0; i < streamBuffers; i++) {
        buffer_list.push_back({mybuffer[i].buffer_lru, &mybuffer[i]});
    }

    // Sort the buffers based on LRU in ascending order (most-recently-used first)
    sort(buffer_list.begin(), buffer_list.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    // Print the sorted buffers
    for (const auto& buffer_pair : buffer_list) {
        Prefetch_Buffer* buffer = buffer_pair.second;
        for (uint32_t j = 0; j < streamMemoryBlocks; j++) {
            cout << hex << buffer->prefetch_blocks[j].buffer_block << "  ";
        }
        cout << endl;
    }
    cout << endl;
}

// Calculates Miss Rate (Why did I create it?)
void Cache::miss_rate_calc() {
	if((reads + writes) > 0){
	miss_rate = (float(read_misses) + float(write_misses)) / (float(reads) + float(writes));
	}
	else{
		miss_rate = 0;
	}
	
}

// Prints block contents (surprise surprise)
void Cache::print_block_contents() {
    
    for (int i = 0; i < numSets; i++) {
        // Create a vector to store blocks and their LRU order
        vector<pair<int, Cache_Block>> temp;
        for (int j = 0; j < assoc; j++) {
            temp.push_back({mySet[i].blocks[j].lru_counter, mySet[i].blocks[j]});
        }

        sort(temp.begin(), temp.end(), [](const auto& a, const auto& b) {
           return a.first < b.first;  // Sort by first element in ascending order
        });

        // Print the blocks in LRU order
        cout << "set\t" << dec << i << ":\t";
        for (const auto& temp : temp) {
            const Cache_Block& block = temp.second;
            cout << hex << block.tag << " ";
            if (block.dirtyBit == 1) {
               cout << "D ";
            }
            else
                cout << "  ";
        }
        cout << endl;
    }
    cout << endl;
}

// Prints performance parameters (another unused function)
void Cache::print_perf_params() {
    cout << "===== Measurements  =====" << endl;
    cout << "a. number of L1 reads:			" << dec << reads << endl;
    cout << "b. number of L1 read misses:		" << dec << read_misses << endl;
    cout << "c. number of L1 writes:			" << dec << writes << endl;
    cout.precision(4);
    cout.unsetf(ios::floatfield);
    cout.setf(ios::fixed, ios::floatfield);
    cout << "d. number of L1 write misses:		" << dec << write_misses << endl;
    cout << "e. L1 miss rate:			" << dec << ((float)read_misses + (float)write_misses) / ((float)reads + (float)writes) << endl;
    cout.unsetf(ios::floatfield);
    cout << "f. number of writebacks from L1 memory:	" << dec << writebacks << endl;
    cout << "g. total memory traffic:		" << dec << mem_traffic << endl;
}

// Good ol' destructor
Cache::~Cache() {
    for (uint32_t i = 0; i < numSets; i++) {
        for (uint32_t j = 0; j < assoc; j++) {
            if (mySet[i].blocks[j].validBit && mySet[i].blocks[j].dirtyBit) {
                writebacks++;
                mem_traffic++;
            }
        }
        delete[] mySet[i].blocks;
    }
    delete[] mySet;
}
# Multilevel-Cache-Simulator-with-Prefetch

A C++ simulator for L1/L2 cache levels with a configurable stream-buffer prefetch unit, implementing the WBWA policy.

## Project Overview

This project implements a configurable cache simulator that can model different levels of memory hierarchy with stream buffer prefetching capabilities. The simulator provides insights into cache performance metrics and memory traffic patterns.

## Key Implementation Features

1. Configurable Cache Module
   - Flexible cache size, associativity, and block size
   - Supports multiple cache levels (L1, L2)
   - LRU replacement policy
   - Write-back and write-allocate policies

2. Stream Buffer Prefetching
   - Configurable number of stream buffers (N)
   - Configurable buffer depth (M blocks per buffer)
   - LRU replacement for stream buffers
   - Intelligent prefetch stream management

3. Memory Hierarchy Configurations
   - Single-level cache (L1)
   - Two-level cache (L1 + L2)
   - Cache with prefetch unit
   - Multiple cache levels with prefetch unit

## Technical Details

### Cache Parameters
* `SIZE`: Total bytes of data storage
* `ASSOC`: Cache associativity
* `BLOCKSIZE`: Number of bytes per block
* `PREF_N`: Number of stream buffers
* `PREF_M`: Blocks per stream buffer

### Configurations supported
<div align="center">
<img width="800" alt="config supported" src="https://github.com/user-attachments/assets/1586344b-c055-4326-a633-3b3ee9589d26">
</div>

## Simulator Usage

```bash
./sim <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>
```

Example:
```bash
./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
```

## Performance Metrics
* Cache read/write hits and misses
* Miss rates for each cache level
* Number of writebacks
* Prefetch statistics
* Total memory traffic

## Input Format
Traces follow the format:
```
r|w <hex address>
r|w <hex address>
...
```
Where:
- 'r': Read operation
- 'w': Write operation
- `<hex address>`: 32-bit memory address in hex

## Implementation Notes
* Supports power-of-two block sizes
* Implements multi-level cache coherence
* Handles prefetch buffer management
* Tracks LRU information for both cache and stream buffers
* Maintains accurate performance statistics

## Project Requirements
* C/C++ compiler
* Make build system
* Input trace files

## Build Instructions
```bash
make clean
make
```

## Testing
1. Prepare trace files in correct format
2. Run simulator with desired configuration
3. Analyze output metrics and cache contents



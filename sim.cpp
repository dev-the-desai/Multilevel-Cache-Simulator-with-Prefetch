#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iomanip> 
#include "sim.h"
#include "cache.h"

/*  "argc" holds the number of command-line arguments.
   "argv[]" holds the arguments themselves.

   Example:
   ./sim 32 8192 4 262144 8 3 10 gcc_trace.txt
   argc = 9
   argv[0] = "./sim"
   argv[1] = "32"
   argv[2] = "8192"
   ... and so on
*/
using namespace std;

int main (int argc, char *argv[]) {
   FILE *fp;			            // File pointer.
   char *trace_file;		         // This variable holds the trace file name.
   cache_params_t params;	      // Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;			               // This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;		            // This variable holds the request's address obtained from the trace.
				                     // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9) {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }
    
   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t) atoi(argv[1]);
   params.L1_SIZE   = (uint32_t) atoi(argv[2]);
   params.L1_ASSOC  = (uint32_t) atoi(argv[3]);
   params.L2_SIZE   = (uint32_t) atoi(argv[4]);
   params.L2_ASSOC  = (uint32_t) atoi(argv[5]);
   params.PREF_N    = (uint32_t) atoi(argv[6]);
   params.PREF_M    = (uint32_t) atoi(argv[7]);
   trace_file       = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *) NULL) {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

   // Instantiate Caches

   // Chech if we need to create L2
   bool L2_present = params.L2_SIZE;

   // Create L2 regardless (lol)
   Cache L2_cache(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE, params.PREF_N, params.PREF_M, NULL);

   // If L2_present==true, Set the pointer to L2 or else NULL
   Cache *L2_pointer = L2_present ? &L2_cache : NULL;

   // Create L1 cache instance 
   Cache L1_cache(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE, params.PREF_N, params.PREF_M, L2_pointer);

   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);
   printf("\n");

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2) {	// Stay in the loop if fscanf() successfully parsed two tokens as specified.
      if (rw == 'r') {
         L1_cache.cache_read(addr);
      }
      else if (rw == 'w') {
         L1_cache.cache_write(addr);
      }
      else {
         cout << "\nHERE" << endl;
         printf("Error: Unknown request type %c.\n", rw);
	      exit(EXIT_FAILURE);
      }
   }
   
   // Print L1 contents
   cout << "===== L1 contents =====" << endl;
   L1_cache.print_block_contents();

   // Print L2 contents (if L2 is present)
   if (L2_present) {
      cout << "===== L2 contents =====" << endl;
      L2_cache.print_block_contents();
   }

   // Print L1 buffer contents (if buffer is present)
   if (L1_cache.buffer_active) {
      L1_cache.print_buffer();
   }

   // Print L2 buffer contents (if buffer is present)
   if (L2_cache.buffer_active) {
      L2_cache.print_buffer();
   }

   // Set parameters based on L2 presence
   float mr2 = L2_present ? ((float)L2_cache.read_misses / (float)L2_cache.reads) : 0;
   uint32_t mem_traffic = L2_present ? (L2_cache.read_misses + L1_cache.prefetches + L2_cache.write_misses + L2_cache.writebacks + L2_cache.prefetches) : (L1_cache.read_misses + L1_cache.write_misses + L1_cache.writebacks + L1_cache.prefetches);

   cout << "===== Measurements =====" << endl;
   cout << left << setw(30) << "a. L1 reads:"                  << dec << L1_cache.reads << endl;
   cout << left << setw(30) << "b. L1 read misses:"            << dec << L1_cache.read_misses << endl;
   cout << left << setw(30) << "c. L1 writes:"                 << dec << L1_cache.writes << endl;
   cout << left << setw(30) << "d. L1 write misses:"           << dec << L1_cache.write_misses << endl;
   cout << left << setw(30) << "e. L1 miss rate:"              << fixed << setprecision(4) << ((float)L1_cache.read_misses + (float)L1_cache.write_misses) / ((float)L1_cache.reads + (float)L1_cache.writes) << endl;
   cout << left << setw(30) << "f. L1 writebacks:"             << dec << L1_cache.writebacks << endl;
   cout << left << setw(30) << "g. L1 prefetches:"             << dec << L1_cache.prefetches << endl;
   cout << left << setw(30) << "h. L2 reads (demand):"         << dec << L2_cache.reads << endl;
   cout << left << setw(30) << "i. L2 read misses (demand):"   << dec << L2_cache.read_misses << endl;
   cout << left << setw(30) << "j. L2 reads (prefetch):"       << dec << L2_cache.read_prefetch << endl;
   cout << left << setw(30) << "k. L2 read misses (prefetch):" << dec << L2_cache.read_prefetch_misses << endl;
   cout << left << setw(30) << "l. L2 writes:"                 << dec << L2_cache.writes << endl;
   cout << left << setw(30) << "m. L2 write misses:"           << dec << L2_cache.write_misses << endl;
   cout << left << setw(30) << "n. L2 miss rate:"              << fixed << setprecision(4) << mr2 << endl;
   cout << left << setw(30) << "o. L2 writebacks:"             << dec << L2_cache.writebacks << endl;
   cout << left << setw(30) << "p. L2 prefetches:"             << dec << L2_cache.prefetches << endl;
   cout << left << setw(30) << "q. memory traffic:"            << dec << mem_traffic << endl;

   return(0);
}

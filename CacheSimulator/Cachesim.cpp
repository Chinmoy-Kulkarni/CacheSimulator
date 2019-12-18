#include "cachesim.h"
#include <cstdio>

#define index(r,c,num_cols)  (r*num_cols) + c

typedef struct {            // each block
	uint64_t tag;           // Tag of cache line
	bool valid;             // Valid => 1, Invalid => 0
	bool dirty;             // Dirty (Written to) => 1, Not Dirty (Not written) => 0
	uint64_t timestamp;     // Using accesses counter as timestamp for LRU
} block_t;

typedef struct {                //big cache rows and columns
	block_t* cache;             // Array Pointer for cache directory
	uint64_t size_block;        // 2^block_size byte block
	uint64_t num_sets;          // 2^num_sets total sets
	uint64_t num_ways;          // num_ways associativity
} cache_t;

cache_t* L1;       // L1 Cache
cache_t* VC;       // VC Cache
cache_t* L2;       // L2 Cache

// Function to find blocks in main cache (used to both L1 & L2)
bool 
main_cache_find_replace(cache_t* L,
						char type, 
						uint64_t& arg, 
						block_t& replacement_block, 
						uint64_t& replacement_set_number, 
						uint64_t& replacement_way_number, 
						uint64_t& accesses, 
						uint64_t& read_misses, 
						uint64_t& write_misses);

// Function to find blocks in victim cache
bool 
victim_cache_find_replace(cache_t* VC,
						  uint64_t& arg,
						  uint64_t& evicted_block_tag,
						  uint64_t& accesses,
						  uint64_t& hits);

/*-----------------------------------------------------------------------*/

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @c1 The total number of bytes for data storage in L1 is 2^c1
 * @b1 The size of L1's blocks in bytes: 2^b1-byte blocks.
 * @s1 The number of blocks in each set of L1: 2^s1 blocks per set.
 * @v Victim Cache's total number of blocks (blocks are of size 2^b1).
 *    v is in [0, 4].
 * @c2 The total number of bytes for data storage in L2 is 2^c2
 * @b2 The size of L2's blocks in bytes: 2^b2-byte blocks.
 * @s2 The number of blocks in each set of L2: 2^s2 blocks per set.
 * Note: c2 >= c1, b2 >= b1 and s2 >= s1.
 */

void
setup_cache(uint64_t c1,
	uint64_t b1,
	uint64_t s1,
	uint64_t v,
	uint64_t c2,
	uint64_t b2,
	uint64_t s2)
{
	//total size = 2^c
	//block size = 2^b
	//set associativity = 2^s

	uint64_t i, j;   // Temps

	// Allocate & Intitialise L1 cache & its parameters
	L1 = new cache_t;
	L1->cache = new block_t[1 << (c1 - b1)];
	L1->size_block = b1;
	L1->num_sets = c1 - (b1 + s1);            // rows = total /(block size * SA)
	L1->num_ways = 1 << s1;

	// Initialize cache to zero
	for (i = 0; i < (uint64_t)(1 << L1->num_sets); i++) {
		for (j = 0; j < L1->num_ways; j++) {
			L1->cache[index(i, j, L1->num_ways)].tag = 0;         // (row*SA + col)
			L1->cache[index(i, j, L1->num_ways)].valid = 0;
			L1->cache[index(i, j, L1->num_ways)].dirty = 0;
			L1->cache[index(i, j, L1->num_ways)].timestamp = 0;
		}
	}

	// Allocate & Intitialise VC cache & its parameters
	VC = new cache_t;
	VC->cache = new block_t[v];
	VC->size_block = b1;
	VC->num_sets = 1;       //flat (Fully associative)
	VC->num_ways = v;       // SA = no. of blocks 

	// Initialize cache to zero
	for (j = 0; j < VC->num_ways; j++) {     //flat one row num_ways columns
		VC->cache[j].tag = 0;
		VC->cache[j].valid = 0;
		VC->cache[j].dirty = 0;
		VC->cache[j].timestamp = 0;
	}

	// Allocate & Intitialise L2 cache & its parameters
	L2 = new cache_t;
	L2->cache = new block_t[1 << (c2 - b2)];
	L2->size_block = b2;
	L2->num_sets = c2 - b2 - s2;
	L2->num_ways = 1 << s2;

	// Initialize cache to zero
	for (i = 0; i < (uint64_t)(1 << L2->num_sets); i++) {
		for (j = 0; j < L2->num_ways; j++) {
			L2->cache[index(i, j, L2->num_ways)].tag = 0;
			L2->cache[index(i, j, L2->num_ways)].valid = 0;
			L2->cache[index(i, j, L2->num_ways)].dirty = 0;
			L2->cache[index(i, j, L2->num_ways)].timestamp = 0;
		}
	}
}

/**
 * Subroutine that simulates the cache one trace event at a time.
 * XXX: You're responsible for completing this routine
 *
 * @type The type of event, can be READ or WRITE.
 * @arg  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void
cache_access(char type,
	uint64_t arg,
	cache_stats_t* p_stats)
{

	// Increament read/write counters to input
	if (type == READ) {
		p_stats->reads++;
	}
	else {
		p_stats->writes++;
	}

	// --------------------------------Access L1---------------------------------

	// replacement block details
	block_t L1_replacement_block;           //the block
	uint64_t L1_replacement_set_number;     // replacement block_row
	uint64_t L1_replacement_way_number;     // replacement block_col

	// Look for block in L1, if not found return replacement block & position
	bool flag_L1 = main_cache_find_replace(L1, type, arg, L1_replacement_block, L1_replacement_set_number, L1_replacement_way_number, p_stats->accesses, p_stats->read_misses_l1, p_stats->write_misses_l1);

	// Done if L1 Hit
	if (flag_L1 == true) {

		// Debug Prints: L1 Hit
		if (VC->num_ways == 0) {
			printf("H1**\n");
		}
		else {
			printf("H1****\n");
		}

		return;
	}

	// --------------------------------Access VC---------------------------------

	bool flag_VC = false;
	// If VC Exists
	if (VC->num_ways > 0) {

		// Increment VC Access
		p_stats->accesses_vc++;

		// If Valid Block Evicted search VC
		if (L1_replacement_block.valid == 1) {

			// Build VC tag
			uint64_t VC_tag = (L1_replacement_block.tag << L1->num_sets) | L1_replacement_set_number;   //shift tag to left and append set(index) from right

			// Look for block in VC, if not found return replacement block and position
			flag_VC = victim_cache_find_replace(VC, arg, VC_tag, p_stats->accesses_vc, p_stats->victim_hits);

			// Debug Prints: L1 Miss, VC Hit
			if (flag_VC == true) {
				printf("M1HV**\n");
			}
		}
	}

	// --------------------------------Access L2---------------------------------

	// If not in VC or VC does exist
	if (flag_VC == false) {

		// replacement block details
		block_t L2_replacement_block;
		uint64_t L2_replacement_set_number;
		uint64_t L2_replacement_way_number;

		// Look for block in L2, if not found return replacement block and position
		bool flag_L2 = main_cache_find_replace(L2, READ, arg, L2_replacement_block, L2_replacement_set_number, L2_replacement_way_number, p_stats->accesses_l2, p_stats->read_misses_l2, p_stats->write_misses_l2);


		if (flag_L2 == false) {
			// Debug Print: L1 Miss, VC Miss, L2 Miss
			if (VC->num_ways == 0) {
				printf("M1M2\n");
			}
			else {
				printf("M1MVM2\n");
			}
		}
		else {
			// Debug Print: L1 Miss, VC Miss, L2 Hit
			if (VC->num_ways == 0) {
				printf("M1H2\n");
			}
			else {
				printf("M1MVH2\n");
			}
		}

		// --------------------------------Writeback L2---------------------------------
		// If L2 miss and valid, dirty block evicted
		if (flag_L2 == false && L2_replacement_block.valid == 1 && L2_replacement_block.dirty == 1) {
			// L2 Writeback
			p_stats->write_back_l2++;
		}
	}

	// --------------------------------Writeback L1---------------------------------
	// If L1 miss and valid, dirty block evicted
	if (flag_L1 == false && L1_replacement_block.valid == 1 && L1_replacement_block.dirty == 1) {

		// L1 Writeback
		p_stats->write_back_l1++;

		// replacement block details
		block_t L2_WB_replacement_block;
		uint64_t L2_WB_replacement_set_number;
		uint64_t L2_WB_replacement_way_number;

		// Re-create 64 bit address, ignoring block bits of L1
		uint64_t replace_address = (L1_replacement_block.tag << L1->num_sets) | L1_replacement_set_number;
		replace_address = replace_address << L1->size_block;

		// Look for block in L2, if not found return replacement block and position
		bool flag_WB_L2 = main_cache_find_replace(L2, WRITE, replace_address, L2_WB_replacement_block, L2_WB_replacement_set_number, L2_WB_replacement_way_number, p_stats->accesses_l2, p_stats->read_misses_l2, p_stats->write_misses_l2);

		// If l2 evicted new block, writeback
		if (flag_WB_L2 == false && L2_WB_replacement_block.valid == 1 && L2_WB_replacement_block.dirty == 1) {

			// L2 Writeback
			p_stats->write_back_l2++;

		}
	}
}

/**
 * Subroutine that simulates the cache. If computes the set & tag for the
 * address and searches. If found returns true for hit. Else finds the LRU
 * block. It also increment access, read_misses, write_misses for that cache.
 *
 * @L    The cache (L1/L2)
 * @type The type of event, can be READ or WRITE.
 * @arg  The target memory address
 * @replacement_block  Returns block evicted
 * @replacement_set_number Set number of evicted block
 * @replacement_way_number Way number of evicted block
 * @accesses number of accesses to specified cache
 * @read_misses number of read misses to specified cache
 * @write_misses number of write misses to specified cache
 */
bool 
main_cache_find_replace(cache_t* L,
						char type, 
						uint64_t& arg, 
						block_t& replacement_block, 
						uint64_t& replacement_set_number, 
						uint64_t& replacement_way_number, 
						uint64_t& accesses, 
						uint64_t& read_misses, 
						uint64_t& write_misses)
{
	// Increment Counter
	accesses++;

	// Find set number of input address
	uint64_t set_number = (arg >> L->size_block)& ((1 << L->num_sets) - 1); // finding set number by remove block_size_bits from right and then ANDing with num_set_bit


	// Find tag of input address
	uint64_t tag = arg >> (L->size_block + L->num_sets);

	// Initialise variables to find least recently used tag
	uint64_t replacement_index = 0;
	uint64_t min_timestamp = L->cache[index(set_number, 0, L->num_ways)].timestamp; // timestamp of correct row and first col.

	// Traverse particular set
	for (uint64_t i = 0; i < L->num_ways; i++) {

		// If line is valid and tag matches
		if (L->cache[index(set_number, i, L->num_ways)].valid == 1 && L->cache[index(set_number, i, L->num_ways)].tag == tag) {

			// Renew time stamp
			L->cache[index(set_number, i, L->num_ways)].timestamp = accesses;

			// Set dirty if write access
			if (type == WRITE) {
				L->cache[index(set_number, i, L->num_ways)].dirty = 1;
			}

			// Position of hit
			replacement_set_number = set_number;
			replacement_way_number = i;

			return true;

		}
		else {
			// If access to line is older than min_timestamp
			if (L->cache[index(set_number, i, L->num_ways)].timestamp < min_timestamp) {
				// Store this as new minimum (least recently accessed)
				replacement_index = i;
				min_timestamp = L->cache[index(set_number, i, L->num_ways)].timestamp;
			}
		}
	}

	// Store details of replacement block
	replacement_block.valid = L->cache[index(set_number, replacement_index, L->num_ways)].valid;
	replacement_block.tag = L->cache[index(set_number, replacement_index, L->num_ways)].tag;
	replacement_block.dirty = L->cache[index(set_number, replacement_index, L->num_ways)].dirty;
	replacement_set_number = set_number;
	replacement_way_number = replacement_index;

	// Store new entry
	L->cache[index(set_number, replacement_index, L->num_ways)].valid = 1;
	L->cache[index(set_number, replacement_index, L->num_ways)].tag = tag;
	if (type == READ) {
		L->cache[index(set_number, replacement_index, L->num_ways)].dirty = 0;
		read_misses++;
	}
	else {
		L->cache[index(set_number, replacement_index, L->num_ways)].dirty = 1;
		write_misses++;
	}
	L->cache[index(set_number, replacement_index, L->num_ways)].timestamp = accesses;

	return false;
}

/**
 * Subroutine that simulates the victim cache. It computes the set & tag for the
 * address and searches. If found returns true for hit and replaces with L1
 * evicted block. Else finds the last block. It also increment access, hits for cache.
 *
 * @VC    The victim cache
 * @type The type of event, can be READ or WRITE.
 * @arg  The target memory address
 * @evicted_block_tag  block evicted by L1
 * @accesses number of accesses to specified cache
 * @hits number of hits to specified cache
 */

bool 
victim_cache_find_replace(cache_t* VC,
						  uint64_t& arg,
						  uint64_t& evicted_block_tag,
						  uint64_t& accesses,
						  uint64_t& hits)
{
	// Find tag of input address
	uint64_t tag = arg >> VC->size_block;

	// Initialise variables to find least recently used tag
	uint64_t replacement_index = 0;
	uint64_t min_timestamp = VC->cache[0].timestamp;

	// Traverse
	for (uint64_t i = 0; i < VC->num_ways; i++) {

		// If line is valid and tag matches
		if (VC->cache[i].valid == 1 && VC->cache[i].tag == tag) {

			// Renew time stamp
			VC->cache[i].timestamp = accesses;

			// Replace entries with L1
			VC->cache[i].tag = evicted_block_tag;

			// Increment hits
			hits++;

			return true;

		}
		else {
			// If access to line is older than min_timestamp
			if (VC->cache[i].timestamp < min_timestamp) {
				// Store this as new minimum (least recently accessed)
				replacement_index = i;
				min_timestamp = VC->cache[i].timestamp;
			}
		}
	}

	VC->cache[replacement_index].valid = 1;
	VC->cache[replacement_index].tag = evicted_block_tag;
	VC->cache[replacement_index].timestamp = accesses;

	return false;

}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */

void 
complete_cache(uint64_t c1,
			   uint64_t b1,
			   uint64_t s1,
			   uint64_t v,
			   uint64_t c2,
			   uint64_t b2,
			   uint64_t s2,
			   cache_stats_t* p_stats)
{
	// Clear L1 Memory
	delete[] L1->cache;
	delete L1;

	// Clear L2 Memory
	delete[] L2->cache;
	delete L2;

	// Clear VC Memory
	delete[] VC->cache;
	delete VC;

	float ht1 = 4 + (0.2 * s1);
	float mr1 = (p_stats->read_misses_l1 + p_stats->write_misses_l1) / (float)p_stats->accesses;

	float mrvc = 1 - (float)(p_stats->victim_hits / (float)(p_stats->accesses_vc));

	float ht2 = 20 + (0.4 * s2);
	float mr2 = (float)(p_stats->read_misses_l2 + p_stats->write_misses_l2) / (float)p_stats->accesses_l2;
	float mp2 = 500;

	float aat_l2 = ht2 + (mr2 * mp2);

	float mp1;
	if (v == 0) {
		mp1 = aat_l2;
	}
	else {
		mp1 = aat_l2 * mrvc;
	}

	p_stats->avg_access_time_l1 = (float)(ht1 + (mr1 * mp1));

}

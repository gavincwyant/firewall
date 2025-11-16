#include <stdint.h>
#include <time.h>

#define STARTING_TOKENS 1
#define TABLE_SIZE 1024

typedef struct bucket{
	uint32_t ip;
	int tokens;
	time_t last_refill;
	struct bucket *next;
}bucket;

typedef struct {
	bucket **buckets;
	int size;	//number of buckets (total capacity)
	int count;	//number of entries (current num)
}ip_table;


bucket *lookup(ip_table *table, uint32_t ip);
bucket *bucket_get_or_create(ip_table *table, uint32_t ip);

ip_table *create_table(int initial_size);
void free_table(ip_table *table);

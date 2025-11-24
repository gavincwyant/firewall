#include "bucket.h"
#include <stdlib.h>

static uint32_t hash_ip(uint32_t ip){
	return (ip * 2654435761u);
}

ip_table *create_table(int initial_size){
	ip_table *table = (ip_table *)malloc(initial_size * sizeof(ip_table));
	table->size = initial_size;
	table->count = 0;
	table->buckets = calloc(initial_size, sizeof(bucket *));
	return table;
}

void free_table(ip_table *table){
	int size = table->size;
	for (int i = 0; i < size; i++){
		bucket *b = table->buckets[i];
		while(b){
			bucket *next = b->next;
			free(b);
			b = next;
		}
	}
}

bucket *lookup(ip_table *table, uint32_t ip){
	uint32_t index = hash_ip(ip) % table->size;
	bucket *b = table->buckets[index];
	while(b){
		if(b->ip == ip){
			return b;
		}
		b = b->next;
	}
	return NULL;
}



bucket *bucket_get_or_create(ip_table* table, uint32_t ip){
	bucket *b = lookup(table, ip);
	if (b){
		return b;
	}

	uint32_t index = hash_ip(ip) % table->size;
	bucket *new_bucket = (bucket *)malloc(sizeof(bucket));
	new_bucket->ip = ip;
	new_bucket->tokens = STARTING_TOKENS;
	new_bucket->last_refill = time(NULL);
	new_bucket->uses = 1;

	//insert to head of list
	new_bucket->next = table->buckets[index];
	table->buckets[index] = new_bucket;

	table->count++;
	return new_bucket;
}

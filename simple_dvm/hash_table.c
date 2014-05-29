#include <stdio.h>
#include "list_head.h"
#include "hash_table.h"

void hash_init(struct hash_table *table)
{
	int i;

	for (i = 0; i < HASH_SIZE; i++)
	{
		list_init(&table->entries[i]);
	} 
}

unsigned int hash(char *str)
{
	unsigned int hash = 5381;
	int c;

	while (c = *str++)
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash;
}

void hash_add(struct hash_table *table, struct list_head *node, unsigned int key)
{
	list_add_head(node, &table->entries[key % HASH_SIZE]); 
}

struct list_head *hash_get(struct hash_table *table, unsigned int key)
{
	if (list_empty(&table->entries[key % HASH_SIZE]))
			return NULL;
	return &table->entries[key % HASH_SIZE];
}


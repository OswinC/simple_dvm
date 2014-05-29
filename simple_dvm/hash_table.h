#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

#define HASH_SIZE 128

struct hash_table
{
	struct list_head entries[HASH_SIZE];
};

void hash_init(struct hash_table *table);
unsigned int hash(char *ptr);
void hash_add(struct hash_table *table, struct list_head *node, unsigned int key);
struct list_head *hash_get(struct hash_table *table, unsigned int key);

#endif // _HASHTABLE_H_

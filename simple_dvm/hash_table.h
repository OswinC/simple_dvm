#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

struct hash_table
{
	struct list_head entries[128];
};

void hash_init(struct hash_table *table);
unsigned int hash(char *ptr);
void hash_add(struct hash_table *table, struct list_head *node, int key);
struct list_head *hash_get(struct hash_table *table, int key);

#endif // _HASHTABLE_H_

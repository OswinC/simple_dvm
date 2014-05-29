#include <stdio.h>
#include "list_head.h"
#include "hash_table.h"

struct test
{
	struct list_head entry;
	char data[128];
};

int main()
{
	int i;
	struct hash_table table;
	struct list_head *e;
	struct test array[10];

	hash_init(&table);

	for (i = 0; i < 10; i++)
	{
		list_init(&array[i].entry);
		sprintf(array[i].data, "%d", i);
	}

	hash_add(&table, &array[0].entry, hash(array[0].data));
	hash_add(&table, &array[3].entry, hash(array[3].data));
	hash_add(&table, &array[4].entry, hash(array[4].data));
	hash_add(&table, &array[5].entry, hash(array[5].data));
	hash_add(&table, &array[7].entry, hash(array[7].data));
	hash_add(&table, &array[9].entry, hash(array[9].data));

	for (i = 0; i < 10; i++)
	{
		char str[32];
		struct test *tmp;

		sprintf(str, "%d", i);
		e = hash_get(&table, hash(str));
		if (!e)
		{
			printf("%d: null\n", i);
			continue;
		}

		tmp = container_of(e, struct test, entry);
		printf("%d: %s\n", i, tmp->data);
	}

	return 0;
}

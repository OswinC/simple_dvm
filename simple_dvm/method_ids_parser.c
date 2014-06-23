/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#include "simple_dvm.h"

void parse_method_ids(DexFileFormat *dex, unsigned char *buf, int offset)
{
    int i = 0;
    if (is_verbose() > 3)
        printf("parse method ids offset = %04x\n", offset + sizeof(DexHeader));
    dex->method_id_item = malloc(
                              sizeof(method_id_item) * dex->header.methodIdsSize);

    for (i = 0 ; i < dex->header.methodIdsSize ; i++) {
        memcpy(&dex->method_id_item[i],
               buf + i * sizeof(method_id_item) + offset,
               sizeof(method_id_item));

        if (is_verbose() > 3)
            printf(" method[%d], cls_id = %d, proto_id = %d, name_id = %d, %s\n",
                   i,
                   dex->method_id_item[i].class_idx,
                   dex->method_id_item[i].proto_idx,
                   dex->method_id_item[i].name_idx,
                   dex->string_data_item[dex->method_id_item[i].name_idx].data);
    }
}

method_id_item *get_method_item_by_name(DexFileFormat *dex, int class_idx, const char *name)
{
	int i, j;
	method_id_item *found = NULL;

	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		if (dex->class_def_item[i].class_idx == class_idx)
		{
			class_data_item *item = &dex->class_data_item[i];
			int methods_size = item->direct_methods_size;
			int aggregated_idx = 0;

			for (j = 0; j < methods_size; j++)
			{
				encoded_method *tmp = &item->direct_methods[j];
				aggregated_idx += tmp->method_idx_diff;
				method_id_item *item = &dex->method_id_item[aggregated_idx]; 
				if (strcmp(get_string_data(dex, item->name_idx), name) == 0) 
				{
					found = item;
					break;
				}
			}

			if (found)
				break;
		}
	}

	return found;
}

method_id_item *get_method_item(DexFileFormat *dex, int method_id)
{
    if (method_id >= 0 && method_id < dex->header.methodIdsSize)
        return &dex->method_id_item[method_id];
    return 0;
}

int get_method_name(DexFileFormat *dex, int method_id, char *name)
{
    method_id_item *m = get_method_item(dex, method_id);
    type_id_item *type_class = 0;
    proto_id_item *proto_item = 0;
    char *method_name = 0;
    char *class_name = 0;
    if (m != 0) {
        method_name = get_string_data(dex, m->name_idx);
        type_class = get_type_item(dex, m->class_idx);
        if (type_class != 0)
            class_name = get_string_data(dex, type_class->descriptor_idx);
        proto_item = get_proto_item(dex, m->proto_idx);
    }
    return 0;
}

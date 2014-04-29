/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#include "simple_dvm.h"

/* Print Dex File Format */
static void printDexHeader(DexHeader *dex)
{
    int i = 0;
    printf("Magic Number = ");
    for (i = 0 ; i < 8 ; i++)
        printf("0x%02x ", dex->magic[i]);
    printf("( ");
    for (i = 0 ; i < 8 ; i++)
        if (dex->magic[i] != '\n') {
            printf("%c", dex->magic[i]);
        } else {
            printf("\\n");
        }
    printf(" )\n");

    printf("Checksum      = ");
    for (i = 3 ; i >= 0 ; i--)
        printf("%02x ", dex->checksum[i]);
    printf("\n");

    printf("FileSize      = %4d (0x%04x)\n", dex->fileSize, dex->fileSize);
    printf("headerSize    = %4d (0x%04x)\n", dex->headerSize, dex->headerSize);
    printf("endianTag     = %4d (0x%04x)\n", dex->endianTag, dex->endianTag);
    printf("linkSize      = %4d (0x%04x)\n", dex->linkSize, dex->linkSize);
    printf("mapOff        = %4d (0x%04x)\n", dex->mapOff, dex->mapOff);
    printf("stringIdsSize = %4d (0x%04x)\n", dex->stringIdsSize, dex->stringIdsSize);
    printf("stringIdsOff  = %4d (0x%04x)\n", dex->stringIdsOff, dex->stringIdsOff);
    printf("typeIdsSize   = %4d (0x%04x)\n", dex->typeIdsSize, dex->typeIdsSize);
    printf("typeIdsOff    = %4d (0x%04x)\n", dex->typeIdsOff, dex->typeIdsOff);
    printf("protoIdsSize  = %4d (0x%04x)\n", dex->protoIdsSize, dex->protoIdsSize);
    printf("protoIdsOff   = %4d (0x%04x)\n", dex->protoIdsOff, dex->protoIdsOff);
    printf("fieldIdsSize  = %4d (0x%04x)\n", dex->fieldIdsSize, dex->fieldIdsSize);
    printf("fieldIdsOff   = %4d (0x%04x)\n", dex->fieldIdsOff, dex->fieldIdsOff);
    printf("methodIdsSize = %4d (0x%04x)\n", dex->methodIdsSize, dex->methodIdsSize);
    printf("methodIdsOff  = %4d (0x%04x)\n", dex->methodIdsOff, dex->methodIdsOff);
    printf("classDefsSize = %4d (0x%04x)\n", dex->classDefsSize, dex->classDefsSize);
    printf("classDefsOff  = %4d (0x%04x)\n", dex->classDefsOff, dex->classDefsOff);
    printf("dataSize      = %4d (0x%04x)\n", dex->dataSize, dex->dataSize);
    printf("dataOff       = %4d (0x%04x)\n", dex->dataOff, dex->dataOff);
}

void printDexFile(DexFileFormat *dex)
{
    printDexHeader(&dex->header);
}

/* Parse Dex File */
int parseDexFile(char *file, DexFileFormat *dex)
{
    FILE *fp = 0;
    unsigned char *buf = 0;

    fp = fopen(file, "rb");
    if (fp == 0) {
        printf("Open file %s failed\n", file);
        return -1;
    }
    memset(dex, 0, sizeof(dex));
    fread(&dex->header, sizeof(DexHeader), 1, fp);
    buf = (unsigned char *) malloc(
              sizeof(u1) * (dex->header.fileSize - sizeof(DexHeader)));

    /* read all value into buf */
    fread(buf, (dex->header.fileSize - sizeof(DexHeader)), 1, fp);
    fclose(fp);

    parse_map_list(dex, buf, dex->header.mapOff - sizeof(DexHeader));
    parse_string_ids(dex, buf, dex->header.stringIdsOff - sizeof(DexHeader));
    parse_type_ids(dex, buf, dex->header.typeIdsOff - sizeof(DexHeader));
    parse_proto_ids(dex, buf, dex->header.protoIdsOff - sizeof(DexHeader));
    parse_field_ids(dex, buf, dex->header.fieldIdsOff - sizeof(DexHeader));
    parse_method_ids(dex, buf, dex->header.methodIdsOff - sizeof(DexHeader));
    parse_class_defs(dex, buf, dex->header.classDefsOff - sizeof(DexHeader));

    if (dex->header.dataSize > 0) {
        dex->data = malloc(sizeof(u1) * dex->header.dataSize);
        memcpy(dex->data, buf, dex->header.dataOff - sizeof(DexHeader));
    }

    free(buf);
    return 0;
}

void free_map_list(DexFileFormat *dex)
{
	if (dex->type_list.type_item)
		free(dex->type_list.type_item);

	if (dex->map_list.map_item)
		free(dex->map_list.map_item);
}

void free_string_ids(DexFileFormat *dex)
{
	if (dex->string_data_item)
		free(dex->string_data_item);

	if (dex->string_ids)
		free(dex->string_ids);
}

void free_type_ids(DexFileFormat *dex)
{
	if (dex->type_id_item)
		free(dex->type_id_item);
}

void free_proto_ids(DexFileFormat *dex)
{
	int i;

	if (dex->proto_id_item)
		free(dex->proto_id_item);

	if (dex->proto_type_list) {
		for (i = 0; i < dex->header.protoIdsSize; i++) {
			if (dex->proto_type_list[i].type_item)
				free(dex->proto_type_list[i].type_item);
		}

		free(dex->proto_type_list);
	}
}

void free_field_ids(DexFileFormat *dex)
{
	if (dex->field_id_item)
		free(dex->field_id_item);
}

void free_method_ids(DexFileFormat *dex)
{
	if (dex->method_id_item)
		free(dex->method_id_item);
}

void free_encoded_method(encoded_method *method)
{
	if (method->code_item.insns)
		free(method->code_item.insns);
}

void free_class_data_item(DexFileFormat *dex, int idx)
{
	int i;
	class_data_item *item = &dex->class_data_item[idx];

	if (item->direct_methods) {
		for (i = 0; i < item->direct_methods_size; i++) {
			free_encoded_method(&item->direct_methods[i]);
		}

		free(item->direct_methods);
	}
}

void free_class_defs(DexFileFormat *dex)
{
	int i;

	if (dex->class_def_item)
		free(dex->class_def_item);

	if (dex->class_data_item) {
		for (i = 0; i < dex->header.classDefsSize; i++) {
			free_class_data_item(dex, i);
		}

		free(dex->class_data_item);
	}
}

void freeDex(DexFileFormat *dex)
{
	free_map_list(dex);
	free_string_ids(dex);
	free_type_ids(dex);
	free_proto_ids(dex);
	free_field_ids(dex);
	free_method_ids(dex);
	free_class_defs(dex);
}

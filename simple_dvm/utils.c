/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simple_dvm.h"

static int verbose_flag = 0;

int is_verbose()
{
    return verbose_flag;
}

int enable_verbose()
{
    verbose_flag = 1;
    return 0;
}

int disable_verbose()
{
    verbose_flag = 0;
    return 0;
}

int set_verbose(int l)
{
    verbose_flag = l;
    return 0;
}

void load_reg_to(simple_dalvik_vm *vm, int id, unsigned char *ptr)
{
    simple_dvm_register *r = &vm->regs[id];
    ptr[0] = r->data[0];
    ptr[1] = r->data[1];
    ptr[2] = r->data[2];
    ptr[3] = r->data[3];
}

void load_reg_to_double(simple_dalvik_vm *vm, int id, unsigned char *ptr)
{
    simple_dvm_register *r = &vm->regs[id];
    ptr[0] = r->data[2];
    ptr[1] = r->data[3];
    ptr[2] = r->data[0];
    ptr[3] = r->data[1];
}
void load_result_to_double(simple_dalvik_vm *vm, unsigned char *ptr)
{
    ptr[0] = vm->result[2];
    ptr[1] = vm->result[3];
    ptr[2] = vm->result[0];
    ptr[3] = vm->result[1];
    ptr[4] = vm->result[6];
    ptr[5] = vm->result[7];
    ptr[6] = vm->result[4];
    ptr[7] = vm->result[5];
}

void store_double_to_result(simple_dalvik_vm *vm, unsigned char *ptr)
{
    vm->result[0] = ptr[2];
    vm->result[1] = ptr[3];
    vm->result[2] = ptr[0];
    vm->result[3] = ptr[1];
    vm->result[4] = ptr[6];
    vm->result[5] = ptr[7];
    vm->result[6] = ptr[4];
    vm->result[7] = ptr[5];
}

void store_double_to_reg(simple_dalvik_vm *vm, int id, unsigned char *ptr)
{
    simple_dvm_register *r = &vm->regs[id];
    r->data[0] = ptr[2];
    r->data[1] = ptr[3];
    r->data[2] = ptr[0];
    r->data[3] = ptr[1];
}

void store_to_reg(simple_dalvik_vm *vm, int id, unsigned char *ptr)
{
    simple_dvm_register *r = &vm->regs[id];
    r->data[0] = ptr[0];
    r->data[1] = ptr[1];
    r->data[2] = ptr[2];
    r->data[3] = ptr[3];
}

void move_top_half_result_to_reg(simple_dalvik_vm *vm, int id)
{
    simple_dvm_register *r = &vm->regs[id];
    r->data[0] = vm->result[0];
    r->data[1] = vm->result[1];
    r->data[2] = vm->result[2];
    r->data[3] = vm->result[3];
}

void move_bottom_half_result_to_reg(simple_dalvik_vm *vm, int id)
{
    simple_dvm_register *r = &vm->regs[id];
    r->data[0] = vm->result[4];
    r->data[1] = vm->result[5];
    r->data[2] = vm->result[6];
    r->data[3] = vm->result[7];
}

void move_reg_to_top_half_result(simple_dalvik_vm *vm, int id)
{
    simple_dvm_register *r = &vm->regs[id];
    vm->result[0] = r->data[0];
    vm->result[1] = r->data[1];
    vm->result[2] = r->data[2];
    vm->result[3] = r->data[3];
}

void move_reg_to_bottom_half_result(simple_dalvik_vm *vm, int id)
{
    simple_dvm_register *r = &vm->regs[id];
    vm->result[4] = r->data[0];
    vm->result[5] = r->data[1];
    vm->result[6] = r->data[2];
    vm->result[7] = r->data[3];
}

/*
 * Load the value of "field_name" of the object pointed by register "obj_id" to
 * register "val_id"
 */
void load_field_to(simple_dalvik_vm *vm, int val_id, int obj_id, char *field_name)
{
    instance_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

    load_reg_to(vm, obj_id, (unsigned char *) &obj); 
    for (i = 0; i < obj->field_size; i++)
    {
        if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	} 
    if (!field)
    {
       printf("%s: no field found: %s\n", __FUNCTION__, field_name);
	   return;
    } 
	ptr = (unsigned char *) &field->data;
	store_to_reg(vm, val_id, ptr); 
}

/*
 * Load the wide value of "field_name" of the object pointed by register "obj_id"
 * to register pair of "val_id" & "val_id+1"
 */
void load_field_to_wide(simple_dalvik_vm *vm, int val_id, int obj_id, char *field_name)
{
    instance_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

    load_reg_to(vm, obj_id, (unsigned char *) &obj); 
    for (i = 0; i < obj->field_size; i++)
    {
        if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	} 
    if (!field)
    {
       printf("%s: no field found: %s\n", __FUNCTION__, field_name);
	   return;
    } 
	ptr = (unsigned char *) &field->data;
    store_double_to_reg(vm, val_id, ptr + 4);
    store_double_to_reg(vm, val_id + 1, ptr);
}

class_obj *find_class_obj(simple_dalvik_vm *vm, char *name);
/*
 * Store the value in register "val_id" to "field_name" of the class object
 */
void load_static_field_to(simple_dalvik_vm *vm, int val_id, char *class_name, char *field_name)
{
	class_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

	obj = find_class_obj(vm, class_name);
	if (!obj)
	{
		printf("[%s] No class obj found: %s\n", class_name);
		return;
	}

	for (i = 0; i < obj->field_size; i++)
	{
		if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	}

	if (!field)
	{
		printf("[%s]: no field found: %s\n", __FUNCTION__, field_name);
		return;
	}

	ptr = (unsigned char *) &field->data;
	store_to_reg(vm, val_id, ptr);
}

/*
 * Store the wide value in register pair of "val_id" & "val_id+1" to "field_name" of the class object
 */
void load_static_field_to_wide(simple_dalvik_vm *vm, int val_id, char *class_name, char *field_name)
{
	class_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

	obj = find_class_obj(vm, class_name);
	if (!obj)
	{
		printf("[%s] No class obj found: %s\n", class_name);
		return;
	}

	for (i = 0; i < obj->field_size; i++)
	{
		if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	}

	if (!field)
	{
		printf("[%s]: no field found: %s\n", __FUNCTION__, field_name);
		return;
	}

	ptr = (unsigned char *) &field->data;
	store_double_to_reg(vm, val_id, ptr + 4);
	store_double_to_reg(vm, val_id + 1, ptr);
}

/*
 * Store the value in register "val_id" to "field_name" of the class object
 */
void store_to_static_field(simple_dalvik_vm *vm, int val_id, char *class_name, char *field_name)
{
	class_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

	obj = find_class_obj(vm, class_name);
	if (!obj)
	{
		printf("[%s] No class obj found: %s\n", class_name);
		return;
	}

	for (i = 0; i < obj->field_size; i++)
	{
		if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	}

	if (!field)
	{
		printf("[%s]: no field found: %s\n", __FUNCTION__, field_name);
		return;
	}

	ptr = (unsigned char *) &field->data;
	load_reg_to(vm, val_id, ptr);
}

/*
 * Store the wide value in register pair of "val_id" & "val_id+1" to "field_name" of the class object
 */
void store_to_static_field_wide(simple_dalvik_vm *vm, int val_id, char *class_name, char *field_name)
{
	class_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

	obj = find_class_obj(vm, class_name);
	if (!obj)
	{
		printf("[%s] No class obj found: %s\n", class_name);
		return;
	}

	for (i = 0; i < obj->field_size; i++)
	{
		if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	}

	if (!field)
	{
		printf("[%s]: no field found: %s\n", __FUNCTION__, field_name);
		return;
	}

	ptr = (unsigned char *) &field->data;
	load_reg_to_double(vm, val_id, ptr + 4);
	load_reg_to_double(vm, val_id + 1, ptr);
}

/*
 * Store the value in register "val_id" to "field_name" of the object pointed by
 * register "obj_id"
 */
void store_to_field(simple_dalvik_vm *vm, int val_id, int obj_id, char *field_name)
{
    instance_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

    load_reg_to(vm, obj_id, (unsigned char *) &obj); 
    for (i = 0; i < obj->field_size; i++)
    {
        if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	} 
    if (!field)
    {
       printf("%s: no field found: %s\n", __FUNCTION__, field_name);
	   return;
    } 
	ptr = (unsigned char *) &field->data;
	load_reg_to(vm, val_id, ptr); 
}

/*
 * Store the wide value in register pair of "val_id" & "val_id+1" to "field_name" of the object
 * pointed by register "obj_id"
 */
void store_to_field_wide(simple_dalvik_vm *vm, int val_id, int obj_id, char *field_name)
{
    instance_obj *obj;
	int i;
	obj_field *field = NULL;
	unsigned char *ptr = NULL;

    load_reg_to(vm, obj_id, (unsigned char *) &obj); 
    for (i = 0; i < obj->field_size; i++)
    {
        if (!strncmp(field_name, obj->fields[i].name, strlen(field_name)))
		{
			field = &obj->fields[i];
			break;
		}
	} 
    if (!field)
    {
       printf("%s: no field found: %s\n", __FUNCTION__, field_name);
	   return;
    } 
	ptr = (unsigned char *) &field->data;
    load_reg_to_double(vm, val_id, ptr + 4);
    load_reg_to_double(vm, val_id + 1, ptr);
}

void printStaticFields(class_obj *cls)
{
    int i = 0;
    if (is_verbose()) {
		printf("Class %x of %s, with fields:\n", cls, cls->name);
		for (i = 0; i < cls->field_size; i++)
		{
			printf(".%s:%s: 0x%x\n", cls->fields[i].name, cls->fields[i].type, cls->fields[i].data);
		}
    }
}

void printInsFields(instance_obj *obj)
{
    int i = 0;
    if (is_verbose()) {
		printf("Instance %x of %s, with fields:\n", obj, obj->cls->name);
		for (i = 0; i < obj->field_size; i++)
		{
			printf(".%s:%s: 0x%x\n", obj->fields[i].name, obj->fields[i].type, obj->fields[i].data);
		}
    }
}

void printVTable(class_obj *obj)
{
    int i = 0;
    if (is_verbose()) {
		if (!obj)
			return;
		printf("Virtual table of %s:\n", obj->name);
		for (i = 0; i < obj->vtable_size; i++)
		{
			printf("vtable[%d].name: %s\n"
				   "vtable[%d].method_id: %d\n", 
				   i, obj->vtable[i].name, i, obj->vtable[i].method_id);
		}
    }
}

int cmp_val(int val1, int val2, CMP_TYPE cmp_type)
{ 
	switch (cmp_type)
	{
		case EQ:
			if (val1 == val2)
				return 0;
			break;
		case NE:
			if (val1 != val2)
				return 0;
			break;
		case LT:
			if (val1 < val2)
				return 0;
			break;
		case GE:
			if (val1 >= val2)
				return 0;
			break;
		case GT:
			if (val1 > val2)
				return 0;
			break;
		case LE:
			if (val1 <= val2)
				return 0;
			break;
		default:
			break;
	}
	return 1;
}

int cmp_reg(simple_dalvik_vm *vm, int id1, int id2, CMP_TYPE cmp_type)
{
	int val1;
	int val2;
	load_reg_to(vm, id1, (unsigned char *) &val1);
	load_reg_to(vm, id2, (unsigned char *) &val2);
	return cmp_val(val1, val2, cmp_type);
}

int cmp_reg_z(simple_dalvik_vm *vm, int id, CMP_TYPE cmp_type)
{
	int val;
	load_reg_to(vm, id, (unsigned char *) &val);
	return cmp_val(val, 0, cmp_type);
}

void dump_array(instance_obj *array)
{
	int i;
	array_obj *arr_obj = (array_obj *)array->priv_data;

	printf("array class: %s\n", array->cls->name);
	for (i = 0; i < arr_obj->size; i++)
		printf("[%d]: 0x%x\n", i, arr_obj->ptr[i]);
}


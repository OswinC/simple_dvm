/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2014 Jim Huang <jserv.tw@gmail.com>
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#include "simple_dvm.h"
#include "java_lib.h"

encoded_method *find_method(DexFileFormat *dex, int class_idx, int method_name_idx);
encoded_method *find_method_by_name(DexFileFormat *dex, int class_idx, const char *name);
encoded_method *find_vmethod(DexFileFormat *dex, instance_obj *ins_obj, int class_idx, int method_name_idx);
static int invoke_method(char *name, DexFileFormat *dex, simple_dalvik_vm *vm, method_id_item *m, invoke_parameters *p);
int new_invoke_frame(DexFileFormat *dex, simple_dalvik_vm *vm, encoded_method *m);
void stack_push(simple_dalvik_vm *vm, u4 data);
u4 stack_pop(simple_dalvik_vm *vm);
static int op_utils_invoke_35c_parse(DexFileFormat *dex, u1 *ptr, int *pc,
                                     invoke_parameters *p);

static int find_const_string(DexFileFormat *dex, char *entry)
{
    int i = 0;
    for (i = 0; i < dex->header.stringIdsSize; i++) {
        if (memcmp(dex->string_data_item[i].data, entry, strlen(entry)) == 0) {
            if (is_verbose())
                printf("find %s in dex->string_data_item[%d]\n", entry, i);
            return i;
        }
    }
    return -1;
}

static void printRegs(simple_dalvik_vm *vm)
{
    int i = 0;
    if (is_verbose()) {
        printf("pc = %08x\n", vm->pc);
        for (i = 0; i < 16 ; i++) {
            printf("Reg[%2d] = %4d (%04x) ",
                   i, vm->regs[i], vm->regs[i]);
            if ((i + 1) % 4 == 0) printf("\n");
        }
    }
}

static void op_utils_move(simple_dalvik_vm *vm, int reg_idx_vx, int reg_idx_vy)
{
	unsigned int data;

	if (is_verbose())
		printRegs(vm);

	load_reg_to(vm, reg_idx_vy, (unsigned char *)&data);
	store_to_reg(vm, reg_idx_vx, (unsigned char *)&data);

	if (is_verbose())
		printRegs(vm);
}

/* 0x01, move vx, vy
 *
 * Move data in vy to vx.
 *
 * 0101 - move v1, v0
 * Move data in v0 to v1.
 */
static int op_move(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;

    reg_idx_vx = ptr[*pc + 1] & 0xf;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0xf;

    if (is_verbose())
        printf("move v%d, v%d\n", reg_idx_vx, reg_idx_vy);

    op_utils_move(vm, reg_idx_vx, reg_idx_vy);

    *pc = *pc + 2;
    return 0;
}

/* 0x0a, move-result vx
 *
 * Move the result value of previous method invocation into vx.
 *
 * 0A01 - move-result v1
 * Move the result value of previous method invocation into v1.
 */
static int op_move_result(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    reg_idx_vx = ptr[*pc + 1];
    if (is_verbose())
        printf("move-result v%d\n", reg_idx_vx);
    move_bottom_half_result_to_reg(vm, reg_idx_vx);
    *pc = *pc + 2;
    return 0;
}

/* 0x0b, move-result-wide
 *
 * Move the long/double result value of the previous method invocation
 * into vx, vx+1.
 *
 * 0B02 - move-result-wide v2
 * Move the long/double result value of the previous method
 * invocation into v2,v3.
 */
static int op_move_result_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = reg_idx_vx + 1;
    if (is_verbose())
        printf("move-result-wide v%d,v%d\n", reg_idx_vx, reg_idx_vy);
    move_bottom_half_result_to_reg(vm, reg_idx_vx);
    move_top_half_result_to_reg(vm, reg_idx_vy);
    *pc = *pc + 2;
    return 0;
}

/* 0x0c, move-result-object vx
 *
 * Move the result object reference of
 * the previous method invocation into vx.
 *
 * 0C00 - move-result-object v0
 */
static int op_move_result_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    reg_idx_vx = ptr[*pc + 1];
    if (is_verbose())
        printf("move-result-object v%d\n", reg_idx_vx);
    move_bottom_half_result_to_reg(vm, reg_idx_vx);
    *pc = *pc + 2;
    return 0;
}

/*
 * Pop registers from the stack in vm to restore calling frame,
 * then set the flag returned to be 1 to notify the module running 
 * instructions (e.g., the function runMethod().)
 */
static int op_utils_return(simple_dalvik_vm *vm)
{
	int reg_size;
	int idx;

	/* step 1: Update current sp */
	vm->sp = vm->fp;

	/* step 2: Pop caller registers */
	reg_size = stack_pop(vm);
	for (idx = reg_size - 1; idx >= 0; idx--)
	{
		u4 value;

		value = stack_pop(vm);
		store_to_reg(vm, idx, (u1 *)&value);
	}

	/* step 3: Pop fp & pc */
	vm->fp = (u1 *) stack_pop(vm);
	vm->pc = stack_pop(vm);

	vm->returned = 1;

	return 0;
}

/* 0x0e , return-void
 * Return without a return value
 * 0E00 - return-void
 */
static int op_return_void(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    if (is_verbose())
        printf("return-void\n"); 
    return op_utils_return(vm);
}

/* 0x0f , return
 * Return from a single-width (32-bit) non-object value-returning method
 * 0F01 - return v1
 * Move the value in v1 into result register, then return
 */
static int op_return(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    reg_idx_vx = ptr[*pc + 1];
    if (is_verbose())
		printf("return v%d\n", reg_idx_vx);
	move_reg_to_bottom_half_result(vm, reg_idx_vx); 
    return op_utils_return(vm);
}

/* 0x10 , return-wide
 * Return from a double-width (64-bit) value-returning method 
 * 1001 - return-wide v1
 * Move the value in the register pair of v1 and v2 into result register, then return
 */
static int op_return_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = reg_idx_vx + 1;
    if (is_verbose())
		printf("return-wide v%d\n", reg_idx_vx);
	move_reg_to_bottom_half_result(vm, reg_idx_vx); 
	move_reg_to_top_half_result(vm, reg_idx_vy);
    return op_utils_return(vm);
}

/* 0x11 , return-object
 * Return from a object-returning method
 * 1101 - return-object v1
 * Move the object reference in v1 into result register, then return
 */
static int op_return_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    reg_idx_vx = ptr[*pc + 1];
    if (is_verbose())
		printf("return-object v%d\n", reg_idx_vx);
	move_reg_to_bottom_half_result(vm, reg_idx_vx); 
    return op_utils_return(vm);
}

/* 0x12, const/4 vx,lit4
 * Puts the 4 bit constant into vx
 * 1221 - const/4 v1, #int2
 * Moves literal 2 into v1.
 * The destination register is in the lower 4 bit
 * in the second byte, the literal 2 is in the higher 4 bit.
 */
static int op_const_4(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int value = 0;
    int reg_idx_vx = 0;
    value = ptr[*pc + 1] >> 4;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    if (value & 0x08) {
        value = 0x0F - value + 1;
        value = -value;
    }
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &value);
    if (is_verbose())
        printf("const/4 v%d, #int%d\n", reg_idx_vx , value);
    *pc = *pc + 2;
    return 0;
}

/* 0x13, const/16 vx,lit16
 * Puts the 16 bit constant into vx
 * 1300 0A00 - const/16 v0, #int 10
 * Puts the literal constant of 10 into v0.
 */
static int op_const_16(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int value = 0;
    reg_idx_vx = ptr[*pc + 1];
    value = (ptr[*pc + 3] << 8 | ptr[*pc + 2]);

    store_to_reg(vm, reg_idx_vx, (unsigned char *) &value);
    if (is_verbose())
        printf("const/16 v%d, #int%d\n", reg_idx_vx, value);
    *pc = *pc + 4;
    return 0;
}

/* 0x14, const vx,lit32
 * Puts the 32 bit constant into vx
 * 1400 0A00 0000 - const v0, #int 10
 * Puts the literal constant of 10 into v0.
 */
static int op_const(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int value = 0;
    reg_idx_vx = ptr[*pc + 1];
    value = (ptr[*pc + 5] << 24 | ptr[*pc + 4] << 16 | ptr[*pc + 3] << 8 | ptr[*pc + 2]);

    store_to_reg(vm, reg_idx_vx, (unsigned char *) &value);
    if (is_verbose())
        printf("const v%d, #int%d\n", reg_idx_vx, value);
    *pc = *pc + 6;
    return 0;
}

/* 0x16, const-wide/16 vx,lit16
 * Puts the 16 bit constant into vx and vx+1 registers.
 * Used to initialize wide values.
 * 1601 1000 - const-wide/16 v1, #int 16 // #0010
 * Puts the int constant of 65540 into v1 and v2 register pair.
 */
static int op_const_wide_16(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	long long value = 0;
    int reg_idx_vx = 0;
    unsigned char *ptr_value = (unsigned char *) &value;
    reg_idx_vx = ptr[*pc + 1];
	value = (short) (ptr[*pc + 3] << 8 | ptr[*pc + 2]);
    if (is_verbose())
        printf("const-wide/16 v%d, #int %lld\n", reg_idx_vx, value);
    store_double_to_reg(vm, reg_idx_vx, ptr_value + 4);
    store_double_to_reg(vm, reg_idx_vx + 1, ptr_value);
    *pc = *pc + 4;
    return 0;
}

/* 0x17, const-wide/32 vx,lit32
 * Puts the 32 bit constant into vx and vx+1 registers.
 * Used to initialize wide values.
 * 1701 0400 0100 - const-wide/32 v1, #long 65540 // #00010004
 * Puts the long constant of 65540 into v1 and v2 register pair.
 */
static int op_const_wide_32(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	long long value = 0;
    int reg_idx_vx = 0;
    unsigned char *ptr_value = (unsigned char *) &value;
    reg_idx_vx = ptr[*pc + 1];
	value = (int) (ptr[*pc + 5] << 24 | ptr[*pc + 4] << 16 | ptr[*pc + 3] << 8 | ptr[*pc + 2]);
    if (is_verbose())
        printf("const-wide/32 v%d, #long %lld\n", reg_idx_vx, value);
    store_double_to_reg(vm, reg_idx_vx, ptr_value + 4);
    store_double_to_reg(vm, reg_idx_vx + 1, ptr_value);
    *pc = *pc + 6;
    return 0;
}

/* 0x19, const-wide/high16 vx,lit16
 * Puts the 16 bit constant into the highest 16 bit of vx
 * and vx+1 registers.
 * Used to initialize double values.
 * 1900 2440 - const-wide/high16 v0, #double 10.0 // #4024
 * Puts the double constant of 10.0 into v0 register.
 */
static int op_const_wide_high16(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    long long value = 0L;
    unsigned char *ptr2 = (unsigned char *) &value;
    int reg_idx_vx = 0;
    reg_idx_vx = ptr[*pc + 1];
    ptr2[1] = ptr[*pc + 3];
    ptr2[0] = ptr[*pc + 2];
    if (is_verbose())
        printf("const-wide/hight16 v%d, #long %lld\n", reg_idx_vx, value);
    store_to_reg(vm, reg_idx_vx, ptr2);
    value = 1L;
    *pc = *pc + 4;
    return 0;
}

/* 0x1a, const-string vx,string_id
 * Puts reference to a string constant identified by string_id into vx.
 * 1A08 0000 - const-string v8, "" // string@0000
 * Puts reference to string@0000 (entry #0 in the string table) into v8.
 */
static int op_const_string(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int string_id = 0;
    reg_idx_vx = ptr[*pc + 1];
    string_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    if (is_verbose())
        printf("const-string v%d, string_id 0x%04x\n",
               reg_idx_vx , string_id);
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &string_id);
    *pc = *pc + 4;
    return 0;
}

class_def_item *find_class_def(DexFileFormat *dex, int type_id);
class_data_item *find_class_data(DexFileFormat *dex, int type_id);
class_obj *create_class_obj(simple_dalvik_vm *vm, DexFileFormat *dex, class_def_item *class_def, class_data_item *class_data);
/* 0x1c, const-class vx, type_id
 * Puts reference to a class identified by type_id into vx.
 * 1C08 0000 - const-class v8, LTest // type@0000
 * Puts reference to type@0000 (entry #0 in the string table) into v8.
 */
static int op_const_class(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int type_id = 0;
    class_def_item *class_def;
    class_data_item *class_data;
    class_obj *cls_obj;

    reg_idx_vx = ptr[*pc + 1];
    type_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    if (is_verbose())
        printf("const-class v%d, type_id 0x%04x\n",
               reg_idx_vx , type_id);

    class_data = find_class_data(dex, type_id);
    if (!class_data)
    {
        printf("[%s] No class data found: %s\n", __FUNCTION__, get_type_item_name(dex, type_id));
        return -1;
    }
    class_def = find_class_def(dex, type_id);
    if (!class_def)
    {
        printf("[%s] No class def found: %s\n", __FUNCTION__, get_type_item_name(dex, type_id));
        return -1;
    }

    cls_obj = create_class_obj(vm, dex, class_def, class_data);
    if (!cls_obj)
    {
        printf("cls_obj create fail %s\n", get_type_item_name(dex, type_id));
        return -1;
    }

    store_to_reg(vm, reg_idx_vx, (unsigned char *) &cls_obj);
    *pc = *pc + 4;
    return 0;
}

/*
class_def_item *find_class_def_by_name(DexFileFormat *dex, char *class_name)
{
	int i;
	class_def_item *found = NULL;
	char *name;

	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		class_def_item *item = &dex->class_def_item[i];

		name = get_type_item_name(dex, item->class_idx);
		if (!strcmp(name, class_name))
		{
			found = item;
			break;
		}
	}

	return found;
}
*/

class_def_item *find_class_def(DexFileFormat *dex, int type_id)
{
	int i;
	class_def_item *found = NULL;

	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		class_def_item *item = &dex->class_def_item[i];

		if (item->class_idx == type_id)
		{
			found = item;
			break;
		}
	}

	return found;
}

/*
class_data_item *find_class_data_by_name(DexFileFormat *dex, char *class_name)
{
	int i;
	class_data_item *found = NULL;
	char *name;

	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		class_data_item *class_data = &dex->class_data_item[i];
		class_def_item *item = &dex->class_def_item[i];

		name = get_type_item_name(dex, item->class_idx);
		if (!strcmp(name, class_name))
		{
			found = class_data;
			break;
		}
	}

	return found;
}
*/

class_data_item *find_class_data(DexFileFormat *dex, int type_id)
{
	int i;
	class_data_item *found = NULL;

	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		class_data_item *class_data = &dex->class_data_item[i];
		class_def_item *item = &dex->class_def_item[i];

		if (item->class_idx == type_id)
		{
			found = class_data;
			break;
		}
	}

	return found;
}

class_obj *find_class_obj(simple_dalvik_vm *vm, char *name)
{
	class_obj *obj = NULL, *found = NULL;
	struct list_head *p, *list;

	list = hash_get(&vm->root_set, hash(name));
	if (!list)
		return NULL;

	foreach(p, list)
	{
		obj = (class_obj *) container_of(p, class_obj, class_list);
		if (strncmp(name, obj->name, strlen(name)) == 0)
		{
			found = obj;
			break;
		}
	}

	return found;
}

//int get_type_size(char *type_str)
//{
//	if (!strncmp(type_str, "Z", 1))
//		return 1;
//	if (!strncmp(type_str, "B", 1))
//		return 1;
//	if (!strncmp(type_str, "S", 1))
//		return 2;
//	if (!strncmp(type_str, "C", 1))
//		return 1;
//	if (!strncmp(type_str, "I", 1))
//		return 4;
//	if (!strncmp(type_str, "J", 1))
//		return 4;
//	if (!strncmp(type_str, "F", 1))
//		return 4;
//	if (!strncmp(type_str, "D", 1))
//		return 8;
//	if (!strncmp(type_str, "L", 1))
//		return 4;
//	if (!strncmp(type_str, "[", 1))
//		return 4;
//}

/* This function sholud be called after obj->parent is initialized */
static void fill_vtable(DexFileFormat *dex, class_obj *obj, class_data_item *class_data)
{
	int parent_vtable_size = (obj->parent == NULL) ? 0 : obj->parent->vtable_size;
	int this_vmethods_size = class_data->virtual_methods_size;
	vtable_item *parent_vtable = (obj->parent == NULL) ? NULL : obj->parent->vtable;
	vtable_item **non_overridden_pvtable_entries = NULL;
	int overridden_size = 0;
	int non_overridden_size = 0;
	int i = 0, j = 0;

	if (is_verbose())
		printf("Filling vtable for %s...\n", obj->name);

	/* Step 1: Calculate the vtable size for this class 
	 *         and record those parent vtable entries are not overridden by this class */
	non_overridden_pvtable_entries = malloc(sizeof(vtable_item*) * parent_vtable_size);
	for (i = 0; i < parent_vtable_size; i++)
	{
		int aggregated_idx = 0;
		int tmp_overridden_size = overridden_size;

		for (j = 0; j < this_vmethods_size; j++)
		{
			method_id_item *m;
			char *this_method_name;
			aggregated_idx += class_data->virtual_methods[j].method_idx_diff;
			m = get_method_item(dex, aggregated_idx);
			this_method_name = get_string_data(dex, m->name_idx);
			if (strcmp(parent_vtable[i].name, this_method_name) == 0)
				overridden_size++;
		}
		// If the vtable entry is not overridden
		if (overridden_size == tmp_overridden_size)
			non_overridden_pvtable_entries[non_overridden_size++] = &parent_vtable[i];
	} 
	obj->vtable_size = parent_vtable_size + this_vmethods_size - overridden_size; 
	if (is_verbose())
		printf("vtable_size: %d\n", obj->vtable_size);
	obj->vtable = malloc(sizeof(vtable_item) * obj->vtable_size);

	/* Step 2: Fill the vtable with this class' virtual methods */
	int aggregated_idx = 0;
	for (i = 0; i < this_vmethods_size; i++)
	{
		method_id_item *m;

		aggregated_idx += class_data->virtual_methods[i].method_idx_diff;
		m = get_method_item(dex, aggregated_idx);
		strncpy(obj->vtable[i].name, get_string_data(dex, m->name_idx), sizeof(obj->vtable[i].name));
		obj->vtable[i].method = &class_data->virtual_methods[i];
		obj->vtable[i].method_id = aggregated_idx;
		if (is_verbose())
			printf("vtable[%d].name: %s\n"
				   "vtable[%d].method_id: %d\n", 
				   i, obj->vtable[i].name, i, obj->vtable[i].method_id);
	}

	/* Step 3: Fill the remaining vtable entries with non-overridden virtual methods from the parent class */
	for (; i < obj->vtable_size; i++)
	{
		memcpy(&obj->vtable[i], non_overridden_pvtable_entries[i - this_vmethods_size], sizeof(vtable_item));
		if (is_verbose())
			printf("vtable[%d].name: %s\n"
				   "vtable[%d].method_id: %d\n", 
				   i, obj->vtable[i].name, i, obj->vtable[i].method_id);
	}

	if (non_overridden_pvtable_entries)
		free(non_overridden_pvtable_entries);

	if (is_verbose())
		printf("done.\n");
}

class_obj *create_class_obj(simple_dalvik_vm *vm, DexFileFormat *dex, class_def_item *class_def, class_data_item *class_data)
{
	int i;
	int aggregated_idx = 0;
	class_obj *obj;
	char *name;
	class_obj *parent;
	class_def_item *parent_class_def;
	class_data_item *parent_class_data;
	int parent_type_id;
	char *parent_name;
	method_id_item *method;

	name = get_type_item_name(dex, class_def->class_idx);
	obj = find_class_obj(vm, name);
	if (obj)
		return obj;

	parent_type_id = class_def->superclass_idx;
	parent_name = get_type_item_name(dex, parent_type_id);
	parent_class_def = find_class_def(dex, parent_type_id);
	parent_class_data = find_class_data(dex, parent_type_id);

	if (is_verbose())
		printf("parent: %s\n", parent_name);
	if (strcmp(parent_name, "Ljava/lang/Object;"))
	{
		parent = create_class_obj(vm, dex, parent_class_def, parent_class_data);
		if (!parent)
			return NULL;
	}
	else
	{
		if (is_verbose())
			printf("Reach the Object\n");
		parent = NULL;
	}

	obj = (class_obj*)malloc(sizeof(class_obj) + class_data->static_fields_size * sizeof(obj_field));
	if (!obj)
	{
		printf("alloc class obj fail\n");
		return NULL;
	}

	memset(obj, 0, sizeof(class_obj) + class_data->static_fields_size * sizeof(obj_field));
	obj->parent = parent;
	obj->fields = (obj_field *)((char *)obj + sizeof(class_obj));
	obj->field_size = class_data->static_fields_size;
	strcpy(obj->name, name);
	fill_vtable(dex, obj, class_data);

	for (i = 0; i < class_data->static_fields_size; i++)
	{
		encoded_field *field = &class_data->static_fields[i];
		obj_field *obj_field = &obj->fields[i];
		field_id_item *field_item;
		char *type_str, *name_str;

		aggregated_idx += field->field_idx_diff;
		field_item = get_field_item(dex, aggregated_idx);
		name_str = get_string_data(dex, field_item->name_idx);
		type_str = get_type_item_name(dex, field_item->type_idx);

		strcpy(obj_field->name, name_str);
		strcpy(obj_field->type, type_str);
	}
	// TODO: wrap it to another class_*-series function?
	hash_add(&vm->root_set, &obj->class_list, hash(obj->name));

	// If there is a <clinit>, call it to initialize static fields 
	method = get_method_item_by_name(dex, class_def->class_idx, "<clinit>");
	if (method)
		invoke_method("invoke-direct", dex, vm, method, &vm->p);

	if (is_verbose())
		printf("Class object for %s is created: 0x%08x\n", obj->name, obj);

	return obj;
}

instance_obj *create_instance_obj(DexFileFormat *dex, class_obj *cls, class_def_item *class_def, class_data_item *class_data)
{
	int i, j, k = 0;
	int idx_cls_data = 0;
	int aggregated_idx = 0;
	instance_obj *obj;
	uint fields_size = 0;
	class_def_item *parent_class_def;
	class_data_item *parent_class_data;
	int parent_type_id;
	char *parent_name;
	class_data_item *total_class_data[256];
	char *total_class_name[256];
	class_data_item *cls_data_ptr;

	parent_type_id = class_def->superclass_idx;
	parent_name = get_type_item_name(dex, parent_type_id);
	parent_class_def = find_class_def(dex, parent_type_id);
	parent_class_data = find_class_data(dex, parent_type_id);

	fields_size = class_data->instance_fields_size;
	total_class_data[idx_cls_data] = class_data;
	total_class_name[idx_cls_data] = get_type_item_name(dex, class_def->class_idx);

	while (strcmp(parent_name, "Ljava/lang/Object;"))
	{
		idx_cls_data++;
		fields_size += parent_class_data->instance_fields_size;
		total_class_data[idx_cls_data] = parent_class_data;
		total_class_name[idx_cls_data] = parent_name;

		parent_type_id = parent_class_def->superclass_idx;
		parent_name = get_type_item_name(dex, parent_type_id);
		parent_class_def = find_class_def(dex, parent_type_id);
		parent_class_data = find_class_data(dex, parent_type_id);
	}

	obj = (instance_obj*)malloc(sizeof(instance_obj) + fields_size * sizeof(obj_field));
	if (!obj)
	{
		printf("alloc instance obj fail\n");
		return NULL;
	}

	memset(obj, 0, sizeof(instance_obj) + fields_size * sizeof(obj_field));
	obj->fields = (obj_field *)((char *)obj + sizeof(instance_obj));
	obj->cls = cls;
	obj->field_size = fields_size;

	for (j = 0; j <= idx_cls_data; j++)
	{
		cls_data_ptr = total_class_data[j];
		aggregated_idx = 0;
		for (i = 0; i < cls_data_ptr->instance_fields_size; i++)
		{
			encoded_field *field = &cls_data_ptr->instance_fields[i];
			obj_field *obj_field = &obj->fields[k++];
			field_id_item *field_item;
			char *type_str, *name_str;

			aggregated_idx += field->field_idx_diff;
			field_item = get_field_item(dex, aggregated_idx);
			name_str = get_string_data(dex, field_item->name_idx);
			type_str = get_type_item_name(dex, field_item->type_idx);

			strcpy(obj_field->name, total_class_name[j]);
			strcat(obj_field->name, "."); 
			strcat(obj_field->name, name_str); 
			strcpy(obj_field->type, type_str);
		}
	}

	return obj;
}

instance_obj *new_instance_java_lang_library(simple_dalvik_vm *vm, DexFileFormat *dex, int type_id)
{
	class_obj *cls_obj;
	instance_obj *ins_obj;
	char *name = get_type_item_name(dex, type_id);

	cls_obj = find_class_obj(vm, name);
        if (!cls_obj)
	{
		cls_obj = (class_obj *)malloc(sizeof(class_obj));
		if (!cls_obj)
		{
			printf("[%s] class obj malloc fail\n", __FUNCTION__);
			return NULL;
		}

		memset(cls_obj, 0, sizeof(class_obj));
		strncpy(cls_obj->name, name, strlen(name));
		list_init(&cls_obj->class_list);
		hash_add(&vm->root_set, &cls_obj->class_list, hash(cls_obj->name));
	}

	ins_obj = (instance_obj *)malloc(sizeof(instance_obj));
	if (!ins_obj)
	{
		printf("[%s] class obj malloc fail\n", __FUNCTION__);

		return NULL;
	}

	memset(ins_obj, 0, sizeof(instance_obj));
	ins_obj->cls = cls_obj;

	return ins_obj;
}

static instance_obj *new_array(simple_dalvik_vm *vm, DexFileFormat *dex, int type_id, int size)
{
	class_obj *cls_obj;
	instance_obj *ins_obj;
	array_obj *arr_obj;
	char *name = get_type_item_name(dex, type_id);
	int arr_obj_size;

	cls_obj = find_class_obj(vm, name);
        if (!cls_obj)
	{
		cls_obj = (class_obj *)malloc(sizeof(class_obj));
		if (!cls_obj)
		{
			printf("[%s] class obj malloc fail\n", __FUNCTION__);
			return NULL;
		}

		memset(cls_obj, 0, sizeof(class_obj));
		strncpy(cls_obj->name, name, strlen(name));
		list_init(&cls_obj->class_list);
		hash_add(&vm->root_set, &cls_obj->class_list, hash(cls_obj->name));
	}

	ins_obj = (instance_obj *)malloc(sizeof(instance_obj));
	if (!ins_obj)
	{
		printf("[%s] instance obj malloc fail\n", __FUNCTION__);

		return NULL;
	}

	/* If it's an array of double number, the size should be doubled */
	if (strcmp(name, "[D") == 0)
		size <<= 1;
	arr_obj_size = sizeof(array_obj) + (size - 1) * sizeof(void *);

	arr_obj = (array_obj *)malloc(arr_obj_size);
	if (!arr_obj)
	{
		printf("[%s] array obj malloc fail\n", __FUNCTION__);
		free(ins_obj);

		return NULL;
	}

	memset(ins_obj, 0, sizeof(instance_obj));
	memset(arr_obj, 0, arr_obj_size);
	arr_obj->size = size;
	ins_obj->cls = cls_obj;
	ins_obj->priv_data = (void *)arr_obj;

	return ins_obj;
}

/* 0x23 new-array va, vb, type
 * Instantiates an array of given object type
 * the reference of the newly created array into va
 * 2321 1500 - new-instance v1, v2, java.io.FileInputStream // type@0015
 * Instantiates of array of type@0015 (entry #15H in the type table)
 * and puts its reference into v1.
 */
static int op_new_array(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_va = 0;
    int reg_idx_vb = 0;
    int type_id = 0;
    type_id_item *type_item = 0;
    class_obj *cls_obj;
    instance_obj *ins_obj;
    int size;

    reg_idx_va = ptr[*pc + 1] & 0xf;
    reg_idx_vb = (ptr[*pc + 1] >> 4) & 0xf;
    type_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    type_item = get_type_item(dex, type_id);

    if (is_verbose()) {
        printf("new-array v%d, v%d, type_id 0x%04x", reg_idx_va, reg_idx_vb, type_id);
        if (type_item != 0) {
            printf(" %s", get_string_data(dex, type_item->descriptor_idx));
        }
        printf("\n");
    }

    load_reg_to(vm, reg_idx_vb, (unsigned char *)&size);
    ins_obj = new_array(vm, dex, type_id, size);
    if (!ins_obj)
       return -1;

    store_to_reg(vm, reg_idx_va, (unsigned char *)&ins_obj);

    *pc = *pc + 4;
    return 0;
}

static instance_obj *util_op_filled_new_array(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc,
		invoke_parameters *p)
{
	array_obj *arr_obj;
	instance_obj *ins_obj;
	int size = p->reg_count;
	int i;

	ins_obj = new_array(vm, dex, p->method_id, size);
	if (!ins_obj)
	{
		printf("[%s] array obj malloc failure\n", __FUNCTION__);
		return NULL;
	}
	arr_obj = (array_obj *)ins_obj->priv_data;

	arr_obj->size = size;
	for (i = 0; i < size; i++)
	{
		unsigned int tmp;

		load_reg_to(vm, p->reg_idx[i], (unsigned char *)&tmp);
		arr_obj->ptr[i] = (void *)tmp;
	}

	return ins_obj;
}

/* filled-new-array { parameters } */
/*
 * 2453 0600 0421 - filled-new-array { v4, v0, v1, v2, v3}, [I // type@0006
 * 2420 0200 3200   filled-new-array {v2, v3}, [I // type@0002
 */
static int op_filled_new_array(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    invoke_parameters *p = &vm->p;
    char *type_name;
    char *name = "filled-new-array";
    instance_obj *obj;

    op_utils_invoke_35c_parse(dex, ptr, pc, &vm->p);
    type_name = get_type_item_name(dex, p->method_id);

    switch (p->reg_count) {
        case 0:
            if (is_verbose())
                printf("%s {} type_id 0x%04x", name, p->method_id);
            break;
        case 1:
            if (is_verbose())
                printf("%s, {v%d} type_id 0x%04x",
                       name, p->reg_idx[0], p->method_id);
            break;
        case 2:
            if (is_verbose())
                printf("%s {v%d, v%d} type_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1],
                       p->method_id);
            break;
        case 3:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d} type_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1], p->reg_idx[2],
                       p->method_id);
            break;
        case 4:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d, v%d} type_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1],
                       p->reg_idx[2], p->reg_idx[3],
                       p->method_id);
            break;
        case 5:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d, v%d, v%d} type_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1], p->reg_idx[2],
                       p->reg_idx[3], p->reg_idx[4],
                       p->method_id);
            break;
        default:
            break;
    }

    if (is_verbose())
        printf(", %s\n", type_name);

    obj = util_op_filled_new_array(dex, vm, ptr, pc, p);
    if (!obj)
    {
	printf("[%s] Can't create array object\n", __FUNCTION__);
	return -1;
    }

    store_to_bottom_half_result(vm, (unsigned char *)&obj);

    if (is_verbose())
        dump_array(obj);

    *pc = *pc + 6;
    return 0;
}

/* 0x22 new-instance vx,type
 * Instantiates an object type and puts
 * the reference of the newly created instance into vx
 * 2200 1500 - new-instance v0, java.io.FileInputStream // type@0015
 * Instantiates type@0015 (entry #15H in the type table)
 * and puts its reference into v0.
 */
static int op_new_instance(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int type_id = 0;
    type_id_item *type_item = 0;
    class_data_item *class_data;
    class_def_item *class_def;
    class_obj *cls_obj;
    instance_obj *ins_obj;
    char *type_name;

    reg_idx_vx = ptr[*pc + 1];
    type_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    type_item = get_type_item(dex, type_id);

    if (is_verbose()) {
        printf("new-instance v%d, type_id 0x%04x", reg_idx_vx, type_id);
        if (type_item != 0) {
            printf(" %s", get_string_data(dex, type_item->descriptor_idx));
        }
        printf("\n");
    }

    if (!strncmp("Ljava", get_type_item_name(dex, type_id), strlen("Ljava")))
    {
        ins_obj = new_instance_java_lang_library(vm, dex, type_id);
	if (!ins_obj)
            return -1;
	goto out;
    }

//    store_to_reg(vm, reg_idx_vx, (unsigned char*)&type_id);
    /* TODO */
    class_data = find_class_data(dex, type_id);
    if (!class_data)
    {
        printf("[%s] No class data found: %s\n", __FUNCTION__, get_type_item_name(dex, type_id));
        return -1;
    }
    class_def = find_class_def(dex, type_id);
    if (!class_def)
    {
        printf("[%s] No class def found: %s\n", __FUNCTION__, get_type_item_name(dex, type_id));
        return -1;
    }

	cls_obj = create_class_obj(vm, dex, class_def, class_data);
    if (!cls_obj)
    {
        printf("cls_obj create fail: %s\n", get_string_data(dex, type_item->descriptor_idx));
        return -1;
    }

    ins_obj = create_instance_obj(dex, cls_obj, class_def, class_data);
	printInsFields(ins_obj);
    if (!ins_obj)
    {
        printf("ins_obj create fail: %s\n", get_string_data(dex, type_item->descriptor_idx));
        return -1;
    }

out:
    store_to_reg(vm, reg_idx_vx, (unsigned char *)&ins_obj);

    *pc = *pc + 4;
    return 0;
}

/* 0x28, goto +AA
 *
 * Unconditionally jump to the indicated instruction.
 *
 * 2802 - goto +0x02
 * Jump to pc + 0x02
 */
static int op_goto(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int offset = 0;
    offset = (signed char) ptr[*pc + 1];
    if (is_verbose())
        printf("goto +0x%02x\n", offset);
    *pc = *pc + offset * 2;
    return 0;
}

/* 0x29, goto/16 +AAAA
 *
 * Unconditionally jump to the indicated instruction.
 *
 * 2900 1234 - goto/16 +0x3412
 * Jump to pc + 0x3412
 */
static int op_goto_16(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int offset = 0;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("goto +0x%04x\n", offset);
    *pc = *pc + offset * 2;
    return 0;
}

/* 0x2a, goto/32 +AAAAAAAA
 *
 * Unconditionally jump to the indicated instruction.
 *
 * 2900 1234 5678  - goto/32 +0x78563412
 * Jump to pc + 0x78563412
 */
static int op_goto_32(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int offset = 0;
    offset = (signed int) ((ptr[*pc + 5] << 24) | (ptr[*pc + 4] << 16) | (ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("goto +0x%08x\n", offset);
    *pc = *pc + offset * 2;
    return 0;
}

/* 0x2b, packed-switch vAA, +BBBBBBBB
 *
 * Jump to a new instruction based on the value in the given register,
 * using a table of offsets corresponding to each value in a particular
 * integral range, or fall through to the next instruction if there is
 * no match.
 *
 * 2b01 1234 5678  - packed-switch v1, +0x78563412
 * Conditionally jump according to the comparison of value in v1 and
 * switch table at pc + 0x78563412, which should fit with the format:
 *
 * Name         Format          Description
 * ident        ushort = 0x0100 identifying pseudo-opcode
 * size         ushort          number of entries in the table
 * first_key    int             first (and lowest) switch case value
 * targets      int[]           list of size relative branch targets.
 *                              The targets are relative to the address
 *                              of the switch opcode, not of this table.
 */
static int op_packed_switch(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    int offset_word = 0;
	int value;
	ushort ident;
	ushort size;
	int first_key;
	int *targets;

    reg_idx_vx = ptr[*pc + 1];
	offset = (signed int) ((ptr[*pc + 5] << 24) | (ptr[*pc + 4] << 16) | (ptr[*pc + 3] << 8) | ptr[*pc + 2]);
	offset_word = offset * 2;
    if (is_verbose())
        printf("packed-switch v%d, +0x%08x\n", reg_idx_vx, offset);

	load_reg_to(vm, reg_idx_vx, (unsigned char *)&value);
	ident = (ushort) ((ptr[*pc + offset_word + 1] << 8) | ptr[*pc + offset_word]);
	size = (ushort) ((ptr[*pc + offset_word + 3] << 8) | ptr[*pc + offset_word + 2]);
	first_key = (int) ((ptr[*pc + offset_word + 7] << 24) | (ptr[*pc + offset_word + 6] << 16) |
						(ptr[*pc + offset_word + 5] << 8) | ptr[*pc + offset_word + 4]);
	targets = (int *) (ptr + *pc + offset_word + 8);

	if (ident != 0x0100)
		printf("Error: invalid ident for packed switch payload: %04x!\n", ident);

	// if value is in the range of this packed switch table
	if (value >= first_key || value < first_key + size)
		*pc = *pc + targets[value - first_key] * 2;
	else
		*pc = *pc + 6;

    return 0;
}

/* 0x32, if-eq vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as equal
 *
 * 3201 1234 - if-eq v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is equal to v0
 */
static int op_if_eq(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-eq v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, EQ) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x33, if-ne vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as not equal
 *
 * 3301 1234 - if-ne v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is not equal to v0
 */
static int op_if_ne(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-ne v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, NE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x34, if-lt vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as less than
 *
 * 3401 1234 - if-lt v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is less than v0
 */
static int op_if_lt(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-lt v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, LT) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x35, if-ge vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as greater or equal
 *
 * 3501 1234 - if-ge v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is greater than or equal to v0
 */
static int op_if_ge(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-ge v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, GE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x36, if-gt vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as greater than
 *
 * 3601 1234 - if-gt v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is greater than v0
 */
static int op_if_gt(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-gt v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, GT) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x37, if-le vA, vB, +CCCC
 *
 * Branch to the given destination if the given two registers' values compare as less or equal
 *
 * 3701 1234 - if-le v1, v0, +0x3412
 * Jump to pc + 0x3412 if v1 is less than or equal to v0
 */
static int op_if_le(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-le v%d, v%d, +0x%04x\n", reg_idx_vx, reg_idx_vy, offset);
	if (cmp_reg(vm, reg_idx_vx, reg_idx_vy, LE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x38, if-eqz vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as equal
 *
 * 3801 1234 - if-eqz v1, +0x3412
 * Jump to pc + 0x3412 if v1 is equal to 0
 */
static int op_if_eqz(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-eqz v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, EQ) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x39, if-nez vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as not equal
 *
 * 3901 1234 - if-nez v1, +0x3412
 * Jump to pc + 0x3412 if v1 is not equal to 0
 */
static int op_if_nez(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-nez v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, NE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x3a, if-ltz vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as less than
 *
 * 3a01 1234 - if-ltz v1, +0x3412
 * Jump to pc + 0x3412 if v1 is less than 0
 */
static int op_if_ltz(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-ltz v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, LT) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x3b, if-gez vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as greater or equal
 *
 * 3b01 1234 - if-gez v1, +0x3412
 * Jump to pc + 0x3412 if v1 is greater than or equal to 0
 */
static int op_if_gez(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-gez v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, GE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x3c, if-gtz vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as greater than
 *
 * 3c01 1234 - if-gtz v1, +0x3412
 * Jump to pc + 0x3412 if v1 is greater than 0
 */
static int op_if_gtz(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-gtz v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, GT) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 0x3d, if-lez vAA, +BBBB
 *
 * Branch to the given destination if the given register's value compares with 0 as less or equal
 *
 * 3d01 1234 - if-lez v1, +0x3412
 * Jump to pc + 0x3412 if v1 is less than or equal to 0
 */
static int op_if_lez(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int offset = 0;
    reg_idx_vx = ptr[*pc + 1];
    offset = (signed short) ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);
    if (is_verbose())
        printf("if-lez v%d, +0x%04x\n", reg_idx_vx, offset);
	if (cmp_reg_z(vm, reg_idx_vx, LE) == 0)
		*pc = *pc + offset * 2;
	else
		*pc = *pc + 4;
    return 0;
}

/* 35c format
 * A|G|op BBBB F|E|D|C
 * [A=5] op {vC, vD, vE, vF, vG}, meth@BBBB
 * [A=5] op {vC, vD, vE, vF, vG}, type@BBBB
 * [A=4] op {vC, vD, vE, vF}, kind@BBBB
 * [A=3] op {vC, vD, vE}, kind@BBBB
 * [A=2] op {vC, vD}, kind@BBBB
 * [A=1] op {vC}, kind@BBBB
 * [A=0] op {}, kind@BBBB
 * The unusual choice in lettering here reflects a desire to
 * make the count and the reference index have the same label as in format 3rc.
 */
static int op_utils_invoke_35c_parse(DexFileFormat *dex, u1 *ptr, int *pc,
                                     invoke_parameters *p)
{
    unsigned char tmp = 0;
    int i = 0;
    if (dex != 0 && ptr != 0 && p != 0) {
        memset(p, 0, sizeof(invoke_parameters));

        tmp = ptr[*pc + 1];
        p->reg_count = tmp >> 4;
        p->reg_idx[4] = tmp & 0x0F;

        p->method_id = ptr[*pc + 2];
        p->method_id |= (ptr[*pc + 3] << 8);

        tmp = ptr[*pc + 4];
        p->reg_idx[1] = tmp >> 4;
        p->reg_idx[0] = tmp & 0x0F;

        tmp = ptr[*pc + 5];
        p->reg_idx[3] = tmp >> 4;
        p->reg_idx[2] = tmp & 0x0F;
    }
    return 0;
}

static int invoke_method(char *name, DexFileFormat *dex, simple_dalvik_vm *vm,
		method_id_item *m, invoke_parameters *p)
{
	encoded_method *method;
	int ins_size;
	int reg_size;
	int target_idx, i;
	u4 values[32];
	u4 tmp;

	if (!strcmp(name, "invoke-virtual"))
	{
		instance_obj *ins_obj;

		load_reg_to(vm, p->reg_idx[0], (unsigned char *)&ins_obj);
		method = find_vmethod(dex, ins_obj, (int)m->class_idx, (int)m->name_idx);
	}
//	else if (strcmp(name, "invoke-static"))
		// FIXME
	else
	{
		method = find_method(dex, (int)m->class_idx, (int)m->name_idx);
	}

	if (!method)
	{
		char *class_name = get_type_item_name(dex, m->class_idx);

		if (strcmp("Ljava/lang/Object;", class_name))
		{
			printf("%s: no method found: %s.%s\n", __FUNCTION__,
					class_name, get_string_data(dex, m->name_idx));
		}

		return 0;
	}

	if (is_verbose())
		printRegs(vm);

	if (new_invoke_frame(dex, vm, method))
	{
		printf("new frame fail\n");
		return 0;
	}

	ins_size = method->code_item.ins_size;
	reg_size = method->code_item.registers_size;
	target_idx = reg_size - 1;

	/*
	 * Load register contents to internal storage
	 */
	for (i = ins_size - 1; i >= 0; i--, target_idx--)
	{
		if (is_verbose())
			printf("reg %d --> %d\n", p->reg_idx[i], target_idx);
		load_reg_to(vm, p->reg_idx[i], (u1 *)&values[target_idx]);
	}

	/*
	 * Store contents to destination registers
	 */
	target_idx = reg_size - 1;
	for (i = ins_size - 1; i >= 0; i--, target_idx--)
		store_to_reg(vm, target_idx, (u1 *)&values[target_idx]);

	/*
	 * Initialize other registers with 0
	 */
	tmp = 0;
	for (i = target_idx; i >= 0; i--)
	{
		store_to_reg(vm, i, (u1 *)&tmp);
	}

	if (is_verbose())
		printRegs(vm);

	vm->pc = 0;
	runMethod(dex, vm, method);

	return 1;
}

static int op_utils_invoke(char *name, DexFileFormat *dex, simple_dalvik_vm *vm,
                           invoke_parameters *p)
{
    method_id_item *m = 0;
    type_id_item *type_class = 0;
    proto_id_item *proto_item = 0;
    type_list *proto_type_list = 0;
    if (p != 0) {
        m = get_method_item(dex, p->method_id);
        if (m != 0) {
            type_class = get_type_item(dex, m->class_idx);
            proto_item = get_proto_item(dex, m->proto_idx);
        }
        switch (p->reg_count) {
        case 0:
            if (is_verbose())
                printf("%s {} method_id 0x%04x", name, p->method_id);
            break;
        case 1:
            if (is_verbose())
                printf("%s, {v%d} method_id 0x%04x",
                       name, p->reg_idx[0], p->method_id);
            break;
        case 2:
            if (is_verbose())
                printf("%s {v%d, v%d} method_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1],
                       p->method_id);
            break;
        case 3:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d} method_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1], p->reg_idx[2],
                       p->method_id);
            break;
        case 4:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d, v%d} method_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1],
                       p->reg_idx[2], p->reg_idx[3],
                       p->method_id);
            break;
        case 5:
            if (is_verbose())
                printf("%s {v%d, v%d, v%d, v%d, v%d} method_id 0x%04x",
                       name,
                       p->reg_idx[0], p->reg_idx[1], p->reg_idx[2],
                       p->reg_idx[3], p->reg_idx[4],
                       p->method_id);
            break;
        default:
            break;
        }

        if (m != 0 && type_class != 0 && p->reg_count <= 5) {
            if (proto_item != 0)
                proto_type_list = get_proto_type_list(dex, m->proto_idx);
            if (proto_type_list != 0 && proto_type_list->size > 0) {
                if (is_verbose())
                    printf(" %s,%s,(%s)%s \n",
                           get_string_data(dex, type_class->descriptor_idx),
                           get_string_data(dex, m->name_idx),
                           get_type_item_name(dex,
                                              proto_type_list->type_item[0].type_idx),
                           get_type_item_name(dex,
                                              proto_item->return_type_idx));
                if (invoke_java_lang_library(dex, vm,
                                         get_string_data(dex, type_class->descriptor_idx),
                                         get_string_data(dex, m->name_idx),
                                         get_type_item_name(dex, proto_type_list->type_item[0].type_idx)))
			goto out;
		if (invoke_method(name, dex, vm, m, p))
			goto out;
            } else {
                if (is_verbose())
                    printf(" %s,%s,()%s \n",
                           get_string_data(dex, type_class->descriptor_idx),
                           get_string_data(dex, m->name_idx),
                           get_type_item_name(dex,
                                              proto_item->return_type_idx));
                if (invoke_java_lang_library(dex, vm,
                                         get_string_data(dex, type_class->descriptor_idx),
                                         get_string_data(dex, m->name_idx), 0))
			goto out;
		if (invoke_method(name, dex, vm, m, p))
			goto out;
            }

        } else {
            if (is_verbose())
                printf("\n");
        }
    }

out:
    return 0;
}


/* invoke-virtual { parameters }, methodtocall */
/*
 * 6E53 0600 0421 - invoke-virtual { v4, v0, v1, v2, v3}, Test2.method5:(IIII)V // method@0006
 * 6e20 0200 3200   invoke-virtual {v2, v3}, Ljava/io/PrintStream;.println:(Ljava/lang/String;)V // method@0002
 */
static int op_invoke_virtual(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int string_id = 0;

    op_utils_invoke_35c_parse(dex, ptr, pc, &vm->p);
    op_utils_invoke("invoke-virtual", dex, vm, &vm->p);
    /* TODO */
    *pc = *pc + 6;
    return 0;
}

/* invoke-direct
 * 7010 0400 0300  invoke-direct {v3}, Ljava/lang/StringBuilder;.<init>:()V // method@0004
 */
static int op_invoke_direct(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    invoke_parameters p;
    int string_id = 0;

    op_utils_invoke_35c_parse(dex, ptr, pc, &vm->p);
    op_utils_invoke("invoke-direct", dex, vm, &vm->p);
    /* TODO */
    *pc = *pc + 6;
    return 0;
}

/* 0x71 invoke-direct
 * 7100 0300 0000  invoke-static {}, Ljava/lang/Math;.random:()D // method@0003
 */
static int op_invoke_static(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    invoke_parameters p;
    int string_id = 0;

    op_utils_invoke_35c_parse(dex, ptr, pc, &vm->p);
    op_utils_invoke("invoke-static", dex, vm, &vm->p);
    /* TODO */
    *pc = *pc + 6;
    return 0;
}

/*
 * 23x family aget operation for 4-byte long data
 */
static int op_utils_aget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc, char *op_name)
{
	int reg_idx_va = 0;
	int reg_idx_vb = 0;
	int reg_idx_vc = 0;
	int size;
	int idx;
	instance_obj *arr_ins_obj;
	array_obj *arr_obj;
	unsigned int data;

	reg_idx_va = ptr[*pc + 1];
	reg_idx_vb = ptr[*pc + 2];
	reg_idx_vc = ptr[*pc + 3];

	if (is_verbose()) {
		printf("%s v%d, v%d, v%d", op_name, reg_idx_va, reg_idx_vb, reg_idx_vc);
		printf("\n");
	}

	load_reg_to(vm, reg_idx_vb, (unsigned char *)&arr_ins_obj);
	load_reg_to(vm, reg_idx_vc, (unsigned char *)&idx);

	arr_obj = (array_obj *)arr_ins_obj->priv_data;
	if (idx >= arr_obj->size)
	{
		printf("[%s] Out of boundary in array %s: size: %d, idx: %d\n", __FUNCTION__,
			arr_ins_obj->cls->name, arr_obj->size, idx);
		return -1;
	}

	data = (unsigned int)arr_obj->ptr[idx];

	store_to_reg(vm, reg_idx_va, (unsigned char *)&data);

	if (is_verbose())
		printRegs(vm);

	*pc = *pc + 4;
	return 0;
}

/*
 * 23x family aget operation for 8-byte long data
 */
static int op_utils_aget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc, char *op_name)
{
	int reg_idx_va = 0;
	int reg_idx_vb = 0;
	int reg_idx_vc = 0;
	int size;
	int idx;
	instance_obj *arr_ins_obj;
	array_obj *arr_obj;
	unsigned int data[2];

	reg_idx_va = ptr[*pc + 1];
	reg_idx_vb = ptr[*pc + 2];
	reg_idx_vc = ptr[*pc + 3];

	if (is_verbose()) {
		printf("%s v%d, v%d, v%d", op_name, reg_idx_va, reg_idx_vb, reg_idx_vc);
		printf("\n");
	}

	load_reg_to(vm, reg_idx_vb, (unsigned char *)&arr_ins_obj);
	load_reg_to(vm, reg_idx_vc, (unsigned char *)&idx);

	arr_obj = (array_obj *)arr_ins_obj->priv_data;
	idx *= 2;
	if (idx >= arr_obj->size)
	{
		printf("[%s] Out of boundary in array %s: size: %d, idx: %d\n", __FUNCTION__,
			arr_ins_obj->cls->name, arr_obj->size, idx);
		return -1;
	}

	data[0] = (unsigned int)arr_obj->ptr[idx];
	data[1] = (unsigned int)arr_obj->ptr[idx + 1];

	store_double_to_reg(vm, reg_idx_va, (unsigned char *)&data[1]);
	store_double_to_reg(vm, reg_idx_va + 1, (unsigned char *)&data[0]);

	if (is_verbose())
		printRegs(vm);

	*pc = *pc + 4;
	return 0;
}

/*
 * 23x family aput operation for 4-byte long data
 */
static int op_utils_aput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc, char *op_name)
{
	int reg_idx_va = 0;
	int reg_idx_vb = 0;
	int reg_idx_vc = 0;
	int size;
	int idx;
	instance_obj *arr_ins_obj;
	array_obj *arr_obj;
	unsigned int data;

	reg_idx_va = ptr[*pc + 1];
	reg_idx_vb = ptr[*pc + 2];
	reg_idx_vc = ptr[*pc + 3];

	if (is_verbose()) {
		printf("%s v%d, v%d, v%d", op_name, reg_idx_va, reg_idx_vb, reg_idx_vc);
		printf("\n");
	}

	load_reg_to(vm, reg_idx_va, (unsigned char *)&data);
	load_reg_to(vm, reg_idx_vb, (unsigned char *)&arr_ins_obj);
	load_reg_to(vm, reg_idx_vc, (unsigned char *)&idx);

	arr_obj = (array_obj *)arr_ins_obj->priv_data;
	if (idx >= arr_obj->size)
	{
		printf("[%s] Out of boundary in array %s: size: %d, idx: %d\n", __FUNCTION__,
			arr_ins_obj->cls->name, arr_obj->size, idx);
		return -1;
	}

	arr_obj->ptr[idx] = (void *)data;

	if (is_verbose())
		dump_array(arr_ins_obj);

	*pc = *pc + 4;
	return 0;
}

/*
 * 23x family aput operation for 8-byte long data
 */
static int op_utils_aput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc, char *op_name)
{
	int reg_idx_va = 0;
	int reg_idx_vb = 0;
	int reg_idx_vc = 0;
	int size;
	int idx;
	instance_obj *arr_ins_obj;
	array_obj *arr_obj;
	unsigned int data[2];

	reg_idx_va = ptr[*pc + 1];
	reg_idx_vb = ptr[*pc + 2];
	reg_idx_vc = ptr[*pc + 3];

	if (is_verbose()) {
		printf("%s v%d, v%d, v%d", op_name, reg_idx_va, reg_idx_vb, reg_idx_vc);
		printf("\n");
	}

	load_reg_to_double(vm, reg_idx_va, (unsigned char *)&data[1]);
	load_reg_to_double(vm, reg_idx_va + 1, (unsigned char *)&data[0]);
	load_reg_to(vm, reg_idx_vb, (unsigned char *)&arr_ins_obj);
	load_reg_to(vm, reg_idx_vc, (unsigned char *)&idx);

	arr_obj = (array_obj *)arr_ins_obj->priv_data;
	idx *= 2;
	if (idx >= arr_obj->size)
	{
		printf("[%s] Out of boundary in array %s: size: %d, idx: %d\n", __FUNCTION__,
			arr_ins_obj->cls->name, arr_obj->size, idx);
		return -1;
	}

	arr_obj->ptr[idx] = (void *)data[0];
	arr_obj->ptr[idx + 1] = (void *)data[1];

	if (is_verbose())
		dump_array_wide(arr_ins_obj);

	*pc = *pc + 4;
	return 0;
}

/* 0x44 aget va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4402 0100 - aput v2, v1, v0
 */
static int op_aget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget");
}

/* 0x45 aget-wide va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va,va + 1
 * 4502 0100 - aput v2, v1, v0
 */
static int op_aget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget_wide(dex, vm, ptr, pc, "aget-wide");
}

/* 0x46 aget-object va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4602 0100 - aput v2, v1, v0
 */
static int op_aget_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget-object");
}

/* 0x47 aget-boolean va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4702 0100 - aput v2, v1, v0
 */
static int op_aget_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget-boolean");
}

/* 0x48 aget-byte va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4802 0100 - aput v2, v1, v0
 */
static int op_aget_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget-byte");
}

/* 0x49 aget-char va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4902 0100 - aput v2, v1, v0
 */
static int op_aget_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget-char");
}

/* 0x4a aget-short va, vb, vc
 * Load data from the element of an array pointed by vb,
 * which is indexed by vc, to va
 * 4a02 0100 - aput v2, v1, v0
 */
static int op_aget_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aget(dex, vm, ptr, pc, "aget-short");
}

/* 0x4b aput va, vb, vc
 * Store data in va to the element of an array pointed by vb,
 * which is indexed by vc
 * 4b02 0100 - aput v2, v1, v0
 */
static int op_aput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput");
}

/* 0x4c aput-wide va, vb, vc
 * Store data in va & va + 1 to the element of an array pointed by vb,
 * which is indexed by vc
 * 4c02 0100 - aput-object v2, v1, v0
 */
static int op_aput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput_wide(dex, vm, ptr, pc, "aput-wide");
}

/* 0x4d aput-object va, vb, vc
 * Store an object reference pointed by va in the element of an array pointed by vb,
 * which is indexed by vc
 * 4d02 0100 - aput-object v2, v1, v0
 */
static int op_aput_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput-object");
}

/* 0x4e aput-boolean va, vb, vc
 * Store boolean data in va to the element of an array pointed by vb,
 * which is indexed by vc
 * 4e02 0100 - aput-object v2, v1, v0
 */
static int op_aput_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput-boolean");
}

/* 0x4f aput-byte va, vb, vc
 * Store byte data in va to the element of an array pointed by vb,
 * which is indexed by vc
 * 4f02 0100 - aput-object v2, v1, v0
 */
static int op_aput_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput-byte");
}

/* 0x50 aput-char va, vb, vc
 * Store character data in va to the element of an array pointed by vb,
 * which is indexed by vc
 * 5002 0100 - aput-object v2, v1, v0
 */
static int op_aput_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput-char");
}

/* 0x51 aput-short va, vb, vc
 * Store short data in va to the element of an array pointed by vb,
 * which is indexed by vc
 * 5102 0100 - aput-object v2, v1, v0
 */
static int op_aput_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	return op_utils_aput(dex, vm, ptr, pc, "aput-short");
}

/*
 * 22c family iget operation for 4-byte long data
 */
static int op_utils_iget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int reg_idx_vb = 0;
	char full_field_name[255];
    int i;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1] & 0xf;
    reg_idx_vb = (ptr[*pc + 1] >> 4) & 0xf;
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

	gen_full_field_name(full_field_name, get_field_class_name(dex, field_id), get_field_item_name(dex, field_id));

    if (is_verbose()) {
        printf("op_utils_iget v%d, v%d, field 0x%04x (%s)\n", reg_idx_va, reg_idx_vb, field_id, full_field_name);
    }

    load_field_to(vm, reg_idx_va, reg_idx_vb, full_field_name); 

    return 0;
}

/*
 * 22c family iget operation for 8-byte long data
 */
static int op_utils_iget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int reg_idx_vb = 0;
    char full_field_name[256];
    int i;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1] & 0xf;
    reg_idx_vb = (ptr[*pc + 1] >> 4) & 0xf;
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

	gen_full_field_name(full_field_name, get_field_class_name(dex, field_id), get_field_item_name(dex, field_id));

    if (is_verbose()) {
        printf("op_utils_iget_wide v%d, v%d, field 0x%04x (%s)\n", reg_idx_va, reg_idx_vb, field_id, full_field_name);
    }

    load_field_to_wide(vm, reg_idx_va, reg_idx_vb, full_field_name); 

    return 0;
}

/* 0x52 iget va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5210 0000 - iget v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x53 iget-wide va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to register pair of va and va+1.
 * 5320 0000 - iget-wide v0, v2, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v2 to v0 and v1.
 */
static int op_iget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget_wide(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x54 iget-object va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5410 0000 - iget-object v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x55 iget-boolean va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5510 0000 - iget-boolean v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x56 iget-byte va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5610 0000 - iget-byte v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x57 iget-char va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5710 0000 - iget-char v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x58 iget-short va, vb, field_id
 * Loads the field of the object referenced in vb identified by field_id to va.
 * 5810 0000 - iget-char v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Loads field@0000 (entry #0H in the field id table) of v1 to v0.
 */
static int op_iget_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iget(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/*
 * 21c family sget operation for 4-byte long data
 */
static int op_utils_sget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int i;
    char *class_name, *field_name;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1];
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    class_name = get_field_class_name(dex, field_id);
    field_name = get_field_item_name(dex, field_id);

    if (is_verbose()) {
        printf("op_utils_sget v%d, field 0x%04x (%s.%s)\n", reg_idx_va, field_id, class_name, field_name);
    }

    if (strncmp(class_name, "Ljava", strlen("Ljava")) == 0)
	    return 0;

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printRegs(vm);
    }

    load_static_field_to(vm, reg_idx_va, class_name, field_name);

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printRegs(vm);
    }

    return 0;
}

/*
 * 21c family sget operation for 4-byte long data
 */
static int op_utils_sget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int i;
    char *class_name, *field_name;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1];
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    class_name = get_field_class_name(dex, field_id);
    field_name = get_field_item_name(dex, field_id);

    if (is_verbose()) {
        printf("op_utils_sget_wide v%d, field 0x%04x (%s.%s)\n", reg_idx_va, field_id, class_name, field_name);
    }

    if (strncmp(class_name, "Ljava", strlen("Ljava")) == 0)
	    return 0;

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printRegs(vm);
    }

    load_static_field_to_wide(vm, reg_idx_va, class_name, field_name);

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printRegs(vm);
    }

    return 0;
}

/*
 * 21c family sput operation for 4-byte long data
 */
static int op_utils_sput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int i;
    char *class_name, *field_name;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1];
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    class_name = get_field_class_name(dex, field_id);
    field_name = get_field_item_name(dex, field_id);

    if (is_verbose()) {
        printf("op_utils_sput v%d, field 0x%04x (%s.%s)\n", reg_idx_va, field_id, class_name, field_name);
    }

    if (strncmp(class_name, "Ljava", strlen("Ljava")) == 0)
	    return 0;

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printStaticFields(obj);
    }

    store_to_static_field(vm, reg_idx_va, class_name, field_name);

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printStaticFields(obj);
    }

    return 0;
}

/*
 * 21c family sput operation for 4-byte long data
 */
static int op_utils_sput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int i;
    char *class_name, *field_name;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1];
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

    class_name = get_field_class_name(dex, field_id);
    field_name = get_field_item_name(dex, field_id);

    if (is_verbose()) {
        printf("op_utils_sput v%d, field 0x%04x (%s.%s)\n", reg_idx_va, field_id, class_name, field_name);
    }

    if (strncmp(class_name, "Ljava", strlen("Ljava")) == 0)
	    return 0;

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printStaticFields(obj);
    }

    store_to_static_field_wide(vm, reg_idx_va, class_name, field_name);

    if (is_verbose())
    {
	    class_obj *obj;

	    obj = find_class_obj(vm, class_name);
	    printStaticFields(obj);
    }

    return 0;
}

/*
 * 22c family iput operation for 4-byte long data
 */
static int op_utils_iput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int reg_idx_vb = 0;
	char full_field_name[255];
    int i;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1] & 0xf;
    reg_idx_vb = (ptr[*pc + 1] >> 4) & 0xf;
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

	gen_full_field_name(full_field_name, get_field_class_name(dex, field_id), get_field_item_name(dex, field_id));

    if (is_verbose()) {
        printf("op_utils_iput v%d, v%d, field 0x%04x (%s)\n", reg_idx_va, reg_idx_vb, field_id, full_field_name);
    }

	instance_obj *obj; 
    load_reg_to(vm, reg_idx_vb, (unsigned char *) &obj); 
	printInsFields(obj);
    store_to_field(vm, reg_idx_va, reg_idx_vb, full_field_name); 
	printInsFields(obj);

    return 0;
}

/*
 * 22c family iput operation for 8-byte long data
 */
static int op_utils_iput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int field_id = 0;
    int reg_idx_va = 0;
    int reg_idx_vb = 0;
    char full_field_name[256];
    int i;
    obj_field *found = NULL;

    reg_idx_va = ptr[*pc + 1] & 0xf;
    reg_idx_vb = (ptr[*pc + 1] >> 4) & 0xf;
    field_id = ((ptr[*pc + 3] << 8) | ptr[*pc + 2]);

	gen_full_field_name(full_field_name, get_field_class_name(dex, field_id), get_field_item_name(dex, field_id));

    if (is_verbose()) {
        printf("op_utils_iput_wide v%d, v%d, field 0x%04x (%s)\n", reg_idx_va, reg_idx_vb, field_id, full_field_name);
    }

    store_to_field_wide(vm, reg_idx_va, reg_idx_vb, full_field_name); 

    return 0;
}

/* 0x59 iput va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5910 0000 - iput v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5a iput-wide va, vb, field_id
 * Stores the value pair in va and va+1 to the field of the object referenced in vb identified by field_id.
 * 5a20 0000 - iput-wide v0, v2, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v2.
 */
static int op_iput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput_wide(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5b iput-object va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5b10 0000 - iput-object v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5c iput-boolean va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5c10 0000 - iput-boolean v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5d iput-byte va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5d10 0000 - iput-byte v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5e iput-char va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5e10 0000 - iput-char v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x5f iput-short va, vb, field_id
 * Stores the value in va to the field of the object referenced in vb identified by field_id.
 * 5f10 0000 - iput-short v0, v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Stores v0 to field@0000 (entry #0H in the field id table) of v1.
 */
static int op_iput_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_iput(dex, vm, ptr, pc);
out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x60 sget vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6001 0C00 - sget v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x61 sget-wide vx,field_id
 * Reads the object reference field identified by the field_id into vx & vx + 1.
 * 6101 0C00 - sget-wide v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1 & v2.
 */
static int op_sget_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget_wide(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x62 sget-object vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6201 0C00 - sget-object v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x63 sget-boolean vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6301 0C00 - sget-boolean v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x64 sget-byte vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6401 0C00 - sget-byte v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x65 sget-char vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6501 0C00 - sget-char v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x66 sget-short vx,field_id
 * Reads the object reference field identified by the field_id into vx.
 * 6601 0C00 - sget-short v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Reads field@000c (entry #CH in the field id table) into v1.
 */
static int op_sget_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sget(dex, vm, ptr, pc);

    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x67 sput vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x68 sput-wide vx,field_id
 * Store vx & vx +1 to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 & v2 to field@000c (entry #CH in the field id table).
 */
static int op_sput_wide(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput_wide(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x69 sput-object vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput_object(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x6a sput-boolean vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput_boolean(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x6b sput-byte vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput_byte(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x6c sput-char vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput_char(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x6d sput-short vx,field_id
 * Store vx to the object reference field identified by the field_id.
 * 6701 0C00 - sput v1, Test3.os1:Ljava/lang/Object; // field@000c
 * Store v1 to field@000c (entry #CH in the field id table).
 */
static int op_sput_short(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
	op_utils_sput(dex, vm, ptr, pc);

out:
    /* TODO */
    *pc = *pc + 4;
    return 0;
}

/* 0x90 add-int vx,vy vz
 * Calculates vy+vz and puts the result into vx.
 * 9000 0203 - add-int v0, v2, v3
 * Adds v3 to v2 and puts the result into v0.
 */
static int op_add_int(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    int x = 0, y = 0 , z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    reg_idx_vz = ptr[*pc + 3];

    if (is_verbose())
        printf("add-int v%d, v%d, v%d\n", reg_idx_vx, reg_idx_vy,
               reg_idx_vz);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    load_reg_to(vm, reg_idx_vz, (unsigned char *) &z);
    x = y + z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);
    *pc = *pc + 4;
    return 0;

}

/* 0x91 sub-int vx,vy,vz
 * Calculates vy-vz and puts the result into vx.
 * 9100 0203 - sub-int v0, v2, v3
 * Subtracts v3 from v2 and puts the result into v0.
 */
static int op_sub_int(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    int x = 0, y = 0 , z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    reg_idx_vz = ptr[*pc + 3];

    if (is_verbose())
        printf("sub-int v%d, v%d, v%d\n", reg_idx_vx, reg_idx_vz,
               reg_idx_vy);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    load_reg_to(vm, reg_idx_vz, (unsigned char *) &z);
    x = y - z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);
    *pc = *pc + 4;
    return 0;
}

/* 0x92 mul-int vx, vy, vz
 * Multiplies vz with wy and puts the result int vx.
 * 9200 0203 - mul-int v0,v2,v3
 * Multiplies v2 with w3 and puts the result into v0
 */
static int op_mul_int(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    int x = 0, y = 0 , z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    reg_idx_vz = ptr[*pc + 3];

    if (is_verbose())
        printf("add-int v%d, v%d, v%d\n", reg_idx_vx, reg_idx_vy, reg_idx_vz);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    load_reg_to(vm, reg_idx_vz, (unsigned char *) &z);
    x = y * z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);
    *pc = *pc + 4;
    return 0;

}

/* 0x93 div-int vx,vy,vz
 * Divides vy with vz and puts the result into vx.
 * 9303 0001 - div-int v3, v0, v1
 * Divides v0 with v1 and puts the result into v3.
 */
static int op_div_int(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    int x = 0, y = 0 , z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    reg_idx_vz = ptr[*pc + 3];

    if (is_verbose())
        printf("add-int v%d, v%d, v%d\n", reg_idx_vx, reg_idx_vy, reg_idx_vz);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    load_reg_to(vm, reg_idx_vz, (unsigned char *) &z);
    x = y % z;
    x = (y - x) / z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);
    *pc = *pc + 4;
    return 0;

}

/* 0x83 int-to-double vx, vy
 * Converts the int value in vy into an double value in vx,vx+1.
 * 8340  - int-to-double v0, v4
 * Converts the int value in v4 into an double value in v0,v1.
 */
static int op_int_to_double(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    double d = 0;
    unsigned char *ptr_d = (unsigned char *) &d;
    int i = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vz = reg_idx_vx + 1;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;

    load_reg_to(vm, reg_idx_vy, (unsigned char *) &i);

    d = (double)i;
    if (is_verbose()) {
        printf("int-to-double v%d, v%d\n", reg_idx_vx, reg_idx_vy);
        printf("(%d) to (%f) \n", i , d);
    }

    store_double_to_reg(vm, reg_idx_vx , ptr_d + 4);
    store_double_to_reg(vm, reg_idx_vz , ptr_d);
    *pc = *pc + 2;
    return 0;
}

/* 0x8A double-to-int vx, vy
 * Converts the double value in vy,vy+1 into an integer value in vx.
 * 8A40  - double-to-int v0, v4
 * Converts the double value in v4,v5 into an integer value in v0.
 */
static int op_double_to_int(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    double d = 0;
    unsigned char *ptr_d = (unsigned char *) &d;
    int i = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;
    reg_idx_vz = reg_idx_vy + 1;

    load_reg_to_double(vm, reg_idx_vy , ptr_d + 4);
    load_reg_to_double(vm, reg_idx_vz , ptr_d);

    i = (int)d;
    if (is_verbose()) {
        printf("double-to-int v%d, v%d\n", reg_idx_vx, reg_idx_vy);
        printf("(%f) to (%d) \n", d , i);
    }

    store_to_reg(vm, reg_idx_vx, (unsigned char *) &i);
    *pc = *pc + 2;
    return 0;
}

/* 0xb0 add-int/2addr vx,vy
 * Adds vy to vx.
 * B010 - add-int/2addr v0,v1 Adds v1 to v0.
 */
static int op_add_int_2addr(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0;
    reg_idx_vx = ptr[*pc + 1] & 0x0F ;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F ;
    if (is_verbose())
        printf("add-int/2addr v%d, v%d\n", reg_idx_vx, reg_idx_vy);
    load_reg_to(vm, reg_idx_vx, (unsigned char *) &x);
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = x + y;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 2;
    return 0;
}

/* 0xcb , add-double/2addr
 * Adds vy to vx.
 * CB70 - add-double/2addr v0, v7
 * Adds v7 to v0.
 */
static int op_add_double_2addr(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    double x = 0.0, y = 0.0;
    unsigned char *ptr_x = (unsigned char *) &x;
    unsigned char *ptr_y = (unsigned char *) &y;
    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = (ptr[*pc + 1] >> 4) & 0x0F;

    load_reg_to_double(vm, reg_idx_vx, ptr_x + 4);
    load_reg_to_double(vm, reg_idx_vx + 1, ptr_x);
    load_reg_to_double(vm, reg_idx_vy, ptr_y + 4);
    load_reg_to_double(vm, reg_idx_vy + 1, ptr_y);


    if (is_verbose()) {
        printf("add-double/2addr v%d, v%d\n", reg_idx_vx, reg_idx_vy);
        printf("%f(%llx) + %f(%llx) = %f\n", x, x, y, y , y + x);
    }
    x = x + y;
    store_double_to_reg(vm, reg_idx_vx, ptr_x + 4);
    store_double_to_reg(vm, reg_idx_vx + 1, ptr_x);
    *pc = *pc + 2;
    return 0;
}

/* 0xcd , mul-double/2addr
 * Multiplies vx with vy
 * CD20 - mul-double/2addr v0, v2
 * Multiplies the double value in v0,v1 with the
 * double value in v2,v3 and puts the result into v0,v1.
 */
static int op_mul_double_2addr(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int reg_idx_vz = 0;
    int reg_idx_vw = 0;
    double x = 0.0, y = 0.0;
    unsigned char *ptr_x = (unsigned char *) &x;
    unsigned char *ptr_y = (unsigned char *) &y;

    reg_idx_vx = ptr[*pc + 1] & 0x0F;
    reg_idx_vy = reg_idx_vx + 1;
    reg_idx_vz = (ptr[*pc + 1] >> 4) & 0x0F;
    reg_idx_vw = reg_idx_vz + 1 ;

    load_reg_to_double(vm, reg_idx_vx, ptr_x + 4);
    load_reg_to_double(vm, reg_idx_vy, ptr_x);

    load_reg_to_double(vm, reg_idx_vz, ptr_y + 4);
    load_reg_to_double(vm, reg_idx_vw, ptr_y);

    if (is_verbose()) {
        printf("mul-double/2addr v%d, v%d\n", reg_idx_vx, reg_idx_vz);
        printf(" %f * %f = %f\n", x, y, x * y);
    }

    x = x * y;

    store_double_to_reg(vm, reg_idx_vx, ptr_x + 4);
    store_double_to_reg(vm, reg_idx_vy, ptr_x);

    load_reg_to_double(vm, reg_idx_vx, ptr_y + 4);
    load_reg_to_double(vm, reg_idx_vy, ptr_y);

    *pc = *pc + 2;
    return 0;
}

/* 0xd8 add-int/lit8 vx,vy,lit8
 * Calculates vy+lit8 and stores the result into vx.
 * D800 0203 - add-int/lit8 v0,v2, #int3
 * Calculates v2+3 and stores the result into v0.
 */
static int op_add_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("add-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = y + z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

/* 0xd9 rsub-int/lit8 vx,vy,lit8
 * Calculates lit8-vy and stores the result into vx.
 * D900 0203 - rsub-int/lit8 v0,v2, #int3
 * Calculates 3-v2 and stores the result into v0.
 */
static int op_rsub_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("rsub-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = z - y;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

/* 0xda mul-int/lit8 vx,vy,lit8
 * Calculates vy*lit8 and stores the result into vx.
 * DA00 0203 - mul-int/lit8 v0,v2, #int3
 * Calculates v2*3 and stores the result into v0.
 */
static int op_mul_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("mul-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = y * z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

/* 0xdb div-int/lit8 vx,vy,lit8
 * Calculates vy/lit8 and stores the result into vx.
 * DB00 0203 - div-int/lit8 v0,v2, #int3
 * Calculates v2/3 and stores the result into v0.
 */
static int op_div_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("div-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    /* x = y + z */
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = y % z;
    x = (y - x) / z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

/* 0xdc rem-int/lit8 vx,vy,lit8
 * Calculates vy%lit8 and stores the result into vx.
 * DC00 0203 - rem-int/lit8 v0,v2, #int3
 * Calculates v2%3 and stores the result into v0.
 */
static int op_rem_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("rem-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = y % z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

/* 0xdd and-int/lit8 vx,vy,lit8
 * Calculates vy&lit8 and stores the result into vx.
 * DC00 0203 - rem-int/lit8 v0,v2, #int3
 * Calculates v2&3 and stores the result into v0.
 */
static int op_and_int_lit8(DexFileFormat *dex, simple_dalvik_vm *vm, u1 *ptr, int *pc)
{
    int reg_idx_vx = 0;
    int reg_idx_vy = 0;
    int x = 0, y = 0 ;
    int z = 0;
    reg_idx_vx = ptr[*pc + 1];
    reg_idx_vy = ptr[*pc + 2];
    z = ptr[*pc + 3];

    if (is_verbose())
        printf("and-int/lit8 v%d, v%d, #int%d\n", reg_idx_vx, reg_idx_vy, z);
    load_reg_to(vm, reg_idx_vy, (unsigned char *) &y);
    x = y & z;
    store_to_reg(vm, reg_idx_vx, (unsigned char *) &x);

    *pc = *pc + 4;
    return 0;
}

static byteCode byteCodes[] = {
    { "move"		  , 0x01, 2,  op_move },
    { "move-result"		  , 0x0A, 2,  op_move_result },
    { "move-result-wide"  , 0x0B, 2,  op_move_result_wide },
    { "move-result-object", 0x0C, 2,  op_move_result_object },
    { "return-void"       , 0x0e, 2,  op_return_void },
    { "return"			  , 0x0f, 2,  op_return },
    { "return-wide"		  , 0x10, 2,  op_return_wide },
    { "return-object"	  , 0x11, 2,  op_return_object },
    { "const/4"           , 0x12, 2,  op_const_4 },
    { "const/16"          , 0x13, 4,  op_const_16 },
    { "const"			  , 0x14, 6,  op_const },
    { "const-wide/16"	  , 0x16, 4,  op_const_wide_16 },
    { "const-wide/32"	  , 0x17, 6,  op_const_wide_32 },
    { "const-wide/high16" , 0x19, 4,  op_const_wide_high16 },
    { "const-string"      , 0x1a, 4,  op_const_string },
    { "const-class"       , 0x1c, 4,  op_const_class },
    { "new-instance"      , 0x22, 4,  op_new_instance },
    { "new-array"         , 0x23, 4,  op_new_array },
    { "filled-new-array"  , 0x24, 6,  op_filled_new_array },
    { "goto"			  , 0x28, 2,  op_goto },
    { "goto/16"			  , 0x29, 2,  op_goto_16 },
    { "goto/32"			  , 0x2a, 2,  op_goto_32 },
    { "packed-switch"	  , 0x2b, 3,  op_packed_switch },
    { "if-eq"			  , 0x32, 4,  op_if_eq },
    { "if-ne"			  , 0x33, 4,  op_if_ne },
    { "if-lt"			  , 0x34, 4,  op_if_lt },
    { "if-ge"			  , 0x35, 4,  op_if_ge },
    { "if-gt"			  , 0x36, 4,  op_if_gt },
    { "if-le"			  , 0x37, 4,  op_if_le },
    { "if-eqz"			  , 0x38, 4,  op_if_eqz },
    { "if-nez"			  , 0x39, 4,  op_if_nez },
    { "if-ltz"			  , 0x3a, 4,  op_if_ltz },
    { "if-gez"			  , 0x3b, 4,  op_if_gez },
    { "if-gtz"			  , 0x3c, 4,  op_if_gtz },
    { "if-lez"			  , 0x3d, 4,  op_if_lez },
    { "aget"              , 0x44, 4,  op_aget },
    { "aget-wide"         , 0x45, 4,  op_aget_wide },
    { "aget-object"       , 0x46, 4,  op_aget_object },
    { "aget-boolean"      , 0x47, 4,  op_aget_boolean },
    { "aget-byte"         , 0x48, 4,  op_aget_byte },
    { "aget-char"         , 0x49, 4,  op_aget_char },
    { "aget-short"        , 0x4a, 4,  op_aget_short },
    { "aput"              , 0x4b, 4,  op_aput },
    { "aput-wide"         , 0x4c, 4,  op_aput_wide },
    { "aput-object"       , 0x4d, 4,  op_aput_object },
    { "aput-boolean"      , 0x4e, 4,  op_aput_boolean },
    { "aput-byte"         , 0x4f, 4,  op_aput_byte },
    { "aput-char"         , 0x50, 4,  op_aput_char },
    { "aput-short"        , 0x51, 4,  op_aput_short },
    { "iget"              , 0x52, 2,  op_iget },
    { "iget-wide"         , 0x53, 2,  op_iget_wide },
	{ "iget-object"       , 0x54, 2,  op_iget_object },
	{ "iget-boolean"      , 0x55, 2,  op_iget_boolean },
	{ "iget-byte"         , 0x56, 2,  op_iget_byte },
	{ "iget-char"         , 0x57, 2,  op_iget_char },
	{ "iget-short"        , 0x58, 2,  op_iget_short },
    { "iput"              , 0x59, 2,  op_iput },
    { "iput-wide"         , 0x5a, 2,  op_iput_wide },
    { "iput-object"       , 0x5b, 2,  op_iput_object },
    { "iput-boolean"      , 0x5c, 2,  op_iput_boolean },
    { "iput-byte"         , 0x5d, 2,  op_iput_byte },
    { "iput-char"         , 0x5e, 2,  op_iput_char },
    { "iput-short"        , 0x5f, 2,  op_iput_short },
    { "sget"              , 0x60, 4,  op_sget },
    { "sget-wide"         , 0x61, 4,  op_sget_wide },
    { "sget-object"       , 0x62, 4,  op_sget_object },
    { "sget-boolean"      , 0x63, 4,  op_sget_boolean },
    { "sget-byte"         , 0x64, 4,  op_sget_byte },
    { "sget-char"         , 0x65, 4,  op_sget_char },
    { "sget-short"        , 0x66, 4,  op_sget_short },
    { "sput"              , 0x67, 4,  op_sput },
    { "sput-wide"         , 0x68, 4,  op_sput_wide },
    { "sput-object"       , 0x69, 4,  op_sput_object },
    { "sput-boolean"      , 0x6a, 4,  op_sput_boolean },
    { "sput-byte"         , 0x6b, 4,  op_sput_byte },
    { "sput-char"         , 0x6c, 4,  op_sput_char },
    { "sput-short"        , 0x6d, 4,  op_sput_short },
    { "invoke-virtual"    , 0x6e, 6,  op_invoke_virtual },
    { "invoke-direct"     , 0x70, 6,  op_invoke_direct },
    { "invoke-static"     , 0x71, 6,  op_invoke_static },
    { "int-to-double"     , 0x83, 2,  op_int_to_double},
    { "double-to-int"     , 0x8a, 2,  op_double_to_int},
    { "add-int"           , 0x90, 4,  op_add_int },
    { "sub-int"           , 0x91, 4,  op_sub_int },
    { "mul-int"           , 0x92, 4,  op_mul_int },
    { "div-int"           , 0x93, 4,  op_div_int },
    { "add-int/2addr"     , 0xb0, 2,  op_add_int_2addr},
    { "add-double/2addr"  , 0xcb, 2,  op_add_double_2addr},
    { "mul-double/2addr"  , 0xcd, 2,  op_mul_double_2addr},
    { "add-int/lit8"      , 0xd8, 4,  op_add_int_lit8 },
    { "rsub-int/lit8"     , 0xd9, 4,  op_rsub_int_lit8 },
    { "mul-int/lit8"      , 0xda, 4,  op_mul_int_lit8 },
    { "div-int/lit8"      , 0xdb, 4,  op_div_int_lit8 },
    { "rem-int/lit8"      , 0xdc, 4,  op_rem_int_lit8 },
    { "and-int/lit8"      , 0xdd, 4,  op_and_int_lit8 },
};
static int byteCode_size = sizeof(byteCodes) / sizeof(byteCode);

static opCodeFunc findOpCodeFunc(unsigned char op)
{
    int i = 0;
    for (i = 0; i < byteCode_size; i++)
        if (op == byteCodes[i].opCode)
            return byteCodes[i].func;
    return 0;
}

void stack_push(simple_dalvik_vm *vm, u4 data)
{
	u1 *sp = vm->sp - 4;
	u1 *ptr = (u1 *)&data;

	memcpy(sp, ptr, 4);
	vm->sp = sp;
}

u4 stack_pop(simple_dalvik_vm *vm)
{
	u1 *sp = vm->sp;
	u4 value;

	memcpy(&value, sp, 4);
	vm->sp += 4;

	return value;
}

int new_invoke_frame(DexFileFormat *dex, simple_dalvik_vm *vm, encoded_method *m)
{
	int ins_size = m->code_item.ins_size;
	int reg_size = m->code_item.registers_size;
	int idx;

	/* step 1: Push pc & fp */
	stack_push(vm, vm->pc);
	stack_push(vm, (u4)vm->fp);

	/* step 2: Push caller registers */
	for (idx = 0; idx < reg_size; idx++)
	{
		u4 value;

		load_reg_to(vm, idx, (u1 *)&value);
		stack_push(vm, value);
	}
	stack_push(vm, reg_size);

	/* step 3: Update current fp */
	vm->fp = vm->sp;

	return 0;
}

void runMethod(DexFileFormat *dex, simple_dalvik_vm *vm, encoded_method *m)
{
    u1 *ptr = (u1 *) m->code_item.insns;
    unsigned char opCode = 0;
    opCodeFunc func = 0;

    while (1) {
        if (vm->returned || vm->pc >= m->code_item.insns_size * sizeof(ushort)) {
			vm->returned = 0;
            break;
		}
        opCode = ptr[vm->pc];
        func = findOpCodeFunc(opCode);
        if (func != 0) {
            if (func(dex, vm, ptr, &vm->pc))
	        break;
        } else {
            printRegs(vm);
            printf("Unknow OpCode =%02x \n", opCode);
            break;
        }
    }
}

void runMainMethod(DexFileFormat *dex, simple_dalvik_vm *vm, encoded_method *m)
{
    vm->pc = 0;
    runMethod(dex, vm, m);
}

encoded_method *find_vmethod(DexFileFormat *dex, instance_obj *ins_obj, int class_idx, int method_name_idx)
{
	int i, j;
	encoded_method *found = NULL;
	vtable_item *vtable = ins_obj->cls->vtable;
	int vtable_size = ins_obj->cls->vtable_size;
	char *name = get_string_data(dex, method_name_idx);

	for (i = 0; i < vtable_size; i++)
	{
		if (!strcmp(vtable[i].name, name))
		{
			found = vtable[i].method;
			break;
		}
	}

	/*
	for (i = 0; i < dex->header.classDefsSize; i++)
	{
		if (dex->class_def_item[i].class_idx == class_idx)
		{
			class_data_item *item = &dex->class_data_item[i];
			int methods_size = item->virtual_methods_size;
			int aggregated_idx = 0;

			for (j = 0; j < methods_size; j++)
			{
				encoded_method *tmp = &item->virtual_methods[j];
				aggregated_idx += tmp->method_idx_diff;
				method_id_item *item = &dex->method_id_item[aggregated_idx];

				if (item->name_idx == method_name_idx)
				{
					found = tmp;
					break;
				}
			}

			if (found)
				break;
		}
	}
	*/

	return found;
}

encoded_method *find_method_by_name(DexFileFormat *dex, int class_idx, const char *name)
{
	int i, j;
	encoded_method *found = NULL;

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
					found = tmp;
					break;
				}
			}

			if (found)
				break;
		}
	}

	return found;
}

encoded_method *find_method(DexFileFormat *dex, int class_idx, int method_name_idx)
{
	int i, j;
	encoded_method *found = NULL;

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

				if (item->name_idx == method_name_idx)
				{
					found = tmp;
					break;
				}
			}

			if (found)
				break;
		}
	}

	return found;
}

void simple_dvm_startup(DexFileFormat *dex, simple_dalvik_vm *vm, char *entry)
{
    int i = 0;
    int method_name_idx = -1;
    int method_idx = -1;
    int class_idx = -1;

    method_name_idx = find_const_string(dex, entry);

    if (method_name_idx < 0) {
        printf("no method %s in dex\n", entry);
        return;
    }
    for (i = 0 ; i < dex->header.methodIdsSize; i++)
        if (dex->method_id_item[i].name_idx == method_name_idx) {
            class_idx = dex->method_id_item[i].class_idx;
            method_idx = i;
            if (is_verbose() > 2) {
		type_id_item *item = get_type_item(dex, class_idx);
		char *type_name = get_string_data(dex, item->descriptor_idx);
                printf("find %s in class_idx[%d](%s), method_id = %d\n",
                       entry, class_idx, type_name, method_idx);
            }
            break;
        }
    if (class_idx < 0 || method_idx < 0) {
        printf("no method %s in dex\n", entry);
        return;
    }

//    encoded_method *m =
//        &dex->class_data_item[class_idx].direct_methods[method_idx];
    encoded_method *m = find_method(dex, class_idx, method_name_idx);
    if (!m)
    {
        printf("No method found\n");
        return;
    }

    if (is_verbose() > 2)
        printf("encoded_method method_id = %d, insns_size = %d\n",
               m->method_idx_diff, m->code_item.insns_size);

    memset(vm , 0, sizeof(simple_dalvik_vm));
	hash_init(&vm->root_set);
    vm->sp = vm->heap + sizeof(vm->heap);
    vm->fp = vm->sp;

    runMethod(dex, vm, m);
}

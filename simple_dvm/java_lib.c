/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#define _POSIX_C_SOURCE 199309L
#include "java_lib.h"
#include <time.h>

typedef struct _String {
	int buf_size;
    char *buf;
} String;

typedef struct _Long {
	long long val;
} Long;

int java_lang_math_random(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    double r = 0.0f;
    double test = 0.0f;
    int i = 0;
    int times = 0;
    srand(time(0));
    times = rand() % 100;
    for (i = 0; i < times; i++)
        r = ((double) rand() / (double) RAND_MAX);

    if (is_verbose() > 3) printf("get random number = %f \n", r);
    store_double_to_result(vm, (unsigned char *) &r);
    load_result_to_double(vm, (unsigned char *) &test);

    return 0;
}

/* java.io.InputStreamReader.<init> */
int java_io_inputstreamreader_init(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    if (is_verbose())
        printf("call java.io.InputStreamReader.<init>\n");
    return 0;
}

/* java.io.BufferedReader */
static char read_buf[2048];

/* java.io.BufferedReader.<init> */
int java_io_bufferedreader_init(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    if (is_verbose())
        printf("call java.io.BufferedReader.<init>\n");
	memset(read_buf, 0, sizeof(read_buf));
    return 0;
}

/* java.io.BufferedReader.readLine */
int java_io_bufferedreader_readline(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
	String *s;
    if (is_verbose())
        printf("call java.io.BufferedReader.readLine\n");
	fgets(read_buf, sizeof(read_buf), stdin);

	s = malloc(sizeof(String) + strlen(read_buf) + 1);
	if (!s)
		return -1;
	s->buf_size = strlen(read_buf) + 1;
	s->buf = (char *) s + sizeof(String);
	strcpy(s->buf, read_buf);
	// remove the newline character
	strtok(s->buf, "\n");

	store_to_bottom_half_result(vm, (unsigned char *) &s);

    return 0;
}

/* java.io.PrintStream.println */
static char buf[1024];
static int buf_ptr = 0;
static int use_buf = 0;
int java_io_print_stream_println(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
    int i = 0;
    int string_id = 0;
    if (is_verbose())
        printf("call java.io.PrintStream.println\n");

    load_reg_to(vm, p->reg_idx[1], (unsigned char *) &string_id);
    if (use_buf == 1) {
        printf("%s\n", buf);
        use_buf = 0;
        memset(buf, 0, 1024);
        buf_ptr = 0;
    } else {
        printf("%s\n", get_string_data(dex, string_id));
    }
    return 0;
}

int java_lang_long_valueof(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
	String *s;
	Long *l;
    if (is_verbose())
        printf("call java.lang.Long.valueOf\n");

    load_reg_to(vm, p->reg_idx[0], (unsigned char *) &s);
	l = malloc(sizeof(Long));
	l->val = atoll(s->buf);

	store_to_bottom_half_result(vm, (unsigned char *) &l);

    return 0;
}

int java_lang_long_longvalue(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
	Long *l;
    if (is_verbose())
        printf("call java.lang.Long.longValue\n");

    load_reg_to(vm, p->reg_idx[0], (unsigned char *) &l);

	store_double_to_result(vm, (unsigned char *) &l->val);

    return 0;
}

/* java.lang.StringBuilder.<init> */
int java_lang_string_builder_init(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
    if (is_verbose())
        printf("call java.lang.StringBuilder.<init>\n");
    memset(buf, 0, 1024);
    buf_ptr = 0;
    return 0;
}

int java_lang_string_builder_append(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
    int string_id = 0;
    if (is_verbose())
        printf("call java.lang.StringBuilder.append\n");
    load_reg_to(vm, p->reg_idx[1], (unsigned char *) &string_id);
    if (type != 0) {
        if (strcmp(type, "Ljava/lang/String;") == 0) {
            buf_ptr += snprintf(buf + buf_ptr, 1024, "%s", get_string_data(dex, string_id));
        } else if (strcmp(type, "I") == 0) {
            buf_ptr += snprintf(buf + buf_ptr, 1024, "%d", string_id);
        }
    }
    return 0;
}

int java_lang_string_builder_to_string(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
    if (is_verbose())
        printf("call java.lang.StringBuilder.toString\n");
    use_buf = 1;
    return 0;
}

array_obj *array_create_multi_dimension(DexFileFormat *dex, simple_dalvik_vm *vm, array_obj *dim, int dimension)
{
	array_obj *arr_obj;
	int size = (int)dim->ptr[dimension];
	int i;

	arr_obj = (array_obj *)malloc(sizeof(array_obj) + (size - 1) * sizeof(void *));
	if (!arr_obj)
	{
		printf("[%s] malloc fail\n", __FUNCTION__);
		return NULL;
	}

	arr_obj->size = size;

	if (dimension < dim->size - 1)
	{
		for (i = 0; i < size; i++)
			arr_obj->ptr[i] = array_create_multi_dimension(dex, vm, dim, dimension + 1);
	}

	return arr_obj;
}

void gen_array_class_name(char *buf, int size, char *base_name, int dimension)
{
	int i;

	memset(buf, 0, size);
	for (i = 0; i < dimension; i++)
		strcat(buf, "[");

	strcat(buf, base_name);
}

class_obj *find_class_obj(simple_dalvik_vm *vm, char *name);
int java_lang_reflect_array_new_instance(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
    invoke_parameters *p = &vm->p;
    int idx_vx;
    int idx_vy;
    class_obj *cls_obj;
    instance_obj *dim_arr_ins_obj;
    array_obj *dim_arr_obj;
    array_obj *result_arr;
    class_obj *result_cls_obj;
    instance_obj *result_ins_obj;
    char class_name[32];
    int i;

    if (is_verbose())
        printf("call java.lang.reflect.Array.newInstance\n");

    idx_vx = p->reg_idx[0];
    idx_vy = p->reg_idx[1];

    load_reg_to(vm, idx_vx, (unsigned char *) &cls_obj);
    load_reg_to(vm, idx_vy, (unsigned char *) &dim_arr_ins_obj);
    dim_arr_obj = (array_obj *)dim_arr_ins_obj->priv_data;

    result_arr = array_create_multi_dimension(dex, vm, dim_arr_obj, 0);
    if (!result_arr)
	    return -1;

    gen_array_class_name(class_name, sizeof(class_name), cls_obj->name, dim_arr_obj->size);
    if (is_verbose())
	    printf("Array class name: %s\n", class_name);

    result_cls_obj = find_class_obj(vm, class_name);
    if (!result_cls_obj)
    {
        result_cls_obj = (class_obj *)malloc(sizeof(class_obj));
        if (!result_cls_obj)
        {
            printf("[%s] class obj malloc fail\n", __FUNCTION__);
            return -1;
        }

        memset(result_cls_obj, 0, sizeof(class_obj));
        strncpy(result_cls_obj->name, class_name, strlen(class_name));
        list_init(&result_cls_obj->class_list);
        hash_add(&vm->root_set, &result_cls_obj->class_list, hash(result_cls_obj->name));
    }

    result_ins_obj = (instance_obj *)malloc(sizeof(instance_obj));
    if (!result_ins_obj)
    {
        printf("[%s] instance obj malloc fail\n", __FUNCTION__);

        return -1;
    }

    result_ins_obj->cls = result_cls_obj;
    result_ins_obj->priv_data = (void *)result_arr;

    store_to_bottom_half_result(vm, (unsigned char *)&result_ins_obj);
    if (is_verbose())
    {
        printf("Array class pointer: %p\n", result_ins_obj);
	dump_array_dimension(result_arr, dim_arr_obj->size);
    }

    return 0;
}

int java_lang_system_currenttimemillis(DexFileFormat *dex, simple_dalvik_vm *vm, char *type)
{
	long long millis; 
	struct timespec tp;

	clock_gettime(CLOCK_REALTIME, &tp);
	millis = tp.tv_sec * 1000 + tp.tv_nsec / 1000000;

	store_double_to_result(vm, (unsigned char *) &millis);

    return 0;
}

static java_lang_method method_table[] = {
    {"Ljava/lang/Math;",          "random",   java_lang_math_random},
    {"Ljava/io/PrintStream;",     "println",  java_io_print_stream_println},
    {"Ljava/io/InputStreamReader;", "<init>",   java_io_inputstreamreader_init},
    {"Ljava/io/BufferedReader;", "<init>",   java_io_bufferedreader_init},
    {"Ljava/io/BufferedReader;", "readLine",   java_io_bufferedreader_readline},
    {"Ljava/lang/Long;", "valueOf",   java_lang_long_valueof},
    {"Ljava/lang/Long;", "longValue",   java_lang_long_longvalue},
    {"Ljava/lang/StringBuilder;", "<init>",   java_lang_string_builder_init},
    {"Ljava/lang/StringBuilder;", "append",   java_lang_string_builder_append},
    {"Ljava/lang/StringBuilder;", "toString", java_lang_string_builder_to_string},
    {"Ljava/lang/reflect/Array;", "newInstance", java_lang_reflect_array_new_instance},
    {"Ljava/lang/System;", "currentTimeMillis", java_lang_system_currenttimemillis},
};

static int java_lang_method_size = sizeof(method_table) / sizeof(java_lang_method);

java_lang_method *find_java_lang_method(char *cls_name, char *method_name)
{
    int i = 0;
    for (i = 0; i < java_lang_method_size; i++)
        if (strcmp(cls_name, method_table[i].clzname) == 0 &&
            strcmp(method_name, method_table[i].methodname) == 0)
            return &method_table[i];
    return 0;
}

int invoke_java_lang_library(DexFileFormat *dex, simple_dalvik_vm *vm,
                             char *cls_name, char *method_name, char *type)
{
    java_lang_method *method = find_java_lang_method(cls_name, method_name);
    if (method != 0) {
        if (is_verbose())
            printf("invoke %s/%s %s\n", method->clzname, method->methodname, type);
        method->method_runtime(dex, vm, type);
        return 1;
    }
    return 0;
}

/*
 * Simple Dalvik Virtual Machine Implementation
 *
 * Copyright (C) 2013 Chun-Yu Wang <wicanr2@gmail.com>
 */

#ifndef SIMPLE_DVM_JAVA_LIBRARY_H
#define SIMPLE_DVM_JAVA_LIBRARY_H

#include "simple_dvm.h"

typedef struct _String {
	int buf_size;
    char *buf;
} String;

typedef struct _Long {
	long long val;
} Long;

typedef struct _StringBuilder {
	int buf_ptr;
    char buf[1024];
} StringBuilder;

typedef int (*java_lang_lib)(DexFileFormat *dex, simple_dalvik_vm *vm, char*type);

typedef struct _java_lang_method {
    char *clzname;
    char *methodname;
    java_lang_lib method_runtime;
} java_lang_method;

typedef struct _java_lang_clz {
    char *clzname;
    class_obj *clzobj;
} java_lang_clz;

int invoke_java_lang_library(DexFileFormat *dex, simple_dalvik_vm *vm,
                             char *cls_name, char *method_name, char *type); 
String* java_lang_string_const_string(DexFileFormat *dex, simple_dalvik_vm *vm, char *c_str, int len);
class_obj *find_java_class_obj(simple_dalvik_vm *vm, char *name);

#endif

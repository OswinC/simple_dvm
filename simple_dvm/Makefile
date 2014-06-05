PROJECT = dvm

# Toolchain configurations
CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc

SUFFIX ?=
EXECUTABLE = $(PROJECT)$(SUFFIX)

# Basic configurations
CFLAGS += -g -std=c99 -m32
LDFLAGS = -m32

# Optimizations
CFLAGS += -O0

# project starts here
CFLAGS += -I.
OBJS = \
	hash_table.o \
    bytecodes.o \
    java_lib.o \
    map_list_parser.o \
    type_ids_parser.o \
    class_def_parser.o \
    leb128.o \
    method_ids_parser.o \
    utils.o \
    dex_parser.o \
    string_ids_parser.o \
    main.o

$(EXECUTABLE): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLE)
	rm -f $(OBJS)
.PHONY: clean

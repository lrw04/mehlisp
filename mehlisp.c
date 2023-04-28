#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef long double num_t;

#define ERR_EXIT(...)                 \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        exit(1);                      \
    } while (0)

// garbage collector

#define INITIAL_MEMORY 4096

enum {
    T_NUMBER = 1,
    T_CONS,
    T_SYMBOL,
    T_PRIMITIVE,
    T_FUNCTION,
    T_MACRO,
    T_ENVIRONMENT,
};

struct value_t;

typedef struct gc_header_t {
    int type;
    size_t size;
    uint8_t *foward;
} gc_header_t;

typedef struct number_t {
    num_t value;
} number_t;

typedef struct cons_t {
    value_t *car, *cdr;
} cons_t;

typedef struct symbol_t {
    char name;
} symbol_t;

typedef struct primitive_t {
    value_t *(*func)(value_t *args, value_t *env);
} primitive_t;

typedef struct function_t {
    value_t *body, *env, *args;
} function_t;

typedef struct macro_t {
    value_t *body, *env, *args;
} macro_t;

typedef struct environment_t {
    value_t *variables;
    environment_t *parent;
} environment_t;

typedef struct value_t {
    gc_header_t header;
    union {
        number_t number;
        cons_t cons;
        symbol_t symbol;
        primitive_t primitive;
        function_t function;
        macro_t macro;
        environment_t environment;
    };
} value_t;

size_t memory_size;
uint8_t *from_space, *to_space, *alloc_ptr, *scan_ptr;

void gc_init() {
    memory_size = INITIAL_MEMORY;
    from_space = malloc(memory_size);
    if (!from_space) ERR_EXIT("memory allocation failed");
    alloc_ptr = from_space;
}

#define GC_ALIGN (sizeof(long))

uint8_t *gc_allocate(size_t size) {
    size = (size - 1) / GC_ALIGN + 1;
    if (size + alloc_ptr - from_space > memory_size) gc_collect(0);
    if (size + alloc_ptr - from_space > memory_size) gc_collect(1);
    uint8_t *o = alloc_ptr;
    alloc_ptr += size;
    return o;
}

void swap_u8p(uint8_t **a, uint8_t **b) {
    uint8_t *p;
    p = *a;
    *a = *b;
    *b = p;
}

void gc_collect(int expand) {
    if (expand) memory_size *= 2;
    to_space = malloc(memory_size);
    if (!to_space) ERR_EXIT("memory allocation failed");
    swap_u8p(&from_space, &to_space);
    alloc_ptr = scan_ptr = from_space;

    // TODO: copy root

    while (scan_ptr < alloc_ptr) {
        value_t *o = scan_ptr;
        switch (o->header.type) {
            case T_NUMBER:
            case T_PRIMITIVE:
            case T_SYMBOL:
                break;
            case T_CONS:
                o->cons.car = gc_copy(o->cons.car);
                o->cons.cdr = gc_copy(o->cons.cdr);
                break;
            case T_ENVIRONMENT:
                o->environment.variables = gc_copy(o->environment.variables);
                o->environment.parent = gc_copy(o->environment.parent);
                break;
            case T_FUNCTION:
                o->function.args = gc_copy(o->function.args);
                o->function.body = gc_copy(o->function.body);
                o->function.env = gc_copy(o->function.env);
                break;
            case T_MACRO:
                o->macro.args = gc_copy(o->macro.args);
                o->macro.body = gc_copy(o->macro.body);
                o->macro.env = gc_copy(o->macro.env);
                break;
            default:
                ERR_EXIT("encountered value of unknown type: %d",
                         o->header.type);
        }
        scan_ptr += o->header.size;
    }

    free(to_space);
}

uint8_t *gc_copy(uint8_t *o) {}

int main() {
    gc_init();
    return 0;
}

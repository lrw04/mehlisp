#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    uint8_t *forward;
} gc_header_t;

typedef struct number_t {
    num_t value;
} number_t;

typedef struct cons_t {
    value_t *car, *cdr;
} cons_t;

typedef struct symbol_t {
    char name[1];
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

value_t *obarray;

typedef struct root_stack_node_t {
    uint8_t *p;
    root_stack_node_t *next;
} root_stack_node_t;

root_stack_node_t *root_stack_head;

size_t memory_size;
uint8_t *from_space, *to_space, *alloc_ptr, *scan_ptr;

void gc_init() {
    memory_size = INITIAL_MEMORY;
    from_space = malloc(memory_size);
    if (!from_space) ERR_EXIT("out of memory");
    alloc_ptr = from_space;
    root_stack_head = malloc(sizeof(root_stack_node_t));
    root_stack_head->next = root_stack_head->p = NULL;
}

#define GC_ALIGN (sizeof(long))

uint8_t *gc_allocate(size_t size) {
    size = (size - 1) / GC_ALIGN + 1;
    size += offsetof(value_t, number);
    size = (size - 1) / GC_ALIGN + 1;
    if (size + alloc_ptr - from_space > memory_size) gc_collect(0);
    while (size + alloc_ptr - from_space > memory_size) gc_collect(1);
    uint8_t *o = alloc_ptr;
    alloc_ptr += size;
    ((value_t *)o)->header.forward = NULL;
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
    if (!to_space) ERR_EXIT("out of memory");
    swap_u8p(&from_space, &to_space);
    alloc_ptr = scan_ptr = from_space;

    gc_copy_roots();

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

uint8_t *gc_copy(uint8_t *o) {
    ptrdiff_t offset = o - from_space;
    if (offset < 0 || offset >= memory_size) return o;
    value_t *p = o;
    if (p->header.forward) return p->header.forward;
    value_t *next_loc = alloc_ptr;
    memcpy(next_loc, p, p->header.size);
    alloc_ptr += p->header.size;
    p->header.forward = next_loc;
    return next_loc;
}

void gc_copy_roots() {
    obarray = gc_copy(obarray);
    for (root_stack_node_t *u = root_stack_head; u; u = u->next) {
        if (u->p) u->p = gc_copy(u->p);
    }
}

void gc_push_root(uint8_t *p) {
    root_stack_node_t *u = malloc(sizeof(root_stack_node_t));
    u->next = root_stack_head;
    u->p = p;
    root_stack_head = u;
}

void gc_pop_root() {
    root_stack_node_t *u = root_stack_head;
    root_stack_head = u->next;
    free(u);
}

value_t *make_number(num_t num) {
    value_t *p = gc_allocate(sizeof(num_t));
    p->header.type = T_NUMBER;
    p->number.value = num;
    return p;
}

value_t *make_cons(value_t *car, value_t *cdr) {
    value_t *p = gc_allocate(sizeof(cons_t));
    p->header.type = T_CONS;
    p->cons.car = car;
    p->cons.cdr = cdr;
    return p;
}

value_t *make_symbol(const char *name) {
    value_t *p = gc_allocate(strlen(name) + 1);
    p->header.type = T_SYMBOL;
    strcpy(p->symbol.name, name);
    return p;
}

value_t *intern(const char *name) {
    for (value_t *u = obarray; u; u = u->cons.cdr) {
        value_t *v = u->cons.car;
        if (v->header.type != T_SYMBOL) ERR_EXIT("non-symbol in obarray");
        if (!strcmp(v->symbol.name, name)) {
            return v;
        }
    }
    obarray = make_cons(make_symbol(name), obarray);
    return obarray->cons.car;
}

value_t *make_primitive(value_t *(*func)(value_t *args, value_t *env)) {
    value_t *p = gc_allocate(sizeof func);
    p->header.type = T_PRIMITIVE;
    p->primitive.func = func;
    return p;
}

int main() {
    gc_init();
    return 0;
}

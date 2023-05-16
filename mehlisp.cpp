#include <cstdio>
#include <cstring>
#include <iostream>
#include <list>
#include <sstream>
#include <vector>

using namespace std;

#define ERR_EXIT(...)                 \
    do {                              \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(1);                      \
    } while (0)

struct trie {
    struct node {
        int ch[128];
        char path;
        node() {
            path = 0;
            for (int i = 0; i < 128; i++) ch[i] = -1;
        }
    };
    vector<node> nodes;
    trie() { nodes.emplace_back(); }
    int intern(const char *s, int n) {
        int u = 0;
        for (int i = 0; i < n; i++) {
            int &v = nodes[u].ch[(int)s[i]];
            if (v < 0) {
                v = nodes.size();
                nodes.emplace_back();
                nodes.back().path = s[i];
            }
            u = v;
        }
        return u;
    }
};

trie obarray;

enum type_t {
    TNUM,
    TSYM,
    TIPORT,
    TOPORT,
    TPRIM,
    TEOF,
    TUNBOUND,
    TPROC,
    TMACRO,
    TCONS,
    TENV,
};

struct ptr {
    type_t type;
    union {
        long double number;
        int symbol;
        istream *iport;
        ostream *oport;
        long long index;
        ptr (*primitive)(ptr &, ptr &);
        //               args env
    };
};

ptr intern(const char *s, int n) {
    ptr p;
    p.type = TSYM;
    p.symbol = obarray.intern(s, n);
    return p;
}

ptr intern(const char *s) { return intern(s, strlen(s)); }

vector<ptr> car, cdr;
vector<bool> mark;
list<long long> freel, allocl, rootl;

long long memory_size = 4096;

void gc_init() {
    car.resize(memory_size);
    cdr.resize(memory_size);
    mark.resize(memory_size);
    for (long long i = 0; i < memory_size; i++) freel.push_back(i);
}

void gc_mark(long long u) {
    if (mark[u]) return;
    mark[u] = true;
    if (car[u].type >= TPROC) gc_mark(car[u].index);
    if (cdr[u].type >= TPROC) gc_mark(cdr[u].index);
}

void gc_cycle() {
    for (auto p : allocl) mark[p] = false;
    for (auto p : rootl) gc_mark(p);
    for (auto it = allocl.begin(); it != allocl.end();) {
        if (mark[*it]) {
            it++;
        } else {
            freel.push_front(*it);
            allocl.erase(it);
        }
    }
}

long long gc_alloc() {
    if (freel.empty()) gc_cycle();
    if (freel.empty()) {
        for (auto i = memory_size; i < memory_size * 2; i++) freel.push_back(i);
        car.resize(memory_size * 2);
        cdr.resize(memory_size * 2);
        memory_size *= 2;
    }
    auto p = freel.front();
    freel.pop_front();
    allocl.push_back(p);
    return p;
}

ptr cons(const ptr &ccar, const ptr &ccdr, type_t type = TCONS) {
    ptr p;
    p.type = type;
    p.index = gc_alloc();
    car[p.index] = ccar;
    cdr[p.index] = ccdr;
    return p;
}

void push(ptr &lst, const ptr &ccar) { lst = cons(ccar, lst); }

ptr gen_eof() {
    ptr p;
    p.type = TEOF;
    return p;
}

ptr read(ptr &);

ptr read_cdr(ptr &port) {
    int c = port.iport->get();
    while (isspace(c)) c = port.iport->get();
    if (c == '.') {
        ptr tmp = read(port);
        do {
            c = port.iport->get();
        } while (isspace(c));
        if (c != ')')
            ERR_EXIT("Read-cdr: expected )");
        else
            return tmp;
    } else if (c == ')') {
        return intern("nil");
    } else {
        auto ccar = read(port);
        auto ccdr = read_cdr(port);
        return cons(ccar, ccdr);
    }
}

ptr make_number(long double num) {
    ptr p;
    p.type = TNUM;
    p.number = num;
    return p;
}

ptr read(ptr &port) {
    if (port.type != TIPORT) ERR_EXIT("Read: not an input port");
    int c = port.iport->get();
    if (c == EOF) return gen_eof();
    while (isspace(c)) c = port.iport->get();

    if (c == '(') return read_cdr(port);
    if (c == '#') {
        c = port.iport->get();
        if (c == '\\') {
            c = port.iport->get();
            return make_number(c);
        } else if (c == '<') {
            ERR_EXIT("Read: unreadable object");
        }
    } else if (isdigit(c) || c == '+' || c == '-') {
        long double n;
        (*port.iport) >> n;
        return make_number(n);
    } else if (c == '.') {
        ERR_EXIT("Read: unexpected dot");
    } else {
        string s;
        (*port.iport) >> s;
        return intern(s.c_str());
    }
}

int main() { gc_init(); }

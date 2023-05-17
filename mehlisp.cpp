#include <algorithm>
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
        int ch[128], father;
        char path;
        node() {
            path = 0;
            for (int i = 0; i < 128; i++) ch[i] = -1;
            father = -1;
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
                nodes.back().father = u;
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
void print(const ptr &p, ptr &port);

ptr make_input_port(istream *st) {
    ptr p;
    p.type = TIPORT;
    p.iport = st;
    return p;
}

ptr make_output_port(ostream *st) {
    ptr p;
    p.type = TOPORT;
    p.oport = st;
    return p;
}

auto iport = make_input_port(&cin);
auto oport = make_output_port(&cout);

ptr read_cdr(ptr &port) {
    if (port.type != TIPORT) ERR_EXIT("Read-cdr: not an input port");
    int c = port.iport->get();
    while (isspace(c)) c = port.iport->get();
    if (c == EOF) ERR_EXIT("Read-cdr: unexpected EOF");
    if (c == '.') {
        cerr << "dot" << endl;
        ptr tmp = read(port);
        do {
            c = port.iport->get();
        } while (isspace(c));
        if (c != ')')
            ERR_EXIT("Read-cdr: expected )");
        else
            return tmp;
    } else if (c == ')') {
        cerr << "rparen" << endl;
        return intern("nil");
    } else {
        cerr << "item" << endl;
        port.iport->unget();
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

bool delimp(char c) { return isspace(c) || c == '(' || c == ')'; }

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
        ERR_EXIT("Read: unexpected object");
    } else if (isdigit(c) || c == '+' || c == '-') {
        port.iport->unget();
        long double n;
        (*port.iport) >> n;
        return make_number(n);
    } else if (c == '.') {
        ERR_EXIT("Read: unexpected dot");
    } else {
        port.iport->unget();
        string s;
        while (!delimp(c = port.iport->get())) {
            s += c;
        }
        port.iport->unget();
        return intern(s.c_str());
    }
}

void print(const ptr &p, ptr &port) {
    if (port.type != TOPORT) ERR_EXIT("Print: not an output port");
    if (p.type == TCONS) {
        (*port.oport) << "(";
        print(car[p.index], port);
        (*port.oport) << " . ";
        print(cdr[p.index], port);
        (*port.oport) << ")";
    } else if (p.type == TENV) {
        (*port.oport) << "#<environment>";
    } else if (p.type == TEOF) {
        (*port.oport) << "#eof";
    } else if (p.type == TIPORT) {
        (*port.oport) << "#<input port>";
    } else if (p.type == TOPORT) {
        (*port.oport) << "#<output port>";
    } else if (p.type == TMACRO) {
        (*port.oport) << "#<macro>";
    } else if (p.type == TPROC) {
        (*port.oport) << "#<procedure>";
    } else if (p.type == TPRIM) {
        (*port.oport) << "#<primitive>";
    } else if (p.type == TNUM) {
        (*port.oport) << p.number;
    } else if (p.type == TSYM) {
        string s;
        for (auto u = p.symbol; u >= 0; u = obarray.nodes[u].father) {
            if (obarray.nodes[u].path) s += obarray.nodes[u].path;
        }
        reverse(s.begin(), s.end());
        (*port.oport) << s;
    } else {
        ERR_EXIT("Print: unexpected object type");
    }
}

bool eq(ptr p, ptr q) {
    if (p.type == TCONS || p.type == TENV || p.type == TMACRO ||
        p.type == TPROC) {
        return q.type == p.type && p.index == q.index;
    } else if (p.type == TEOF) {
        return q.type == p.type;
    } else if (p.type == TIPORT) {
        return q.type == p.type && p.iport == q.iport;
    } else if (p.type == TOPORT) {
        return q.type == p.type && p.oport == q.oport;
    } else if (p.type == TPRIM) {
        return q.type == p.type && p.primitive == q.primitive;
    } else if (p.type == TNUM) {
        return q.type == p.type && p.number == q.number;
    } else if (p.type == TSYM) {
        return q.type == p.type && p.symbol == q.symbol;
    } else {
        ERR_EXIT("Print: unexpected object type");
    }
}

int main() {
    ios::sync_with_stdio(false);
    gc_init();
    while (true) {
        cout << "> " << flush;
        auto p = read(iport);
        if (eq(p, gen_eof())) break;
        print(p, oport);
        cout << endl;
    }
}

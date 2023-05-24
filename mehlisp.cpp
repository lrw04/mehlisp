#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <list>
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
        //               args   env
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
list<long long> freel, allocl;
list<ptr *> rootl;

long long memory_size = 16;

struct root_guard {
    explicit root_guard(ptr &p) { rootl.push_front(&p); }
    ~root_guard() { rootl.pop_front(); }
};

bool effective_cons_p(ptr p) {
    return p.type == TCONS || p.type == TENV || p.type == TMACRO ||
           p.type == TPROC;
}

ptr &get_car(ptr p) {
    if (!effective_cons_p(p)) ERR_EXIT("Get-car on non-cons");
    return car[p.index];
}

ptr &get_cdr(ptr p) {
    if (!effective_cons_p(p)) ERR_EXIT("Get-cdr on non-cons");
    return cdr[p.index];
}

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
    // auto prev = freel.size();
    for (auto p : allocl) mark[p] = false;
    for (auto p : rootl)
        if (effective_cons_p(*p)) gc_mark(p->index);
    for (auto it = allocl.begin(); it != allocl.end();) {
        if (mark[*it]) {
            it++;
        } else {
            freel.push_front(*it);
            it = allocl.erase(it);
        }
    }
    // cerr << "Freed " << freel.size() - prev << " cons cells" << endl;
    // for (auto i : freel) cerr << i << " ";
    // cerr << endl;
    // for (auto i : rootl)
    //     if (effective_cons_p(*i)) cerr << i->index << " ";
    // cerr << endl;
}

long long gc_alloc() {
    // for (auto i : freel) cerr << i << " ";
    // cerr << endl;
    // for (auto i : rootl)
    //     if (effective_cons_p(*i)) cerr << i->index << " ";
    // cerr << endl;
    gc_cycle();
    if (freel.empty()) gc_cycle();
    if (freel.empty()) {
        for (auto i = memory_size; i < memory_size * 2; i++) freel.push_back(i);
        memory_size *= 2;
        car.resize(memory_size);
        cdr.resize(memory_size);
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

ptr make_number(long double num) {
    ptr p;
    p.type = TNUM;
    p.number = num;
    return p;
}

ptr make_ptr() { return make_number(0); }

ptr read_cdr(ptr &port) {
    if (port.type != TIPORT) ERR_EXIT("Read-cdr: not an input port");
    int c = port.iport->get();
    while (isspace(c)) c = port.iport->get();
    if (c == EOF) ERR_EXIT("Read-cdr: unexpected EOF");
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
        port.iport->unget();
        ptr ccar = make_ptr(), ccdr = make_ptr();
        root_guard carg(ccar);
        ccar = read(port);
        root_guard cdrg(ccdr);
        ccdr = read_cdr(port);
        return cons(ccar, ccdr);
    }
}

bool delimp(char c) { return isspace(c) || c == '(' || c == ')'; }

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
    } else if (p.type == TUNBOUND) {
        return q.type == p.type;
    } else {
        ERR_EXIT("Eq: unexpected object type");
    }
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
        ERR_EXIT("Read: unexpected object");
    } else if (c == '.') {
        ERR_EXIT("Read: unexpected dot");
    } else if (c == '\'') {
        ptr quote = intern("quote"), text = make_ptr(), ccdr = make_ptr();
        root_guard g1(quote), g2(text), g3(ccdr);
        text = read(port);
        ccdr = cons(text, intern("nil"));
        return cons(quote, ccdr);
    } else {
        port.iport->unget();
        string s;
        while (!delimp(c = port.iport->get())) {
            s += c;
        }
        port.iport->unget();
        char *e;
        long double val = strtold(s.c_str(), &e);
        if (*e != '\0' || errno) return intern(s.c_str());
        return make_number(val);
    }
}

void print_cdr(const ptr &p, ptr &port) {
    if (p.type != TCONS) ERR_EXIT("Print-cdr: not a cons");
    print(car[p.index], port);
    if (cdr[p.index].type == TCONS) {
        (*port.oport) << " ";
        print_cdr(cdr[p.index], port);
    } else {
        if (eq(cdr[p.index], intern("nil"))) {
            (*port.oport) << ")";
            return;
        }
        (*port.oport) << " . ";
        print(cdr[p.index], port);
        (*port.oport) << ")";
    }
}

void print(const ptr &p, ptr &port) {
    if (port.type != TOPORT) ERR_EXIT("Print: not an output port");
    if (p.type == TCONS) {
        (*port.oport) << "(";
        print_cdr(p, port);
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

ptr make_unbound() {
    ptr p;
    p.type = TUNBOUND;
    return p;
}

ptr lookup(ptr &env, const ptr &sym) {
    if (eq(env, intern("nil"))) {
        return make_unbound();
    }
    if (env.type != TENV) ERR_EXIT("Lookup: not an environment");
    auto p = get_car(env);
    for (auto i = p; !eq(i, intern("nil")); i = get_cdr(i)) {
        auto c = get_car(i);
        if (eq(get_car(c), sym)) return c;
    }
    return lookup(get_cdr(env), sym);
}

ptr eval(ptr expr, ptr &env) {
eval_start:
    if (expr.type != TCONS) {
        if (expr.type == TSYM) {
            if (eq(expr, intern("nil")) || eq(expr, intern("t"))) return expr;
            auto p = lookup(env, expr);
            if (eq(p, make_unbound())) ERR_EXIT("Eval: unbound variable");
            return get_cdr(p);
        }
        return expr;
    }
    if (eq(get_car(expr), intern("quote"))) return get_car(get_cdr(expr));
    if (eq(get_car(expr), intern("if"))) {
        auto p = make_ptr();
        root_guard g(p);
        p = eval(get_car(get_cdr(expr)), env);
        if (eq(p, intern("nil"))) {
            // alternative or nil
            if (eq(get_cdr(get_cdr(expr)), intern("nil"))) return intern("nil");
            expr = get_car(get_cdr(get_cdr(get_cdr(expr))));
            goto eval_start;
        } else {
            expr = get_car(get_cdr(get_cdr(expr)));
            goto eval_start;
        }
    }
    ERR_EXIT("Eval: unknown expression type");
}

ptr initial_environment() { return cons(intern("nil"), intern("nil"), TENV); }

int main() {
    gc_init();
    ptr env = make_ptr();
    root_guard g(env);
    env = initial_environment();
    while (true) {
        cout << "> " << flush;
        ptr p = make_ptr(), q = make_ptr();
        root_guard g(p), h(q);
        p = read(iport);
        if (eq(p, gen_eof())) break;
        q = eval(p, env);
        print(q, oport);
        cout << endl;
    }
    cout << endl;
}

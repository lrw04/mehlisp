#include <sys/stat.h>

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

long long memory_size = 6;

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

ptr make_number(long double num) {
    ptr p;
    p.type = TNUM;
    p.number = num;
    return p;
}

ptr make_ptr() { return make_number(0); }

ptr cons(const ptr &ccar, const ptr &ccdr, type_t type = TCONS) {
    ptr p;
    p.type = type;
    p.index = gc_alloc();
    car[p.index] = ccar;
    cdr[p.index] = ccdr;
    return p;
}

void push(ptr &lst, ptr ccar) {
    auto p = make_ptr();
    root_guard g(p);
    p = cons(ccar, lst);
    lst = p;
}

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
auto eport = make_output_port(&cerr);

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
        return q.type == p.type && p.index == q.index;
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
    while (isspace(c)) c = port.iport->get();
    if (c == EOF) return gen_eof();

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
    } else if (p.type == TUNBOUND) {
        (*port.oport) << "#<unbound>";
    } else {
        ERR_EXIT("Print: unexpected object type: %d", p.type);
    }
}

ptr make_unbound() {
    ptr p;
    p.type = TUNBOUND;
    return p;
}

ptr lookup(ptr env, ptr sym) {
lookup_start:
    root_guard g1(env), g2(sym);
    if (eq(env, intern("nil"))) {
        return make_unbound();
    }
    if (env.type != TENV) ERR_EXIT("Lookup: not an environment");
    if (sym.type != TSYM) ERR_EXIT("Lookup: not a symbol");
    auto p = get_car(env);
    for (auto i = p; !eq(i, intern("nil")); i = get_cdr(i)) {
        auto c = get_car(i);
        if (eq(get_car(c), sym)) return c;
    }
    // try parent
    env = get_cdr(env);
    goto lookup_start;
}

void print_mem() {
    for (long long i = 0; i < memory_size; i++) {
        cerr << i << ": ";
        cerr << car[i].type << "-";
        if (effective_cons_p(car[i]))
            cerr << car[i].index;
        else if (car[i].type == TNUM)
            cerr << car[i].number;
        else if (car[i].type == TSYM)
            print(car[i], eport);
        cerr << " ";
        cerr << cdr[i].type << "-";
        if (effective_cons_p(cdr[i]))
            cerr << cdr[i].index;
        else if (cdr[i].type == TNUM)
            cerr << cdr[i].number;
        else if (cdr[i].type == TSYM)
            print(cdr[i], eport);
        cerr << endl;
    }
}

ptr make_procedure(ptr formals, ptr body, ptr env, type_t type = TPROC) {
    ptr p = make_ptr();
    root_guard g(p);
    p = cons(formals, body);
    return cons(env, p, type);
}

ptr procedure_formals(ptr f) { return get_car(get_cdr(f)); }
ptr procedure_body(ptr f) { return get_cdr(get_cdr(f)); }
ptr procedure_env(ptr f) { return get_car(f); }

ptr eval(ptr expr, ptr *env);

ptr evlis(ptr args, ptr &env) {
    if (eq(args, intern("nil"))) return intern("nil");
    ptr p = make_ptr(), q = make_ptr();
    root_guard g1(p), g2(q);
    p = eval(get_car(args), &env);
    q = evlis(get_cdr(args), env);
    return cons(p, q);
}

ptr make_frame(ptr formals, ptr args) {
    root_guard g1(formals), g2(args);
    if (eq(formals, intern("nil")) && (!eq(args, intern("nil"))))
        ERR_EXIT("Make-frame: too many arguments");
    if (eq(formals, intern("nil"))) return intern("nil");
    if (formals.type == TSYM) {
        auto p = make_ptr();
        root_guard g(p);
        p = cons(formals, args);
        return cons(p, intern("nil"));
    }
    if (formals.type != TCONS) ERR_EXIT("Make-frame: expected cons");
    if (get_car(formals).type != TSYM)
        ERR_EXIT("Make-frame: non-symbol on car of formals");
    auto p = make_ptr(), q = make_ptr();
    root_guard g3(p), g4(q);
    p = make_frame(get_cdr(formals), get_cdr(args));
    q = cons(get_car(formals), get_car(args));
    return cons(q, p);
}

ptr eval(ptr expr, ptr *env) {
eval_start:
    root_guard g1(expr), g2(*env);
    if (expr.type != TCONS) {
        if (expr.type == TSYM) {
            if (eq(expr, intern("nil")) || eq(expr, intern("t"))) return expr;
            auto p = lookup(*env, expr);
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
            if (eq(get_cdr(get_cdr(get_cdr(expr))), intern("nil")))
                return intern("nil");
            expr = get_car(get_cdr(get_cdr(get_cdr(expr))));
            goto eval_start;  // tail call to eval
        } else {
            expr = get_car(get_cdr(get_cdr(expr)));
            goto eval_start;
        }
    }
    if (eq(get_car(expr), intern("set!"))) {
        auto p = make_ptr();
        root_guard g(p);
        p = lookup(*env, get_car(get_cdr(expr)));
        if (eq(p, make_unbound())) {
            auto pair = make_ptr(), lst = make_ptr();
            root_guard g1(pair), g2(lst);
            pair = cons(get_car(get_cdr(expr)), make_unbound());
            lst = cons(pair, get_car(*env));
            get_car(*env) = lst;
        }
        p = lookup(*env, get_car(get_cdr(expr)));
        get_cdr(p) = eval(get_car(get_cdr(get_cdr(expr))), env);
        return get_car(get_cdr(expr));
    }
    if (eq(get_car(expr), intern("lambda"))) {
        return make_procedure(get_car(get_cdr(expr)), get_cdr(get_cdr(expr)),
                              *env, TPROC);
    }
    if (eq(get_car(expr), intern("syntax"))) {
        return make_procedure(get_car(get_cdr(expr)), get_cdr(get_cdr(expr)),
                              *env, TMACRO);
    }
    auto p = make_ptr(), args = make_ptr();
    root_guard gg1(p), gg2(args);
    p = eval(get_car(expr), env);
    if (p.type == TPROC) {
        args = evlis(get_cdr(expr), *env);
        // apply
        auto body = make_ptr(), frame = make_ptr(), newenv = make_ptr();
        root_guard g1(body), g2(frame), g3(newenv);
        body = procedure_body(p);
        frame = make_frame(procedure_formals(p), args);
        newenv = cons(frame, *env, TENV);
        if (eq(body, intern("nil"))) return intern("nil");
        while (!eq(get_cdr(body), intern("nil"))) {
            eval(get_car(body), &newenv);
            body = get_cdr(body);
        }
        // special handling for last clause in lambda
        expr = get_car(body);
        env = &newenv;
        goto eval_start;
    } else if (p.type == TPRIM) {
        args = evlis(get_cdr(expr), *env);
        // apply
        // TODO
    } else if (p.type == TMACRO) {
        args = get_cdr(expr);
        // apply and eval
        auto body = make_ptr(), frame = make_ptr(), newenv = make_ptr();
        root_guard g1(body), g2(frame), g3(newenv);
        body = procedure_body(p);
        frame = make_frame(procedure_formals(p), args);
        newenv = cons(frame, *env, TENV);
        if (eq(body, intern("nil"))) return intern("nil");
        while (!eq(get_cdr(body), intern("nil"))) {
            eval(get_car(body), &newenv);
            body = get_cdr(body);
        }
        // special handling for last clause in lambda
        expr = get_car(body);
        env = &newenv;
        expr = eval(expr, env);
        goto eval_start;
    }
    ERR_EXIT("Eval: unknown expression type");
}

ptr initial_environment() {
    return cons(intern("nil"), intern("nil"), TENV);
    // TODO
}

int main(int argc, char **argv) {
    gc_init();
    ptr env = make_ptr();
    root_guard g(env);
    env = initial_environment();
    populate_primitives(env);
    for (int i = 1; i < argc; i++) {
        bool filep = strcmp(argv[i], "-");
        ifstream st;
        if (filep) {
            st.open(argv[i]);
            iport = make_input_port(&st);
        } else {
            iport = make_input_port(&cin);
        }
    while (true) {
            if (!filep) cout << "> " << flush;
            ptr p = make_ptr(), q = make_ptr();
            root_guard g1(p), g2(q);
            p = read(iport);
            if (eq(p, gen_eof())) break;
            q = eval(p, &env);
            if (!filep) {
                print(q, oport);
                cout << endl;
            }
        }
        if (filep) {
            st.close();
        } else {
            cout << endl;
        }
    }
}
